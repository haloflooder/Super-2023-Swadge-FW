//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "led_util.h"
#include "swadgeMode.h"
#include "mode_colorchord.h"
#include "mode_main_menu.h"
#include "settingsManager.h"
#include "bresenham.h"
#include "swadge_util.h"

// For colorchord
#include "embeddedout.h"

//==============================================================================
// Defines
//==============================================================================

#define EXIT_TIME_US 1000000
#define TEXT_Y 10
#define TEXT_MARGIN 20

//==============================================================================
// Enums
//==============================================================================

typedef enum
{
    CC_OPT_GAIN,
    CC_OPT_LED_BRIGHT,
    CC_OPT_LED_MODE,
    COLORCHORD_NUM_OPTS
} ccOpt_t;

//==============================================================================
// Functions Prototypes
//==============================================================================

void colorchordEnterMode(display_t* disp);
void colorchordExitMode(void);
void colorchordMainLoop(int64_t elapsedUs);
void colorchordAudioCb(uint16_t* samples, uint32_t sampleCnt);
void colorchordButtonCb(buttonEvt_t* evt);

//==============================================================================
// Variables
//==============================================================================

typedef struct
{
    font_t ibm_vga8;
    display_t* disp;
    dft32_data dd;
    embeddednf_data end;
    embeddedout_data eod;
    uint8_t samplesProcessed;
    uint16_t maxValue;
    ccOpt_t optSel;
    uint16_t * sampleHist;
    uint16_t sampleHistHead;
    uint16_t sampleHistCount;
} colorchord_t;

colorchord_t* colorchord;

swadgeMode modeColorchord =
{
    .modeName = "Colorchord",
    .fnEnterMode = colorchordEnterMode,
    .fnExitMode = colorchordExitMode,
    .fnMainLoop = colorchordMainLoop,
    .fnButtonCallback = colorchordButtonCb,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = colorchordAudioCb,
    .fnTemperatureCallback = NULL,
    .overrideUsb = false
};

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Enter colorchord mode, allocate memory, initialize code
 */
void colorchordEnterMode(display_t* disp)
{
    // Allocate memory for this mode
    colorchord = (colorchord_t*)calloc(1, sizeof(colorchord_t));

    // Save a pointer to the display
    colorchord->disp = disp;

    colorchord->sampleHistCount = 512;
    colorchord->sampleHist = (uint16_t*)calloc( colorchord->sampleHistCount, sizeof( uint16_t) );
    colorchord->sampleHistHead = 0;

    // Load a font
    loadFont("ibm_vga8.font", &colorchord->ibm_vga8);

    // Init CC
    InitColorChord(&colorchord->end, &colorchord->dd);
    colorchord->maxValue = 1;
}

/**
 * @brief Exit colorchord mode, free memory
 */
void colorchordExitMode(void)
{
    if(colorchord->sampleHist)
        free(colorchord->sampleHist);
    freeFont(&colorchord->ibm_vga8);
    free(colorchord);
}

/**
 * @brief This is the main loop, and it draws to the TFT
 *
 * @param elapsedUs unused
 */
