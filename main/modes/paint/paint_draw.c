#include "paint_draw.h"

#include <string.h>

#include "paint_ui.h"
#include "paint_brush.h"
#include "paint_nvs.h"
#include "paint_util.h"
#include "mode_paint.h"


#define MAX(a, b) ((a) > (b) ? (a) : (b))

paintDraw_t* paintState;
paintHelp_t* paintHelp;

/*
 * Interactive Help Definitions
 *
 * Each step here defines a tutorial step with a help message that will
 * be displayed below the canvas, and a trigger that will cause the tutorial
 * step to be considered "completed" and move on to the next step.
 *
 * TODO: Add steps for Polygon brush
 * TODO: Add an "Explain" for each brush after the tutorial
 * TODO: Disallow certain actions at certain steps to prevent confusion/desyncing
 */
const paintHelpStep_t helpSteps[] =
{
    { .trigger = PRESS_ALL, .triggerData = (UP | DOWN | LEFT | RIGHT), .prompt = "Welcome to MFPaint!\nLet's get started: First, use the D-Pad to move the cursor around!" },
    { .trigger = RELEASE, .triggerData = BTN_A, .prompt = "Excellent!\nNow, press A to draw something!" },
    { .trigger = PRESS, .triggerData = SELECT, .prompt = "Now, let's pick a different color.\nFirst, hold SELECT..." },
    { .trigger = PRESS, .triggerData = SELECT | DOWN, .prompt = "Then, press D-Pad DOWN to select a color..." },
    { .trigger = RELEASE, .triggerData = SELECT, .prompt = "And release SELECT to confirm!" },
    { .trigger = RELEASE, .triggerData = BTN_B, .prompt = "Great choice! You can also quickly swap the foreground and background colors with the B BUTTON" },
    { .trigger = PRESS, .triggerData = SELECT, .prompt = "Now, let's change the brush size. Hold SELECT again..." },
    { .trigger = PRESS, .triggerData = SELECT | BTN_A, .prompt = "Then, Press the A BUTTON to increase the brush size." },
    { .trigger = RELEASE, .triggerData = SELECT, .prompt = "And release SELECT to confirm!" },
    { .trigger = RELEASE, .triggerData = BTN_A, .prompt = "Press A to draw again with the larger brush!" },
    { .trigger = PRESS, .triggerData = SELECT, .prompt = "Wow! Now, let's change it back. Hold SELECT again..." },
    { .trigger = PRESS, .triggerData = SELECT | BTN_B, .prompt = "Then, press B to decrease the brush size." },
    { .trigger = RELEASE, .triggerData = SELECT, .prompt = "And release SELECT to confirm!" },
    { .trigger = PRESS, .triggerData = SELECT, .prompt = "Now, let's try a different brush. Hold SELECT again..." },
    { .trigger = PRESS, .triggerData = SELECT | RIGHT, .prompt = "Then, press D-Pad RIGHT to select a brush..." },
    { .trigger = RELEASE, .triggerData = SELECT, .prompt = "And release SELECT to confirm!" },
    { .trigger = CHANGE_BRUSH, .triggerDataPtr = (void*)"Rectangle", .prompt = "Now, choose the RECTANGLE brush!" },
    { .trigger = RELEASE, .triggerData = BTN_A, .prompt = "Now, press A to select the first corner of the rectangle..." },
    { .trigger = PRESS_ANY, .triggerData = (UP | DOWN | LEFT | RIGHT), .prompt = "Then move somewhere else..." },
    { .trigger = RELEASE, .triggerData = BTN_A, .prompt = "Press A again to pick the other coner of the rectangle. Note that the first point you picked will blink!" },
    { .trigger = PRESS, .triggerData = START, .prompt = "Good job! Now, let's press START to toggle the menu." },
    { .trigger = PRESS_ANY, .triggerData = UP | DOWN | SELECT, .prompt = "Press UP, DOWN, or SELECT to go through the menu items" },
    { .trigger = SELECT_MENU_ITEM, .triggerData = PICK_SLOT_SAVE, .prompt = "Now, select the SAVE option.." },
    { .trigger = PRESS_ANY, .triggerData = LEFT | RIGHT, .prompt = "Use D-Pad LEFT and RIGHT to switch between save slots, or other options in the menu" },
    { .trigger = PRESS_ANY, .triggerData = BTN_A | BTN_B, .prompt = "Use the A BUTTON to confirm, or the B BUTTON to cancel and go back. If data could be lost, you'll always be asked to confirm!" },
    { .trigger = SELECT_MENU_ITEM, .triggerData = HIDDEN, .prompt = "Press START or the B BUTTON to exit the menu." },
    { .trigger = PRESS, .triggerData = START, .prompt = "Let's try editing the palette! Press START to open the menu one more time." },
    { .trigger = SELECT_MENU_ITEM, .triggerData = EDIT_PALETTE, .prompt = "Use UP, DOWN, and SELECT to select EDIT PALETTE" },
    { .trigger = PRESS, .triggerData = BTN_A, .prompt = "Press the A BUTTON to begin editing the palette" },
    { .trigger = PRESS_ANY, .triggerData = UP | DOWN, .prompt = "Use D-Pad UP and DOWN to select a color to edit" },
    { .trigger = PRESS_ANY, .triggerData = LEFT | RIGHT, .prompt = "Use D-Pad LEFT and RIGHT to change the selected color's RED, GREED, or BLUE value." },
    { .trigger = PRESS, .triggerData = SELECT, .prompt = "Press SELECT to switch between editing the color's RED, GREEN, or BLUE values." },
    { .trigger = PRESS_ANY, .triggerData = BTN_A | BTN_B, .prompt = "Press the A BUTTON to confirm and update the canvas with the new color. Or, press the B BUTTON to reset." },
    { .trigger = NO_TRIGGER, .prompt = "That's everything. Happy painting!" },
};

const paintHelpStep_t* lastHelp = helpSteps + sizeof(helpSteps) / sizeof(helpSteps[0]) - 1;


static paletteColor_t defaultPalette[] =
{
    c000, // black
    c555, // white
    c012, // dark blue
    c505, // fuchsia
    c540, // yellow
    c235, // cornflower

    c222, // light gray
    c444, // dark gray

    c500, // red
    c050, // green
    c055, // cyan
    c005, // blue
    c530, // orange?
    c503, // pink
    c350, // lime green
    c522, // salmon
};

