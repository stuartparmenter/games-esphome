// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#pragma once

#include "esphome/components/lvgl_game_runner/ai_controller.h"
#include "game_pong.h"
#include <cstdint>

namespace esphome::game_pong {

using lvgl_game_runner::AIController;
using lvgl_game_runner::GameBase;
using lvgl_game_runner::GameState;
using lvgl_game_runner::InputEvent;
using lvgl_game_runner::InputType;

/**
 * Simple AI controller for Pong.
 * Tracks the ball when it's moving toward the paddle, otherwise returns to center.
 * Includes random error to make gameplay interesting.
 */
class PongAI : public AIController {
 public:
  explicit PongAI(uint8_t player_num);
  ~PongAI() override = default;

  InputEvent update(float dt, const GameState &state, const GameBase *game) override;
  void reset() override;

 private:
  // Simple AI configuration
  static constexpr float TRACKING_THRESHOLD = 8.0f;  // Deadband to prevent oscillation (pixels)
  static constexpr float RANDOM_ERROR = 0.10f;        // Random error factor (0-1)

  // Current input state (only one can be active at a time)
  enum class InputState { NONE, UP, DOWN };
  InputState current_input_{InputState::NONE};

  // Random error offset (changes occasionally to simulate imperfect tracking)
  float error_offset_{0.0f};
  int offset_update_counter_{0};

  // PRNG state (xorshift32)
  uint32_t rng_state_;

  // Helper methods
  uint32_t rng_();
  float rand01_();
  float rand_range_(float min, float max);
};

}  // namespace esphome::game_pong
