#ifndef _MODE_PLATFORMER_H_
#define _MODE_PLATFORMER_H_
//==============================================================================
// Includes
//==============================================================================
#include "common_typedef.h"


#define NUM_PLATFORMER_HIGH_SCORES 5
//==============================================================================
// Structs
//==============================================================================

typedef struct {
    uint32_t scores[NUM_PLATFORMER_HIGH_SCORES];
    char initials[NUM_PLATFORMER_HIGH_SCORES][3];
} platformerHighScores_t;

//==============================================================================
// Prototypes
//==============================================================================

void updateGame(platformer_t *platformer);
void updateTitleScreen(platformer_t *platformer);

extern swadgeMode modePlatformer;

#endif