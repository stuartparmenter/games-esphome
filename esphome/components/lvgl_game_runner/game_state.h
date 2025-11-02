// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace esphome::lvgl_game_runner {

/**
 * Generic game state utilities.
 * Header-only for simplicity.
 */

/**
 * Simple score tracker with optional level/lives.
 */
struct GameState {
  uint32_t score{0};
  uint8_t level{1};
  uint8_t lives{3};
  bool game_over{false};

  void reset() {
    score = 0;
    level = 1;
    lives = 3;
    game_over = false;
  }

  void add_score(uint32_t points) { score += points; }

  void lose_life() {
    if (lives > 0) {
      lives--;
      if (lives == 0)
        game_over = true;
    }
  }

  void gain_life(uint8_t max_lives = 9) {
    if (lives < max_lives)
      lives++;
  }

  void next_level() { level++; }
};

}  // namespace esphome::lvgl_game_runner
