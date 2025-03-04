//==============================================================================
// Includes
//==============================================================================

#include <stdbool.h>
#include <unistd.h>
#include <math.h>

#ifdef __linux__
#include <execinfo.h>
#include <signal.h>
#endif

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "swadge_esp32.h"
#include "swadgeMode.h"
#include "mode_main_menu.h"
#include "btn.h"

#include "list.h"

#include "emu_esp.h"
#include "sound.h"
#include "emu_display.h"
#include "emu_sound.h"
#include "emu_sensors.h"
#include "emu_main.h"

#include "fighter_menu.h"
#include "jumper_menu.h"
#include "mode_colorchord.h"
#include "mode_credits.h"
#include "mode_dance.h"
#include "mode_flight.h"
#include "mode_gamepad.h"
#include "mode_main_menu.h"
#include "mode_slide_whistle.h"
#include "mode_test.h"
#include "mode_tiltrads.h"
#include "mode_tunernome.h"
#include "mode_paint.h"
#include "paint_share.h"
#include "paint_share.h"
#include "picross_menu.h"
#include "mode_platformer.h"

//Make it so we don't need to include any other C files in our build.
#define CNFG_IMPLEMENTATION
#define CNFGOGL
#include "rawdraw_sf.h"

//==============================================================================
// Defines
//==============================================================================

#define MIN(x,y) ((x)<(y)?(x):(y))
#define MIN_LED_WIDTH 64

#define BG_COLOR  0x191919FF // This color isn't part of the palette
#define DIV_COLOR 0x808080FF

//==============================================================================
// Function prototypes
//==============================================================================

void drawBitmapPixel(uint32_t* bitmapDisplay, int w, int h, int x, int y, uint32_t col);
void plotRoundedCorners(uint32_t* bitmapDisplay, int w, int h, int r, uint32_t col);

#ifdef __linux__
void init_crashSignals(void);
void signalHandler_crash(int signum, siginfo_t* si, void* vcontext);
#endif

//==============================================================================
// Variables
//==============================================================================

static bool isRunning = true;

//==============================================================================
// Functions
//==============================================================================

/**
 * This function must be provided for rawdraw. Key events are received here
 *
 * @param keycode The key code, a lowercase ascii char
 * @param bDown true if the key was pressed, false if it was released
 */
void HandleKey( int keycode, int bDown )
{
    emuSensorHandleKey(keycode, bDown);
}

/**
 * @brief Handle mouse click events from rawdraw
 *
 * @param x The x coordinate of the mouse event
 * @param y The y coordinate of the mouse event
 * @param button The mouse button that was pressed or released
 * @param bDown true if the button was pressed, false if it was released
 */