brush_t brushes[] =
{
    { .name = "Square Pen", .mode = HOLD_DRAW,  .maxPoints = 1, .minSize = 1, .maxSize = 32, .fnDraw = paintDrawSquarePen, .iconName = "square_pen" },
    { .name = "Circle Pen", .mode = HOLD_DRAW,  .maxPoints = 1, .minSize = 1, .maxSize = 32, .fnDraw = paintDrawCirclePen, .iconName = "circle_pen" },
    { .name = "Line",       .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawLine, .iconName = "line" },
    { .name = "Bezier Curve", .mode = PICK_POINT, .maxPoints = 4, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawCurve, .iconName = "curve" },
    { .name = "Rectangle",  .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawRectangle, .iconName = "rect" },
    { .name = "Filled Rectangle", .mode = PICK_POINT, .maxPoints = 2, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawFilledRectangle, .iconName = "rect_filled" },
    { .name = "Circle",     .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawCircle, .iconName = "circle" },
    { .name = "Filled Circle", .mode = PICK_POINT, .maxPoints = 2, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawFilledCircle, .iconName = "circle_filled" },
    { .name = "Ellipse",    .mode = PICK_POINT, .maxPoints = 2, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawEllipse, .iconName = "ellipse" },
    { .name = "Polygon",    .mode = PICK_POINT_LOOP, .maxPoints = 16, .minSize = 1, .maxSize = 1, .fnDraw = paintDrawPolygon, .iconName = "polygon" },
    { .name = "Squarewave", .mode = PICK_POINT, .maxPoints = 2, .minSize = 0, .maxSize = 32, .fnDraw = paintDrawSquareWave, .iconName = "squarewave" },
    { .name = "Paint Bucket", .mode = PICK_POINT, .maxPoints = 1, .minSize = 0, .maxSize = 0, .fnDraw = paintDrawPaintBucket, .iconName = "paint_bucket" },
};

const char activeIconStr[] = "%s_active.wsg";
const char inactiveIconStr[] = "%s_inactive.wsg";

const brush_t* firstBrush = brushes;
const brush_t* lastBrush = brushes + sizeof(brushes) / sizeof(brushes[0]) - 1;

void paintDrawScreenSetup(display_t* disp)
{
    PAINT_LOGD("Allocating %zu bytes for paintState", sizeof(paintDraw_t));
    paintState = calloc(sizeof(paintDraw_t), 1);
    paintState->disp = disp;
    paintState->canvas.disp = disp;

    loadFont(PAINT_TOOLBAR_FONT, &(paintState->toolbarFont));
    loadFont(PAINT_SAVE_MENU_FONT, &(paintState->saveMenuFont));
    loadFont(PAINT_SMALL_FONT, &(paintState->smallFont));
    paintState->clearScreen = true;
    paintState->blinkOn = true;
    paintState->blinkTimer = 0;


    // Set up the brush icons
    uint16_t spriteH = 0;
    char iconName[32];
    for (brush_t* brush = brushes; brush <= lastBrush; brush++)
    {
        snprintf(iconName, sizeof(iconName), activeIconStr, brush->iconName);
        if (!loadWsg(iconName, &brush->iconActive))
        {
            PAINT_LOGE("Loading icon %s failed!!!", iconName);
        }

        snprintf(iconName, sizeof(iconName), inactiveIconStr, brush->iconName);
        if (!loadWsg(iconName, &brush->iconInactive))
        {
            PAINT_LOGE("Loading icon %s failed!!!", iconName);
        }

        // Keep track of the tallest sprite for layout purposes
        if (brush->iconActive.h > spriteH)
        {
            spriteH = brush->iconActive.h;
        }

        if (brush->iconInactive.h > spriteH)
        {
            spriteH = brush->iconInactive.h;
        }
    }

    if (!loadWsg("pointer.wsg", &paintState->picksWsg))
    {
        PAINT_LOGE("Loading pointer.wsg icon failed!!!");
    }

    if (!loadWsg("brush_size.wsg", &paintState->brushSizeWsg))
    {
        PAINT_LOGE("Loading brush_size.wsg icon failed!!!");
    }

    // Setup the margins
    // Top: Leave room for the tallest of...
    // * The save menu text plus padding above and below it
    // * The tallest sprite, plus some padding
    // * The minimum height of the color picker, plus some padding
    // ... plus, 1px for the canvas border

    uint16_t saveMenuSpace = paintState->saveMenuFont.h + 2 * PAINT_TOOLBAR_TEXT_PADDING_Y;
    uint16_t toolbarSpace = spriteH + 2;

    // text above picker bars, plus 2px margin, plus min colorbar height (6), plus 2px above and below for select borders
    uint16_t colorPickerSpace = paintState->smallFont.h + 2 + PAINT_COLOR_PICKER_MIN_BAR_H + 2 + 2;

    paintState->marginTop = MAX(saveMenuSpace, MAX(toolbarSpace, colorPickerSpace)) + 1;


    // Left: Leave room for the color boxes, their margins, their borders, and the canvas border
    paintState->marginLeft = PAINT_COLORBOX_W + PAINT_COLORBOX_MARGIN_X * 2 + 2 + 1;
    // Bottom: Leave room for the brush name, 4px margin, and the canvas border
    paintState->marginBottom = paintState->toolbarFont.h + 4 + 1;
    // Right: We just need to stay away from the rounded corner, so like, 12px?
    paintState->marginRight = 12;

    if (paintHelp != NULL)
    {
        // Set up some tutorial things that depend on basic paintState data
        paintTutorialPostSetup();

        // We're in help mode! We need some more space for the text
        paintState->marginBottom += paintHelp->helpH;
    }

    paintLoadIndex(&paintState->index);

    if (paintHelp == NULL && paintGetAnySlotInUse(paintState->index))
    {
        // If there's a saved image, load that (but not in the tutorial)
        paintState->selectedSlot = paintGetRecentSlot(paintState->index);
        paintState->doLoad = true;
    }
    else
    {
        // Set up a blank canvas with the default size
        paintState->canvas.w = PAINT_DEFAULT_CANVAS_WIDTH;
        paintState->canvas.h = PAINT_DEFAULT_CANVAS_HEIGHT;

        // Automatically position the canvas in the center of the drawable area at the max scale that will fit
        paintPositionDrawCanvas();

        // load the default palette
        memcpy(paintState->canvas.palette, defaultPalette, sizeof(defaultPalette) / sizeof(paletteColor_t));
        getArtist()->fgColor = paintState->canvas.palette[0];
        getArtist()->bgColor = paintState->canvas.palette[1];
    }

    paintGenerateCursorSprite(&paintState->cursorWsg, &paintState->canvas);

    // Init the cursors for each artist
    // TODO only do one for singleplayer?
    for (uint8_t i = 0; i < sizeof(paintState->artist) / sizeof(paintState->artist[0]); i++)
    {
        initCursor(&paintState->artist[i].cursor, &paintState->canvas, &paintState->cursorWsg);
        initPxStack(&paintState->artist[i].pickPoints);
        paintState->artist[i].brushDef = firstBrush;
        paintState->artist[i].brushWidth = firstBrush->minSize;

        setCursorSprite(&paintState->artist[i].cursor, &paintState->canvas, &paintState->cursorWsg);
        setCursorOffset(&paintState->artist[i].cursor, (paintState->canvas.xScale - paintState->cursorWsg.w) / 2, (paintState->canvas.yScale - paintState->cursorWsg.h) / 2);
        moveCursorAbsolute(getCursor(), &paintState->canvas, paintState->canvas.w / 2, paintState->canvas.h / 2);
    }

    paintState->disp->clearPx();

    // Clear the LEDs
    // Might not be necessary here
    paintUpdateLeds();

    PAINT_LOGI("It's paintin' time! Canvas is %d x %d pixels!", paintState->canvas.w, paintState->canvas.h);
}

