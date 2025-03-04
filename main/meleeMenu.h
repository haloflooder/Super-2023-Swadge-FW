#ifndef _MELEE_MENU_H_
#define _MELEE_MENU_H_

//==============================================================================
// Includes
//==============================================================================

#include "display.h"
#include "btn.h"

//==============================================================================
// Defines
//==============================================================================

#define MAX_ROWS 6

//==============================================================================
// Typedefs
//==============================================================================

typedef void (*meleeMenuCb)(const char*);

typedef struct
{
    const char* rows[MAX_ROWS];
    const char* title;
    font_t* font;
    meleeMenuCb cbFunc;
    uint8_t numRows;
    uint8_t selectedRow;
    uint8_t allowLEDControl;
} meleeMenu_t;

//==============================================================================
// Function Prototypes
//==============================================================================

meleeMenu_t* initMeleeMenu(const char* title, font_t* font, meleeMenuCb cbFunc);
void resetMeleeMenu(meleeMenu_t* menu, const char* title, meleeMenuCb cbFunc);
int addRowToMeleeMenu(meleeMenu_t* menu, const char* label);
void deinitMeleeMenu(meleeMenu_t* menu);
void drawMeleeMenu(display_t* d, meleeMenu_t* menu);
void meleeMenuButton(meleeMenu_t* menu, buttonBit_t btn);
void drawBackgroundGrid(display_t * d);

#endif