void HandleButton( int x UNUSED, int y UNUSED, int button UNUSED, int bDown UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief Handle mouse motion events from rawdraw
 *
 * @param x The x coordinate of the mouse event
 * @param y The y coordinate of the mouse event
 * @param mask A mask of mouse buttons that are currently held down
 */
void HandleMotion( int x UNUSED, int y UNUSED, int mask UNUSED)
{
    WARN_UNIMPLEMENTED();
}

/**
 * @brief Free memory on exit
 */
void HandleDestroy()
{
    // Stop the main loop
    isRunning = false;

    // Cleanup swadge stuff
    cleanupOnExit();

    // Then free display memory
    deinitDisplayMemory();

    // Close sound
    deinitSound();
}

/**
 * @brief Helper function to draw to a bitmap display
 *
 * @param bitmapDisplay The display to draw to
 * @param w The width of the display
 * @param h The height of the display
 * @param x The X coordinate to draw a pixel at
 * @param y The Y coordinate to draw a pixel at
 * @param col The color to draw the pixel
 */
void drawBitmapPixel(uint32_t* bitmapDisplay, int w, int h, int x, int y, uint32_t col)
{
    if((y * w) + x < (w * h))
    {
        bitmapDisplay[(y * w) + x] = col;
    }
}

/**
 * @brief Helper functions to draw rounded corners to the display
 *
 * @param bitmapDisplay The display to round the corners on
 * @param w The width of the display
 * @param h The height of the display
 * @param r The radius of the rounded corners
 * @param col The color to draw the rounded corners
 */
void plotRoundedCorners(uint32_t* bitmapDisplay, int w, int h, int r, uint32_t col)
{
    int or = r;
    int x = -r, y = 0, err = 2 - 2 * r; /* bottom left to top right */
    do
    {
        for(int xLine = 0; xLine <= (or + x); xLine++)
        {
            drawBitmapPixel(bitmapDisplay, w, h,     xLine    , h - (or - y) - 1, col); /* I.   Quadrant -x -y */
            drawBitmapPixel(bitmapDisplay, w, h, w - xLine - 1, h - (or - y) - 1, col); /* II.  Quadrant +x -y */
            drawBitmapPixel(bitmapDisplay, w, h,     xLine    ,     (or - y)    , col); /* III. Quadrant -x -y */
            drawBitmapPixel(bitmapDisplay, w, h, w - xLine - 1,     (or - y)    , col); /* IV.  Quadrant +x -y */
        }

        r = err;
        if (r <= y)
        {
            err += ++y * 2 + 1;    /* e_xy+e_y < 0 */
        }
        if (r > x || err > y) /* e_xy+e_x > 0 or no 2nd y-step */
        {
            err += ++x * 2 + 1;    /* -> x-step now */
        }
    } while (x < 0);
}

/**
 * @brief The main emulator function. This initializes rawdraw and calls
 * app_main(), then spins in a loop updating the rawdraw UI
 *
 * @param argc unused
 * @param argv unused
 * @return 0 on success, a nonzero value for any errors
 */
int main(int argc UNUSED, char** argv UNUSED)
{
#ifdef __linux__
    init_crashSignals();
#endif

    // First initialize rawdraw
    // Screen-specific configurations
    // Save window dimensions from the last loop
    CNFGSetup( "SQUAREWAVEBIRD Simulator", (TFT_WIDTH * 2) + (MIN_LED_WIDTH * 4) + 2, (TFT_HEIGHT * 2));

    // This is the 'main' that gets called when the ESP boots. It does not return
    app_main();
}

/**
 * Handle key input and drawing the display
 */
void emu_loop(void)
{
    // These are persistent!
    static short lastWindow_w = 0;
    static short lastWindow_h = 0;
    static int16_t led_w = MIN_LED_WIDTH;

#ifdef MONKEY_AROUND
    // A list of all modes to randomly jump to
    swadgeMode * allModes[] = 
    {
        &modeFighter,
        &modeJumper,
        &modeColorchord,
        &modeCredits,
        &modeDance,
        &modeFlight,
        &modeGamepad,
        &modeMainMenu,
        &modeSlideWhistle,
        &modeTest,
        &modeTiltrads,
        &modeTunernome,
        &modePaint,
        &modePaintShare,
        &modePaintReceive,
        &modePicross,
        &modePlatformer
    };

    // A list of all keys to randomly press or release, and their states
    const char randKeys[] = {'w', 's', 'a', 'd', 'l', 'k', 'o', 'i'};
    static bool keyState[sizeof(randKeys) / sizeof(randKeys[0])] = {false};

    // Time keeping
    static int64_t tLastCall = 0;
    if(0 == tLastCall)
    {
        tLastCall = esp_timer_get_time();
    }
    int64_t tNow = esp_timer_get_time();
    int64_t tElapsed = (tNow - tLastCall);

    // Randomly press or release keys every 100ms
    static int64_t keyTimer = 0;
    keyTimer += tElapsed;
    while(keyTimer >= 100000)
    {
        keyTimer -= 100000;
        int keyIdx = esp_random() % (sizeof(randKeys) / sizeof(randKeys[0]));
        keyState[keyIdx] = !keyState[keyIdx];
        emuSensorHandleKey(randKeys[keyIdx], keyState[keyIdx]);
    }

    // Change the swadge mode two minutes
    static int64_t resetToMenuTimer = 0;
    resetToMenuTimer += tElapsed;
    while(resetToMenuTimer >= (1000000 * 120))
    {
        resetToMenuTimer -= (1000000 * 120);
        static int modeIdx = 0;
        modeIdx = (modeIdx + 1) % (sizeof(allModes) / sizeof(allModes[0]));
        switchToSwadgeModeFuzzer(allModes[modeIdx]);
    }

    // Timekeeping
    tLastCall = tNow;
#endif // MONKEY_AROUND

    // Always handle inputs
    CNFGHandleInput();

    // If not running anymore, don't handle graphics
    // Must be checked after handling input, before graphics
    if(!isRunning)
    {
        exit(0);
        return;
    }

    // Grey Background
    CNFGBGColor = BG_COLOR;
    CNFGClearFrame();

    // Get the current window dimensions
    short window_w, window_h;
    CNFGGetDimensions(&window_w, &window_h);

    // If the dimensions changed
    if((lastWindow_h != window_h) || (lastWindow_w != window_w))
    {
        // Figure out how much the TFT should be scaled by
        uint8_t widthMult = (window_w - (4 * MIN_LED_WIDTH) - 2) / TFT_WIDTH;
        if(0 == widthMult)
        {
            widthMult = 1;
        }
        uint8_t heightMult = window_h / TFT_HEIGHT;
        if(0 == heightMult)
        {
            heightMult = 1;
        }
        uint8_t screenMult = MIN(widthMult, heightMult);

        // LEDs take up the rest of the horizontal space
        led_w = (window_w - 2 - (screenMult * TFT_WIDTH)) / 4;

        // Set the multiplier
        setDisplayBitmapMultiplier(screenMult);

        // Save for the next loop
        lastWindow_w = window_w;
        lastWindow_h = window_h;
    }

    // Get the LED memory
    uint8_t numLeds;
    led_t* leds = getLedMemory(&numLeds);

    // Where LEDs are drawn, kinda
    const int16_t ledOffsets[8][2] =
    {
        {1, 2},
        {0, 3},
        {0, 1},
        {1, 0},
        {2, 0},
        {3, 1},
        {3, 3},
        {2, 2},
    };

    // Draw simulated LEDs
    if (numLeds > 0 && NULL != leds)
    {
        short led_h = window_h / (numLeds / 2);
        for(int i = 0; i < numLeds; i++)
        {
            CNFGColor( (leds[i].r << 24) | (leds[i].g << 16) | (leds[i].b << 8) | 0xFF);

            int16_t xOffset = 0;
            if(ledOffsets[i][0] < 2)
            {
                xOffset = ledOffsets[i][0] * (led_w / 2);
            }
            else
            {
                xOffset = window_w - led_w - ((4 - ledOffsets[i][0]) * (led_w / 2));
            }

            int16_t yOffset = ledOffsets[i][1] * led_h;

            // Draw the LED
            CNFGTackRectangle(
                xOffset, yOffset,
                xOffset + (led_w * 3) / 2, yOffset + led_h);
        }
    }

    // Draw dividing lines
    CNFGColor( DIV_COLOR );
    CNFGTackSegment(led_w * 2, 0, led_w * 2, window_h);
    CNFGTackSegment(window_w - (led_w * 2), 0, window_w - (led_w * 2), window_h);

    // Get the display memory
    uint16_t bitmapWidth, bitmapHeight;
    uint32_t* bitmapDisplay = getDisplayBitmap(&bitmapWidth, &bitmapHeight);

    if((0 != bitmapWidth) && (0 != bitmapHeight) && (NULL != bitmapDisplay))
    {
#if defined(CONFIG_GC9307_240x280)
        plotRoundedCorners(bitmapDisplay, bitmapWidth, bitmapHeight, (bitmapWidth / TFT_WIDTH) * 40, BG_COLOR);
#endif
        // Update the display, centered
        CNFGBlitImage(bitmapDisplay,
                        (led_w * 2) + 1, (window_h - bitmapHeight) / 2,
                        bitmapWidth, bitmapHeight);
    }

    //Display the image and wait for time to display next frame.
    CNFGSwapBuffers();
}

#ifdef __linux__

/**
 * @brief Initialize a crash handler, only for Linux
 */
void init_crashSignals(void)
{
    const int sigs[] = {SIGSEGV, SIGBUS, SIGILL, SIGSYS, SIGABRT, SIGFPE, SIGIOT, SIGTRAP};
    for(int i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++)
    {
        struct sigaction action;
        memset(&action, 0, sizeof(struct sigaction));
        action.sa_flags = SA_SIGINFO;
        action.sa_sigaction = signalHandler_crash;
        sigaction(sigs[i], &action, NULL);
    }
}

/**
 * @brief Print a backtrace when a crash is caught, only for Linux
 * 
 * @param signum 
 * @param si 
 * @param vcontext 
 */
void signalHandler_crash(int signum, siginfo_t* si, void* vcontext)
{
	char msg[128] = {'\0'};
	ssize_t result;

    char fname[64] = {0};
    sprintf(fname, "crash-%ld.txt", time(NULL));
    int dumpFileDescriptor = open(fname, O_RDWR | O_CREAT | O_TRUNC,
									S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	if(-1 != dumpFileDescriptor)
	{
		snprintf(msg, sizeof(msg), "Signal %d received!\nsigno: %d, errno %d\n, code %d\n", signum, si->si_signo, si->si_errno, si->si_code);
		result = write(dumpFileDescriptor, msg, strnlen(msg, sizeof(msg)));
		(void)result;

        memset(msg, 0, sizeof(msg));
        for(int i = 0; i < __SI_PAD_SIZE; i++)
        {
            char tmp[8];
            sprintf(tmp, "%02X", si->_sifields._pad[i]);
            strcat(msg, tmp);
        }
        strcat(msg, "\n");
		result = write(dumpFileDescriptor, msg, strnlen(msg, sizeof(msg)));
		(void)result;
        
        // Print backtrace
        void *array[128];
		size_t size = backtrace(array, (sizeof(array) / sizeof(array[0])));
		backtrace_symbols_fd(array, size, dumpFileDescriptor);
		close(dumpFileDescriptor);
	}

	// Exit
	_exit(1);
}
#endif