void paintDrawScreenCleanup(void)
{
    for (brush_t* brush = brushes; brush <= lastBrush; brush++)
    {
        freeWsg(&brush->iconActive);
        freeWsg(&brush->iconInactive);
    }

    freeWsg(&paintState->brushSizeWsg);
    freeWsg(&paintState->picksWsg);

    for (uint8_t i = 0; i < sizeof(paintState->artist) / sizeof(paintState->artist[0]) ;i++)
    {
        deinitCursor(&paintState->artist[i].cursor);
        freePxStack(&paintState->artist[i].pickPoints);
    }

    paintFreeCursorSprite(&paintState->cursorWsg);

    freeFont(&paintState->smallFont);
    freeFont(&paintState->saveMenuFont);
    freeFont(&paintState->toolbarFont);
    free(paintState);
}

void paintTutorialSetup(display_t* disp)
{
    paintHelp = calloc(sizeof(paintHelp_t), 1);
    paintHelp->curHelp = helpSteps;
}

// gets called after paintState is allocated and has basic info, but before canvas layout is done
void paintTutorialPostSetup(void)
{
    paintHelp->helpH = PAINT_HELP_TEXT_LINES * (paintState->toolbarFont.h + 1) - 1;
}

void paintTutorialCleanup(void)
{
    free(paintHelp);
    paintHelp = NULL;
}

bool paintTutorialCheckTriggers(void)
{
    switch (paintHelp->curHelp->trigger)
    {
    case PRESS_ALL:
        return (paintHelp->allButtons & paintHelp->curHelp->triggerData) == paintHelp->curHelp->triggerData;

    case PRESS_ANY:
        return (paintHelp->curButtons & paintHelp->curHelp->triggerData) != 0 && paintHelp->lastButtonDown;

    case PRESS:
        return (paintHelp->curButtons & (paintHelp->curHelp->triggerData)) == paintHelp->curHelp->triggerData && paintHelp->lastButtonDown;

    case RELEASE:
        return paintHelp->lastButtonDown == false && paintHelp->lastButton == paintHelp->curHelp->triggerData;

    case CHANGE_BRUSH:
        return !strcmp(getArtist()->brushDef->name, paintHelp->curHelp->triggerDataPtr) && paintHelp->curButtons == 0;

    case CHANGE_COLOR:
        return getArtist()->fgColor == paintHelp->curHelp->triggerData;

    case SELECT_MENU_ITEM:
        return paintState->saveMenu == paintHelp->curHelp->triggerData;

    case NO_TRIGGER:
    default:
        break;
    }

    return false;
}

void paintPositionDrawCanvas(void)
{
    // Calculate the highest scale that will fit on the screen
    uint8_t scale = paintGetMaxScale(paintState->disp, paintState->canvas.w, paintState->canvas.h, paintState->marginLeft + paintState->marginRight, paintState->marginTop + paintState->marginBottom);

    paintState->canvas.xScale = scale;
    paintState->canvas.yScale = scale;

    paintState->canvas.x = paintState->marginLeft + (paintState->canvas.disp->w - paintState->marginLeft - paintState->marginRight - paintState->canvas.w * paintState->canvas.xScale) / 2;
    paintState->canvas.y = paintState->marginTop + (paintState->canvas.disp->h - paintState->marginTop - paintState->marginBottom - paintState->canvas.h * paintState->canvas.yScale) / 2;
}

