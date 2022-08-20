//==============================================================================
// Includes
//==============================================================================

#include <stdlib.h>
#include "gameData.h"

//==============================================================================
// Constants
//==============================================================================

//==============================================================================
// Functions
//==============================================================================
 void initializeGameData(gameData_t * gameData){
    gameData->gameState = 0;
    gameData->btnState = 0;
    gameData->score = 0;
    gameData->lives = 0;
    gameData->countdown = 100;
}