// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#pragma once

#include "input_types.h"
#include "game_state.h"

namespace esphome::lvgl_game_runner {

class GameBase;

/**
 * Base class for AI controllers.
 * Each game implements its own AI by subclassing this.
 *
 * AI controllers receive game state updates and can inject input events
 * to control their assigned player(s).
 */
class AIController {
 public:
  explicit AIController(uint8_t player_num) : player_num_(player_num) {}
  virtual ~AIController() = default;

  /**
   * Called each frame to update AI logic.
   * AI can examine game state and return an input event.
   *
   * @param dt Delta time since last update (seconds)
   * @param state Current game state
   * @param game Pointer to game instance for AI to read game-specific data
   * @return Input event to inject this frame, or InputEvent with type NONE to do nothing
   */
  virtual InputEvent update(float dt, const GameState &state, const GameBase *game) = 0;

  /**
   * Called when game resets.
   * AI can reset internal state here.
   */
  virtual void reset() {}

  uint8_t get_player_num() const { return player_num_; }

 protected:
  uint8_t player_num_;  // Which player this AI controls (1-4)
};

}  // namespace esphome::lvgl_game_runner