void paintDrawScreenMainLoop(int64_t elapsedUs)
{
    // Screen Reset
    if (paintState->clearScreen)
    {
        hideCursor(getCursor(), &paintState->canvas);
        paintClearCanvas(&paintState->canvas, getArtist()->bgColor);
        paintRenderToolbar(getArtist(), &paintState->canvas, paintState, firstBrush, lastBrush);
        paintUpdateLeds();
        showCursor(getCursor(), &paintState->canvas);
        paintState->unsaved = false;
        paintState->clearScreen = false;
    }

    // Save and Load
    if (paintState->doSave || paintState->doLoad)
    {
        paintState->saveInProgress = true;

        if (paintState->doSave)
        {
            hideCursor(getCursor(), &paintState->canvas);
            paintHidePickPoints();
            paintSave(&paintState->index, &paintState->canvas, paintState->selectedSlot);
            paintDrawPickPoints();
            showCursor(getCursor(), &paintState->canvas);
        }
        else
        {
            if (paintGetSlotInUse(paintState->index, paintState->selectedSlot))
            {
                // Load from the selected slot if it's been used
                hideCursor(getCursor(), &paintState->canvas);
                paintClearCanvas(&paintState->canvas, getArtist()->bgColor);
                if(paintLoadDimensions(&paintState->canvas, paintState->selectedSlot))
                {
                    paintPositionDrawCanvas();
                    paintLoad(&paintState->index, &paintState->canvas, paintState->selectedSlot);
                    paintSetRecentSlot(&paintState->index, paintState->selectedSlot);

                    getArtist()->fgColor = paintState->canvas.palette[0];
                    getArtist()->bgColor = paintState->canvas.palette[1];

                    paintFreeCursorSprite(&paintState->cursorWsg);
                    paintGenerateCursorSprite(&paintState->cursorWsg, &paintState->canvas);
                    setCursorSprite(getCursor(), &paintState->canvas, &paintState->cursorWsg);
                    setCursorOffset(getCursor(), (paintState->canvas.xScale - paintState->cursorWsg.w) / 2, (paintState->canvas.yScale - paintState->cursorWsg.h) / 2);

                    // Put the cursor in the middle of the screen
                    moveCursorAbsolute(getCursor(), &paintState->canvas, paintState->canvas.w / 2, paintState->canvas.h / 2);
                    showCursor(getCursor(), &paintState->canvas);
                    paintUpdateLeds();
                }
                else
                {
                    PAINT_LOGE("Slot %d has 0 dimension! Stopping load and clearing slot", paintState->selectedSlot);
                    paintClearSlot(&paintState->index, paintState->selectedSlot);
                    paintReturnToMainMenu();
                }
            }
            else
            {
                // If the slot hasn't been used yet, just clear the screen
                paintState->clearScreen = true;
            }
        }

        paintState->unsaved = false;
        paintState->doSave = false;
        paintState->doLoad = false;
        paintState->saveInProgress = false;

        paintState->buttonMode = BTN_MODE_DRAW;
        paintState->saveMenu = HIDDEN;

        paintState->redrawToolbar = true;
    }

    if (paintState->recolorPickPoints)
    {
        paintDrawPickPoints();
        paintUpdateLeds();
        paintState->recolorPickPoints = false;
    }


    if (paintState->redrawToolbar)
    {
        paintRenderToolbar(getArtist(), &paintState->canvas, paintState, firstBrush, lastBrush);
        paintState->redrawToolbar = false;
    }
    else
    {
        // Don't remember why we only do this when redrawToolbar is true
        // Oh, it's because `paintState->redrawToolbar` is mostly only set in select mode unless you press B?
        if (paintState->aHeld)
        {
            paintDoTool(getCursor()->x, getCursor()->y, getArtist()->fgColor);

            if (getArtist()->brushDef->mode != HOLD_DRAW)
            {
                paintState->aHeld = false;
            }
        }

        if (paintState->moveX || paintState->moveY)
        {
            paintState->btnHoldTime += elapsedUs;
            if (paintState->firstMove || paintState->btnHoldTime >= BUTTON_REPEAT_TIME)
            {
                moveCursorRelative(getCursor(), &paintState->canvas, paintState->moveX, paintState->moveY);
                paintRenderToolbar(getArtist(), &paintState->canvas, paintState, firstBrush, lastBrush);

                paintState->firstMove = false;
            }
        }
    }

    if (paintState->index & PAINT_ENABLE_BLINK)
    {
        if (paintState->blinkOn && paintState->blinkTimer >= BLINK_TIME_ON)
        {
            paintState->blinkTimer %= BLINK_TIME_ON;
            paintState->blinkOn = false;
            paintHidePickPoints();
        }
        else if (!paintState->blinkOn && paintState->blinkTimer >= BLINK_TIME_OFF)
        {
            paintState->blinkTimer %= BLINK_TIME_OFF;
            paintState->blinkOn = true;
            paintDrawPickPoints();
        } else if (paintState->blinkOn)
        {
            paintDrawPickPoints();
        }

        paintState->blinkTimer += elapsedUs;
    }
    else
    {
        paintDrawPickPoints();
    }

    drawCursor(getCursor(), &paintState->canvas);

    if (paintHelp != NULL)
    {
        const char* rest = drawTextWordWrap(paintState->disp, &paintState->toolbarFont, c000, paintHelp->curHelp->prompt,
                 paintState->canvas.x, paintState->canvas.y + paintState->canvas.h * paintState->canvas.yScale + 3,
                 paintState->disp->w, paintState->disp->h - paintState->marginBottom + paintHelp->helpH);
        if (rest)
        {
            PAINT_LOGW("Some tutorial text didn't fit: %s", rest);
        }
    }
}

void paintSaveModePrevItem(void)
{
    switch (paintState->saveMenu)
    {
        case HIDDEN:
        break;

        case PICK_SLOT_SAVE:
        case CONFIRM_OVERWRITE:
            paintState->saveMenu = EXIT;
        break;

        case PICK_SLOT_LOAD:
        case CONFIRM_UNSAVED:
            paintState->saveMenu = PICK_SLOT_SAVE;
        break;

        case EDIT_PALETTE:
        case COLOR_PICKER:
            paintState->saveMenu = PICK_SLOT_LOAD;
        break;

        case CLEAR:
        case CONFIRM_CLEAR:
            paintState->saveMenu = EDIT_PALETTE;
        break;

        case EXIT:
        case CONFIRM_EXIT:
            paintState->saveMenu = CLEAR;
        break;
    }

    paintState->saveMenuBoolOption = false;

    // If we're selecting "Load", then make sure we can actually load a slot
    if (paintState->saveMenu == PICK_SLOT_LOAD)
    {
        // If no slots are in use, skip again
        if (!paintGetAnySlotInUse(paintState->index))
        {
            paintState->saveMenu = PICK_SLOT_SAVE;
        }
        else if (!paintGetSlotInUse(paintState->index, paintState->selectedSlot))
        {
            // Otherwise, make sure the selected slot is in use
            paintState->selectedSlot = paintGetNextSlotInUse(paintState->index, paintState->selectedSlot);
        }
    }
}

void paintSaveModeNextItem(void)
{
    switch (paintState->saveMenu)
    {
        case HIDDEN:
        break;

        case PICK_SLOT_SAVE:
        case CONFIRM_OVERWRITE:
            paintState->saveMenu = PICK_SLOT_LOAD;
        break;

        case PICK_SLOT_LOAD:
        case CONFIRM_UNSAVED:
            paintState->saveMenu = EDIT_PALETTE;
        break;

        case EDIT_PALETTE:
        case COLOR_PICKER:
            paintState->saveMenu = CLEAR;
        break;

        case CLEAR:
        case CONFIRM_CLEAR:
            paintState->saveMenu = EXIT;
        break;

        case EXIT:
        case CONFIRM_EXIT:
            paintState->saveMenu = PICK_SLOT_SAVE;
        break;
    }

    paintState->saveMenuBoolOption = false;

    // If we're selecting "Load", then make sure we can actually load a slot
    if (paintState->saveMenu == PICK_SLOT_LOAD)
    {
        // If no slots are in use, skip again
        if (!paintGetAnySlotInUse(paintState->index))
        {
            paintState->saveMenu = EDIT_PALETTE;
        }
        else if (!paintGetSlotInUse(paintState->index, paintState->selectedSlot))
        {
            // Otherwise, make sure the selected slot is in use
            paintState->selectedSlot = paintGetNextSlotInUse(paintState->index, paintState->selectedSlot);
        }
    }
}

void paintSaveModePrevOption(void)
{
    switch (paintState->saveMenu)
    {
        case PICK_SLOT_SAVE:
            paintState->selectedSlot = PREV_WRAP(paintState->selectedSlot, PAINT_SAVE_SLOTS);
        break;

        case PICK_SLOT_LOAD:
            paintState->selectedSlot = paintGetPrevSlotInUse(paintState->index, paintState->selectedSlot);
        break;

        case CONFIRM_OVERWRITE:
        case CONFIRM_UNSAVED:
        case CONFIRM_CLEAR:
        case CONFIRM_EXIT:
            // Just flip the state
            paintState->saveMenuBoolOption = !paintState->saveMenuBoolOption;
        break;

        case HIDDEN:
        case EDIT_PALETTE:
        case COLOR_PICKER:
        case CLEAR:
        case EXIT:
        // Do nothing, there are no options here
        break;
    }
}

