#pragma once

// Game states for the engine state machine.
enum class GameState {
    MAIN_MENU,    // Main menu screen
    WORLD_MENU,   // World selection / creation screen
    PLAYING,      // In-game (survival)
    CREATIVE,     // In-game (creative mode - fly, infinite blocks)
    PAUSED,       // Paused in-game menu
};