void colorchordMainLoop(int64_t elapsedUs __attribute__((unused)))
{
    // Clear everything
    colorchord->disp->clearPx();

    // Draw the spectrum as a bar graph. Figure out bar and margin size
    int16_t binWidth = (colorchord->disp->w / FIXBINS);
    int16_t binMargin = (colorchord->disp->w - (binWidth * FIXBINS)) / 2;

    // This is the center line to draw the graph around
    uint8_t centerLine = (TEXT_Y + colorchord->ibm_vga8.h + 2) + (colorchord->disp->h -
                         (TEXT_Y + colorchord->ibm_vga8.h + 2)) / 2;

    // Find the max value
    for(uint16_t i = 0; i < FIXBINS; i++)
    {
        if(colorchord->end.fuzzed_bins[i] > colorchord->maxValue)
        {
            colorchord->maxValue = colorchord->end.fuzzed_bins[i];
        }
    }

    // Plot the bars
    for(uint16_t i = 0; i < FIXBINS; i++)
    {
        uint8_t height = ((colorchord->disp->h - colorchord->ibm_vga8.h - 2) * colorchord->end.fuzzed_bins[i]) /
                         colorchord->maxValue;

        paletteColor_t color = RGBtoPalette( ECCtoHEX( ( (i<<SEMIBITSPERBIN)  + colorchord->eod.RootNoteOffset ) % NOTERANGE, 255, 255 ) );
        int16_t x0 = binMargin + (i * binWidth);
        int16_t x1 = binMargin + ((i + 1) * binWidth);
        if(height < 2)
        {
            // Too small to plot, draw a line
            plotLine(colorchord->disp,
                     x0, centerLine,
                     x1, centerLine, color, 0);
        }
        else
        {
            // Big enough, fill an area
            fillDisplayArea(colorchord->disp,
                            x0, centerLine - (height / 2),
                            x1, centerLine + (height / 2), color);
        }
    }

    // Draw sinewave
    {
        SETUP_FOR_TURBO( colorchord->disp );
        int x;
        uint16_t * sampleHist = colorchord->sampleHist;
        uint16_t sampleHistCount = colorchord->sampleHistCount;
        int16_t sampleHistMark = colorchord->sampleHistHead - 1;

        for( x = 0; x < dispWidth; x++ )
        {
            if(sampleHistMark < 0)
            {
                sampleHistMark = sampleHistCount - 1;
            }
            int16_t sample = sampleHist[sampleHistMark];
            sampleHistMark--;
            uint16_t y = ((sample * dispHeight)>>16) + (dispHeight/2);
            if( y >= dispHeight ) continue;
            TURBO_SET_PIXEL( colorchord->disp, x, y, 215 );
        }
    }

    // Draw the HUD text
    char text[16] = {0};

    // Draw gain indicator
    snprintf(text, sizeof(text), "Gain: %d", getMicGain());
    drawText(colorchord->disp, &colorchord->ibm_vga8, c555, text, TEXT_MARGIN, TEXT_Y);

    // Underline it if selected
    if(CC_OPT_GAIN == colorchord->optSel)
    {
        int16_t lineY = 10 + colorchord->ibm_vga8.h + 2;
        plotLine(colorchord->disp,
                 TEXT_MARGIN, lineY,
                 TEXT_MARGIN + textWidth(&colorchord->ibm_vga8, text) - 1, lineY, c555, 0);
    }

    // Draw LED brightness indicator
    snprintf(text, sizeof(text), "LED: %d", getLedBrightness());
    int16_t tWidth = textWidth(&colorchord->ibm_vga8, text);
    drawText(colorchord->disp, &colorchord->ibm_vga8, c555, text, (colorchord->disp->w - tWidth) / 2, TEXT_Y);

    // Underline it if selected
    if(CC_OPT_LED_BRIGHT == colorchord->optSel)
    {
        int16_t lineY = TEXT_Y + colorchord->ibm_vga8.h + 2;
        plotLine(colorchord->disp,
                 (colorchord->disp->w - tWidth) / 2, lineY,
                 (colorchord->disp->w - tWidth) / 2 + tWidth - 1, lineY, c555, 0);
    }

    // Draw colorchord mode
    switch(getColorchordMode())
    {
        case LINEAR_LEDS:
        {
            snprintf(text, sizeof(text), "Rainbow");
            break;
        }
        default:
        case NUM_CC_MODES:
        case ALL_SAME_LEDS:
        {
            snprintf(text, sizeof(text), "Solid");
            break;
        }
    }
    int16_t textX = colorchord->disp->w - TEXT_MARGIN - textWidth(&colorchord->ibm_vga8, text);
    drawText(colorchord->disp, &colorchord->ibm_vga8, c555, text,
             textX, TEXT_Y);

    // Underline it if selected
    if(CC_OPT_LED_MODE == colorchord->optSel)
    {
        int16_t lineY = TEXT_Y + colorchord->ibm_vga8.h + 2;
        plotLine(colorchord->disp,
                 textX, lineY,
                 textX + textWidth(&colorchord->ibm_vga8, text) - 1, lineY, c555, 0);
    }

    // Draw reminder text
    const char exitText[] = "Start + Select to Exit";
    int16_t exitWidth = textWidth(&colorchord->ibm_vga8, exitText);
    drawText(colorchord->disp, &colorchord->ibm_vga8, c555, exitText,
             (colorchord->disp->w - exitWidth) / 2, colorchord->disp->h - colorchord->ibm_vga8.h - TEXT_Y);
}