void paintSaveModeNextOption(void)
{
    switch (paintState->saveMenu)
    {
        case PICK_SLOT_SAVE:
            paintState->selectedSlot = NEXT_WRAP(paintState->selectedSlot, PAINT_SAVE_SLOTS);
        break;

        case PICK_SLOT_LOAD:
            paintState->selectedSlot = paintGetNextSlotInUse(paintState->index, paintState->selectedSlot);
        break;

        case CONFIRM_OVERWRITE:
        case CONFIRM_UNSAVED:
        case CONFIRM_CLEAR:
        case CONFIRM_EXIT:
            // Just flip the state
            paintState->saveMenuBoolOption = !paintState->saveMenuBoolOption;
        break;

        case HIDDEN:
        case EDIT_PALETTE:
        case COLOR_PICKER:
        case CLEAR:
        case EXIT:
        // Do nothing, there are no options here
        break;
    }
}

void paintEditPaletteUpdate(void)
{
    paintState->newColor = (paintState->editPaletteR * 36 + paintState->editPaletteG * 6 + paintState->editPaletteB);
    paintUpdateLeds();
}

void paintEditPaletteDecChannel(void)
{
    *(paintState->editPaletteCur) = PREV_WRAP(*(paintState->editPaletteCur), 6);
    paintEditPaletteUpdate();
}

void paintEditPaletteIncChannel(void)
{
    *(paintState->editPaletteCur) = NEXT_WRAP(*(paintState->editPaletteCur), 6);
    paintEditPaletteUpdate();
}

void paintEditPaletteNextChannel(void)
{
    if (paintState->editPaletteCur == &paintState->editPaletteR)
    {
        paintState->editPaletteCur = &paintState->editPaletteG;
    }
    else if (paintState->editPaletteCur == &paintState->editPaletteG)
    {
        paintState->editPaletteCur = &paintState->editPaletteB;
    }
    else
    {
        paintState->editPaletteCur = &paintState->editPaletteR;
    }
}

void paintEditPaletteSetupColor(void)
{
    paletteColor_t col = paintState->canvas.palette[paintState->paletteSelect];
    paintState->editPaletteCur = &paintState->editPaletteR;
    paintState->editPaletteR = col / 36;
    paintState->editPaletteG = (col / 6) % 6;
    paintState->editPaletteB = col % 6;
    paintState->newColor = col;
    paintEditPaletteUpdate();
}

void paintEditPalettePrevColor(void)
{
    paintState->paletteSelect = PREV_WRAP(paintState->paletteSelect, PAINT_MAX_COLORS);
    paintEditPaletteSetupColor();
}

void paintEditPaletteNextColor(void)
{
    paintState->paletteSelect = NEXT_WRAP(paintState->paletteSelect, PAINT_MAX_COLORS);
    paintEditPaletteSetupColor();
}

void paintEditPaletteConfirm(void)
{
    // Save the old color, and update the palette with the new color
    paletteColor_t old = paintState->canvas.palette[paintState->paletteSelect];
    paletteColor_t new = paintState->newColor;
    paintState->canvas.palette[paintState->paletteSelect] = new;

    // Only replace the color on the canvas if the old color is no longer in the palette
    bool doReplace = true;
    for (uint8_t i = 0; i < PAINT_MAX_COLORS; i++)
    {
        if (paintState->canvas.palette[i] == old)
        {
            doReplace = false;
            break;
        }
    }

    // This color is no longer in the palette, so replace it with the new one
    if (doReplace)
    {
        // Make sure the FG/BG colors aren't outsie of the palette
        if (getArtist()->fgColor == old)
        {
            getArtist()->fgColor = new;
        }

        if (getArtist()->bgColor == old)
        {
            getArtist()->bgColor = new;
        }

        hideCursor(getCursor(), &paintState->canvas);
        paintHidePickPoints();

        // And replace it within the canvas
        paintColorReplace(&paintState->canvas, old, new);
        paintState->unsaved = true;

        paintDrawPickPoints();
        showCursor(getCursor(), &paintState->canvas);
    }
}

void paintPaletteModeButtonCb(const buttonEvt_t* evt)
{
    if (evt->down)
    {
        paintState->redrawToolbar = true;
        switch (evt->button)
        {
            case BTN_A:
            {
                // Don't do anything? Confirm change?
                paintEditPaletteConfirm();
                break;
            }

            case BTN_B:
            {
                // Revert back to the original color
                if (paintState->newColor != paintState->canvas.palette[paintState->paletteSelect])
                {
                    paintEditPaletteSetupColor();
                }
                else
                {
                    paintState->paletteSelect = 0;
                    paintState->buttonMode = BTN_MODE_DRAW;
                    paintState->saveMenu = HIDDEN;
                }
                break;
            }

            case START:
            {
                // Handled in button up
                break;
            }

            case SELECT:
            {
                // Swap between R, G, and B
                paintEditPaletteNextChannel();
                break;
            }

            case UP:
            {
                // Prev color
                paintEditPalettePrevColor();
                break;
            }

            case DOWN:
            {
                // Next color
                paintEditPaletteNextColor();
                break;
            }

            case LEFT:
            {
                // {R/G/B}--
                paintEditPaletteDecChannel();
                break;
            }

            case RIGHT:
            {
                // {R/G/B}++
                paintEditPaletteIncChannel();
                break;
            }
        }
    }
    else
    {
        // Button up
        if (evt->button == START)
        {
            // Return to draw mode
            paintState->paletteSelect = 0;
            paintState->buttonMode = BTN_MODE_DRAW;
            paintState->saveMenu = HIDDEN;
        }
    }
}

void paintSaveModeButtonCb(const buttonEvt_t* evt)
{
    if (evt->down)
    {
        //////// Save menu button down
        paintState->redrawToolbar = true;
        switch (evt->button)
        {
            case BTN_A:
            {
                switch (paintState->saveMenu)
                {
                    case PICK_SLOT_SAVE:
                    {
                        if (paintGetSlotInUse(paintState->index, paintState->selectedSlot))
                        {
                            paintState->saveMenuBoolOption = false;
                            paintState->saveMenu = CONFIRM_OVERWRITE;
                        }
                        else
                        {
                            paintState->doSave = true;
                        }
                        break;
                    }

                    case PICK_SLOT_LOAD:
                    {
                        if (paintState->unsaved)
                        {
                            paintState->saveMenuBoolOption = false;
                            paintState->saveMenu = CONFIRM_UNSAVED;
                        }
                        else
                        {
                            paintState->doLoad = true;
                        }
                        break;
                    }

                    case CONFIRM_OVERWRITE:
                    {
                        if (paintState->saveMenuBoolOption)
                        {
                            paintState->doSave = true;
                            paintState->saveMenu = HIDDEN;
                        }
                        else
                        {
                            paintState->saveMenu = PICK_SLOT_SAVE;
                        }
                        break;
                    }

                    case CONFIRM_UNSAVED:
                    {
                        if (paintState->saveMenuBoolOption)
                        {
                            paintState->doLoad = true;
                            paintState->saveMenu = HIDDEN;
                        }
                        else
                        {
                            paintState->saveMenu = PICK_SLOT_LOAD;
                        }
                        break;
                    }

                    case EDIT_PALETTE:
                    {
                        paintState->saveMenu = COLOR_PICKER;
                        paintState->buttonMode = BTN_MODE_PALETTE;
                        paintState->paletteSelect = 0;
                        paintEditPaletteSetupColor();
                        break;
                    }

                    case CONFIRM_CLEAR:
                    {
                        if (paintState->saveMenuBoolOption)
                        {
                            paintState->clearScreen = true;
                            paintState->saveMenu = HIDDEN;
                            paintState->buttonMode = BTN_MODE_DRAW;
                        }
                        else
                        {
                            paintState->saveMenu = CLEAR;
                        }
                        break;
                    }

                    case CONFIRM_EXIT:
                    {
                        if (paintState->saveMenuBoolOption)
                        {
                            paintReturnToMainMenu();
                        }
                        else
                        {
                            paintState->saveMenu = EXIT;
                        }
                        break;
                    }

                    case CLEAR:
                    {
                        if (paintState->unsaved)
                        {
                            paintState->saveMenuBoolOption = false;
                            paintState->saveMenu = CONFIRM_CLEAR;
                        }
                        else
                        {
                            paintState->clearScreen = true;
                            paintState->saveMenu = HIDDEN;
                            paintState->buttonMode = BTN_MODE_DRAW;
                        }
                        break;
                    }
                    case EXIT:
                    {
                        if (paintState->unsaved)
                        {
                            paintState->saveMenuBoolOption = false;
                            paintState->saveMenu = CONFIRM_EXIT;
                        }
                        else
                        {
                            paintReturnToMainMenu();
                        }
                        break;
                    }
                    // These cases shouldn't actually happen
                    case HIDDEN:
                    case COLOR_PICKER:
                    {
                        paintState->buttonMode = BTN_MODE_DRAW;
                        break;
                    }
                }
                break;
            }

            case UP:
            {
                paintSaveModePrevItem();
                break;
            }

            case DOWN:
            case SELECT:
            {

                paintSaveModeNextItem();
                break;
            }

            case LEFT:
            {
                paintSaveModePrevOption();
                break;
            }

            case RIGHT:
            {
                paintSaveModeNextOption();
                break;
            }

            case BTN_B:
            {
                // Exit save menu
                paintState->saveMenu = HIDDEN;
                paintState->buttonMode = BTN_MODE_DRAW;
                break;
            }

            case START:
            // Handle this in button up
            break;
        }
    }
    else
    {
        //////// Save mode button release
        if (evt->button == START)
        {
            // Exit save menu
            paintState->saveMenu = HIDDEN;
            paintState->buttonMode = BTN_MODE_DRAW;
            paintState->redrawToolbar = true;
        }
    }
}

void paintSelectModeButtonCb(const buttonEvt_t* evt)
{
    if (!evt->down)
    {
        //////// Select-mode button release
        switch (evt->button)
        {
            case SELECT:
            {
                // Exit select mode
                paintState->buttonMode = BTN_MODE_DRAW;

                // Set the current selection as the FG color and rearrange the rest
                paintUpdateRecents(paintState->paletteSelect);
                paintState->paletteSelect = 0;

                paintState->redrawToolbar = true;
                break;
            }

            case UP:
            {
                // Select previous color
                paintState->redrawToolbar = true;
                paintState->paletteSelect = PREV_WRAP(paintState->paletteSelect, PAINT_MAX_COLORS);
                paintUpdateLeds();
                break;
            }

            case DOWN:
            {
                // Select next color
                paintState->redrawToolbar = true;
                paintState->paletteSelect = NEXT_WRAP(paintState->paletteSelect, PAINT_MAX_COLORS);
                paintUpdateLeds();
                break;
            }

            case LEFT:
            {
                // Select previous brush
                paintPrevTool();
                paintState->redrawToolbar = true;
                break;
            }

            case RIGHT:
            {
                // Select next brush
                paintNextTool();
                paintState->redrawToolbar = true;
                break;
            }


            case BTN_A:
            {
                // Increase brush size / next variant
                paintIncBrushWidth(1);
                paintState->redrawToolbar = true;
                break;
            }

            case BTN_B:
            {
                // Decrease brush size / prev variant
                paintDecBrushWidth(1);
                paintState->redrawToolbar = true;
                break;
            }

            case START:
            // Start does nothing in select-mode, plus it's used for exit
            break;
        }
    }
}

void paintDrawScreenTouchCb(const touch_event_t* evt)
{
    PAINT_LOGD("touchCb({ .state = %d, .touch_pad_t = %d, .down = %d, .position = %d })", evt->state, evt->pad, evt->down, evt->position);
    // we get the first down event
    // check if paintState->down is false, set it to true
    // save the position at paintState->firstTouch (and also lastTouch)
    // then, for any more down events while any touchpad is still down, update lastTouch
    // once we get an up event and state == 0 (no touchpads down), clear touchDown, save the position(?)
    // swipe direction = (lastTouch > firstTouch) ? DOWN : UP
    // TODO flip this if it's backwards
    if (evt->down && evt->state != 0 && !paintState->touchDown)
    {
        paintState->touchDown = true;
        paintState->firstTouch = evt->position;
        paintState->lastTouch = evt->position;
    }
    else if (evt->down && evt->state != 0 && paintState->touchDown) {
        paintState->lastTouch = evt->position;
    }
    else if (!evt->down && evt->state == 0 && paintState->touchDown)
    {
        if (paintState->firstTouch < paintState->lastTouch)
        {
            PAINT_LOGD("Swipe RIGHT (%d)", (paintState->lastTouch - paintState->firstTouch) / 32);
            paintDecBrushWidth((paintState->lastTouch - paintState->firstTouch) / 32);
        }
        else if (paintState->firstTouch > paintState->lastTouch)
        {
            PAINT_LOGD("Swipe LEFT (%d)", (paintState->firstTouch - paintState->lastTouch + 1) / 32);
            paintIncBrushWidth((paintState->firstTouch - paintState->lastTouch + 1) / 32);
        }
        else
        {
            // TAP
            PAINT_LOGD("Tap pad %d", evt->pad);
            if (evt->pad == 0)
            {
                // Tap X (?)
                paintIncBrushWidth(1);
            }
            else if (evt->pad == 4)
            {
                // Tap Y (?)
                paintDecBrushWidth(1);
            }
        }
        paintState->touchDown = false;
    }
}