/**
 * @brief Button callback, used to change settings
 *
 * @param evt The button event
 */
void colorchordButtonCb(buttonEvt_t* evt)
{
    if(evt->down)
    {
        switch(evt->button)
        {
            case BTN_A:
            case UP:
            case START:
            {
                switch(colorchord->optSel)
                {
                    case COLORCHORD_NUM_OPTS:
                    case CC_OPT_GAIN:
                    {
                        // Gain
                        incMicGain();
                        // Reset max val
                        colorchord->maxValue = 1;
                        break;
                    }
                    case CC_OPT_LED_MODE:
                    {
                        // LED Output
                        colorchordMode_t newMode = (getColorchordMode() + 1) % NUM_CC_MODES;
                        setColorchordMode(newMode);
                        break;
                    }
                    case CC_OPT_LED_BRIGHT:
                    {
                        incLedBrightness();
                        break;
                    }
                }
                break;
            }
            case DOWN:
            case BTN_B:
            {
                switch(colorchord->optSel)
                {
                    case COLORCHORD_NUM_OPTS:
                    case CC_OPT_GAIN:
                    {
                        // Gain
                        decMicGain();
                        // Reset max val
                        colorchord->maxValue = 1;
                        break;
                    }
                    case CC_OPT_LED_MODE:
                    {
                        // LED Output
                        colorchordMode_t newMode = getColorchordMode();
                        if(newMode == 0)
                        {
                            newMode = NUM_CC_MODES - 1;
                        }
                        else
                        {
                            newMode--;
                        }
                        setColorchordMode(newMode);
                        break;
                    }
                    case CC_OPT_LED_BRIGHT:
                    {
                        decLedBrightness();
                        break;
                    }
                }
                break;
            }
            case SELECT:
            case RIGHT:
            {
                // Select option
                colorchord->optSel++;
                if(colorchord->optSel >= COLORCHORD_NUM_OPTS)
                {
                    colorchord->optSel = 0;
                }
                break;
            }
            case LEFT:
            {
                // Select option
                if(colorchord->optSel == 0)
                {
                    colorchord->optSel = COLORCHORD_NUM_OPTS - 1;
                }
                else
                {
                    colorchord->optSel--;
                }
                break;
            }
        }
    }
}

/**
 * @brief Audio callback. Take the samples and pass them to colorchord
 *
 * @param samples The samples to process
 * @param sampleCnt The number of samples to process
 */
void colorchordAudioCb(uint16_t* samples, uint32_t sampleCnt)
{
    uint16_t * sampleHist = colorchord->sampleHist;
    uint16_t sampleHistHead = colorchord->sampleHistHead;
    uint16_t sampleHistCount = colorchord->sampleHistCount;

    // For each sample
    for(uint32_t idx = 0; idx < sampleCnt; idx++)
    {
        uint16_t sample = samples[idx];

        // Push to colorchord
        PushSample32(&colorchord->dd, sample);

        sampleHist[sampleHistHead] = sample;
        sampleHistHead++;
        if( sampleHistHead == sampleHistCount ) sampleHistHead = 0;

        // If 128 samples have been pushed
        colorchord->samplesProcessed++;
        if(colorchord->samplesProcessed >= 128)
        {
            // Update LEDs
            colorchord->samplesProcessed = 0;
            HandleFrameInfo(&colorchord->end, &colorchord->dd);
            switch (getColorchordMode())
            {
                default:
                case NUM_CC_MODES:
                case ALL_SAME_LEDS:
                {
                    UpdateAllSameLEDs(&colorchord->eod, &colorchord->end);
                    break;
                }
                case LINEAR_LEDS:
                {
                    UpdateLinearLEDs(&colorchord->eod, &colorchord->end);
                    break;
                }
            }
            setLeds((led_t*)colorchord->eod.ledOut, NUM_LEDS);
        }
    }

    colorchord->sampleHistHead = sampleHistHead;
}