void paintDrawScreenButtonCb(const buttonEvt_t* evt)
{
    if (paintHelp != NULL)
    {
        paintHelp->allButtons |= evt->state;
        paintHelp->curButtons = evt->state;
        paintHelp->lastButton = evt->button;
        paintHelp->lastButtonDown = evt->down;
    }

    switch (paintState->buttonMode)
    {
        case BTN_MODE_DRAW:
        {
            paintDrawModeButtonCb(evt);
            break;
        }

        case BTN_MODE_SELECT:
        {
            paintSelectModeButtonCb(evt);
            break;
        }

        case BTN_MODE_SAVE:
        {
            paintSaveModeButtonCb(evt);
            break;
        }

        case BTN_MODE_PALETTE:
        {
            paintPaletteModeButtonCb(evt);
            break;
        }
    }

    if (paintHelp != NULL)
    {
        if (paintTutorialCheckTriggers())
        {
            paintState->redrawToolbar = true;
            if (paintHelp->curHelp != lastHelp)
            {
                paintHelp->curHelp++;
                paintHelp->allButtons = 0;
                paintHelp->curButtons = 0;
                paintHelp->lastButton = 0;
                paintHelp->lastButtonDown = false;
            }
        }
    }
}

void paintDrawModeButtonCb(const buttonEvt_t* evt)
{
    if (evt->down)
    {
        // Draw mode buttons
        switch (evt->button)
        {
            case SELECT:
            {
                // Enter select mode (change color / brush)
                paintState->buttonMode = BTN_MODE_SELECT;
                paintState->redrawToolbar = true;
                paintState->aHeld = false;
                paintState->moveX = 0;
                paintState->moveY = 0;
                paintState->btnHoldTime = 0;
                break;
            }

            case BTN_A:
            {
                // Draw
                paintState->aHeld = true;
                break;
            }

            case BTN_B:
            {
                // Swap the foreground and background colors
                paintSwapFgBgColors();

                paintState->redrawToolbar = true;
                paintState->recolorPickPoints = true;
                break;
            }

            case UP:
            {
                paintState->firstMove = true;
                paintState->moveY = -1;
                break;
            }

            case DOWN:
            {
                paintState->firstMove = true;
                paintState->moveY = 1;
                break;
            }

            case LEFT:
            {
                paintState->firstMove = true;
                paintState->moveX = -1;
                break;
            }

            case RIGHT:
            {
                paintState->firstMove = true;
                paintState->moveX = 1;
                break;
            }

            case START:
            // Don't do anything until start is released to avoid conflicting with EXIT
            break;
        }
    }
    else
    {
        //////// Draw mode button release
        switch (evt->button)
        {
            case START:
            {
                if (!paintState->saveInProgress)
                {
                    // Enter the save menu
                    paintState->buttonMode = BTN_MODE_SAVE;
                    paintState->saveMenu = PICK_SLOT_SAVE;
                    paintState->redrawToolbar = true;

                    // Don't let the cursor keep moving
                    paintState->moveX = 0;
                    paintState->moveY = 0;
                    paintState->btnHoldTime = 0;
                    paintState->aHeld = false;
                }
                break;
            }

            case BTN_A:
            {
                // Stop drawing
                paintState->aHeld = false;
                break;
            }

            case BTN_B:
            // Do nothing; color swap is handled on button down
            break;

            case UP:
            case DOWN:
            {
                // Stop moving vertically
                paintState->moveY = 0;

                // Reset the button hold time, but only if we're not holding another direction
                // This lets you make turns quickly instead of waiting for the repeat timeout in the middle
                if (!paintState->moveX)
                {
                    paintState->btnHoldTime = 0;
                }
                break;
            }

            case LEFT:
            case RIGHT:
            {
                // Stop moving horizontally
                paintState->moveX = 0;

                if (!paintState->moveY)
                {
                    paintState->btnHoldTime = 0;
                }
                break;
            }

            case SELECT:
            {
                paintState->saveMenuBoolOption = false;
                // TODO: Why did this work? I'm pretty sure this should be BTN_MODE_SELECT
                paintState->buttonMode = BTN_MODE_SAVE;
                paintState->redrawToolbar = true;
            }
            break;
        }
    }
}

void paintDoTool(uint16_t x, uint16_t y, paletteColor_t col)
{
    hideCursor(getCursor(), &paintState->canvas);
    bool drawNow = false;
    bool isLastPick = false;

    // Determine if this is the last pick for the tool
    // This is so we don't draw a pick-marker that will be immediately removed
    switch (getArtist()->brushDef->mode)
    {
        case PICK_POINT:
        isLastPick = (pxStackSize(&getArtist()->pickPoints) + 1 == getArtist()->brushDef->maxPoints);
        break;

        case PICK_POINT_LOOP:
        isLastPick = pxStackSize(&getArtist()->pickPoints) + 1 == getArtist()->brushDef->maxPoints - 1;
        break;

        case HOLD_DRAW:
        break;

        default:
        break;
    }

    pushPxScaled(&getArtist()->pickPoints, paintState->disp, getCursor()->x, getCursor()->y, paintState->canvas.x, paintState->canvas.y, paintState->canvas.xScale, paintState->canvas.yScale);

    if (getArtist()->brushDef->mode == HOLD_DRAW)
    {
        drawNow = true;
    }
    else if (getArtist()->brushDef->mode == PICK_POINT || getArtist()->brushDef->mode == PICK_POINT_LOOP)
    {
        // Save the pixel underneath the selection, then draw a temporary pixel to mark it
        // But don't bother if this is the last pick point, since it will never actually be seen

        if (getArtist()->brushDef->mode == PICK_POINT_LOOP)
        {
            pxVal_t firstPick, lastPick;
            if (pxStackSize(&getArtist()->pickPoints) > 1 && getPx(&getArtist()->pickPoints, 0, &firstPick) && peekPx(&getArtist()->pickPoints, &lastPick) && firstPick.x == lastPick.x && firstPick.y == lastPick.y)
            {
                // If this isn't the first pick, and it's in the same position as the first pick, we're done!
                drawNow = true;
            }
            else if (isLastPick)
            {
                // Special case: If we're on the next-to-last possible point, we have to add the start again as the last point
                pushPx(&getArtist()->pickPoints, paintState->disp, firstPick.x, firstPick.y);

                drawNow = true;
            }
        }
        // only for non-loop brushes
        else if (pxStackSize(&getArtist()->pickPoints) == getArtist()->brushDef->maxPoints)
        {
            drawNow = true;
        }
    }

    if (drawNow)
    {
        // Allocate an array of point_t for the canvas pick points
        size_t pickCount = pxStackSize(&getArtist()->pickPoints);
        point_t* canvasPickPoints = malloc(sizeof(point_t) * pickCount);

        // Convert the pick points into an array of canvas-coordinates
        paintConvertPickPointsScaled(&getArtist()->pickPoints, &paintState->canvas, canvasPickPoints);

        while (popPxScaled(&getArtist()->pickPoints, paintState->disp, paintState->canvas.xScale, paintState->canvas.yScale));

        paintState->unsaved = true;
        getArtist()->brushDef->fnDraw(&paintState->canvas, canvasPickPoints, pickCount, getArtist()->brushWidth, col);

        free(canvasPickPoints);
    }
    else
    {
        // A bit counterintuitively, this will restart the blink timer on the next frame
        paintState->blinkTimer = BLINK_TIME_OFF;
        paintState->blinkOn = false;
    }

    showCursor(getCursor(), &paintState->canvas);
    paintRenderToolbar(getArtist(), &paintState->canvas, paintState, firstBrush, lastBrush);
}


void paintSetupTool(void)
{
    // Reset the brush params
    if (getArtist()->brushWidth < getArtist()->brushDef->minSize)
    {
        getArtist()->brushWidth = getArtist()->brushDef->minSize;
    }
    else if (getArtist()->brushWidth > getArtist()->brushDef->maxSize)
    {
        getArtist()->brushWidth = getArtist()->brushDef->maxSize;
    }

    hideCursor(getCursor(), &paintState->canvas);
    switch (getArtist()->brushDef->mode)
    {
        case HOLD_DRAW:
            setCursorSprite(getCursor(), &paintState->canvas, &paintState->cursorWsg);
            setCursorOffset(getCursor(), (paintState->canvas.xScale - paintState->cursorWsg.w) / 2, (paintState->canvas.yScale - paintState->cursorWsg.h) / 2);

        break;

        case PICK_POINT:
        case PICK_POINT_LOOP:
            setCursorSprite(getCursor(), &paintState->canvas, &paintState->picksWsg);
            setCursorOffset(getCursor(), -paintState->picksWsg.w, paintState->canvas.yScale);
        break;
    }
    showCursor(getCursor(), &paintState->canvas);

    // Undraw and hide any stored temporary pixels
    while (popPxScaled(&getArtist()->pickPoints, paintState->disp, paintState->canvas.xScale, paintState->canvas.yScale));
}

void paintPrevTool(void)
{
    if (getArtist()->brushDef == firstBrush)
    {
        getArtist()->brushDef = lastBrush;
    }
    else
    {
        getArtist()->brushDef--;
    }

    paintSetupTool();
}

void paintNextTool(void)
{
    if (getArtist()->brushDef == lastBrush)
    {
        getArtist()->brushDef = firstBrush;
    }
    else
    {
        getArtist()->brushDef++;
    }

    paintSetupTool();
}

void paintDecBrushWidth(uint8_t dec)
{
    if (getArtist()->brushWidth <= dec || getArtist()->brushWidth <= getArtist()->brushDef->minSize)
    {
        getArtist()->brushWidth = getArtist()->brushDef->minSize;
    }
    else
    {
        getArtist()->brushWidth -= dec;
    }
    paintState->redrawToolbar = true;
}

void paintIncBrushWidth(uint8_t inc)
{
    getArtist()->brushWidth += inc;

    if (getArtist()->brushWidth > getArtist()->brushDef->maxSize)
    {
        getArtist()->brushWidth = getArtist()->brushDef->maxSize;
    }
    paintState->redrawToolbar = true;
}

void paintSwapFgBgColors(void)
{
    uint8_t fgIndex = 0, bgIndex = 0;
    swap(&getArtist()->fgColor, &getArtist()->bgColor);

    for (uint8_t i = 0; i < PAINT_MAX_COLORS; i++)
    {
        if (paintState->canvas.palette[i] == getArtist()->fgColor)
        {
            fgIndex = i;
        }
        else if (paintState->canvas.palette[i] == getArtist()->bgColor)
        {
            bgIndex = i;
        }
    }

    for (uint8_t i = fgIndex; i > 0; i--)
    {
        if (i == bgIndex)
        {
            continue;
        }
        paintState->canvas.palette[i] = paintState->canvas.palette[i - 1 + ((i < bgIndex) ? 1 : 0)];
    }

    paintState->canvas.palette[0] = getArtist()->fgColor;

    paintUpdateLeds();
    paintDrawPickPoints();
}

void paintUpdateRecents(uint8_t selectedIndex)
{
    getArtist()->fgColor = paintState->canvas.palette[selectedIndex];

    for (uint8_t i = selectedIndex; i > 0; i--)
    {
        paintState->canvas.palette[i] = paintState->canvas.palette[i-1];
    }
    paintState->canvas.palette[0] = getArtist()->fgColor;

    paintUpdateLeds();

    // If there are any pick points, update their color to reduce confusion
    paintDrawPickPoints();
}

void paintUpdateLeds(void)
{
    uint32_t rgb = 0;

    // Only set the LED color if LEDs are enabled
    if (paintState->index & PAINT_ENABLE_LEDS)
    {
        if (paintState->buttonMode == BTN_MODE_PALETTE)
        {
            // Show the edited color if we're editing the palette
            rgb = paletteToRGB(paintState->newColor);
        }
        else if (paintState->buttonMode == BTN_MODE_SELECT)
        {
            // Show the selected color if we're picking colors
            rgb = paletteToRGB(paintState->canvas.palette[paintState->paletteSelect]);
        }
        else
        {
            // Otherwise, use the current draw color
            rgb = paletteToRGB(getArtist()->fgColor);
        }
    }

    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        paintState->leds[i].b = (rgb >>  0) & 0xFF;
        paintState->leds[i].g = (rgb >>  8) & 0xFF;
        paintState->leds[i].r = (rgb >> 16) & 0xFF;
    }

    setLeds(paintState->leds, NUM_LEDS);
}

void paintDrawPickPoints(void)
{
    pxVal_t point;
    for (size_t i = 0; i < pxStackSize(&getArtist()->pickPoints); i++)
    {
        if (getPx(&getArtist()->pickPoints, i, &point))
        {
            plotRectFilled(paintState->disp, point.x, point.y, point.x + paintState->canvas.xScale + 1, point.y + paintState->canvas.yScale + 1, getArtist()->fgColor);
        }
    }
}

void paintHidePickPoints(void)
{
    pxVal_t point;
    for (size_t i = 0; i < pxStackSize(&getArtist()->pickPoints); i++)
    {
        if (getPx(&getArtist()->pickPoints, i, &point))
        {
            plotRectFilled(paintState->disp, point.x, point.y, point.x + paintState->canvas.xScale + 1, point.y + paintState->canvas.yScale + 1, point.col);
        }
    }
}

paintArtist_t* getArtist(void)
{
    // TODO: Take player order into account
    return paintState->artist;
}

paintCursor_t* getCursor(void)
{
    // TODO Take player order into account
    return &paintState->artist->cursor;
}
