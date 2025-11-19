// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#include "pong_ai.h"
#include <cmath>

namespace esphome::game_pong {

PongAI::PongAI(uint8_t player_num) : AIController(player_num), rng_state_(2463534242 + player_num * 12345) { reset(); }

void PongAI::reset() {
  current_input_ = InputState::NONE;
  error_offset_ = 0.0f;
  offset_update_counter_ = 0;
}

InputEvent PongAI::update(float dt, const GameState &state, const GameBase *game) {
  // Null event (no action)
  InputEvent null_event(InputType::NONE, player_num_, false, 0);

  // Cast to GamePong to access game-specific data
  const GamePong *pong = static_cast<const GamePong *>(game);
  if (!pong)
    return null_event;

  // Get game state
  const auto &area = pong->get_area();
  float ball_x = pong->get_ball_x();
  float ball_y = pong->get_ball_y();
  float ball_vx = pong->get_ball_vx();
  int ball_w = pong->get_ball_w();
  int ball_h = pong->get_ball_h();
  int paddle_h = pong->get_paddle_h();

  // Get our paddle position
  float paddle_y = (player_num_ == 1) ? pong->get_left_paddle_y() : pong->get_right_paddle_y();

  // Determine if ball is moving toward this paddle
  bool is_left_paddle = (player_num_ == 1);
  bool ball_moving_toward_us = is_left_paddle ? (ball_vx < 0) : (ball_vx > 0);

  // Calculate target position
  float target_y;
  if (ball_moving_toward_us) {
    // Ball coming toward us: track the ball with some random error
    // Update random error offset occasionally (every ~20 frames)
    offset_update_counter_++;
    if (offset_update_counter_ >= 20) {
      offset_update_counter_ = 0;
      // Random error: percentage of paddle height
      error_offset_ = rand_range_(-paddle_h * RANDOM_ERROR, paddle_h * RANDOM_ERROR);
    }

    float ball_center_y = ball_y + ball_h / 2.0f;
    target_y = ball_center_y + error_offset_;
  } else {
    // Ball moving away: return to vertical center (no random error)
    target_y = area.h / 2.0f;
    // Reset error offset for next rally
    offset_update_counter_ = 0;
    error_offset_ = 0.0f;
  }

  // Check if target position is well-centered within paddle (not just barely touching edge)
  // We want the paddle body to hit the ball, not just the corner
  // Add margin from edges to ensure good contact
  float paddle_top = paddle_y;
  float paddle_bottom = paddle_y + paddle_h;
  float safety_margin = paddle_h * 0.15f;  // 15% margin from each edge

  bool target_in_range = (target_y >= paddle_top + safety_margin) &&
                         (target_y <= paddle_bottom - safety_margin);

  // Determine desired input state
  InputState desired_state = InputState::NONE;
  if (!target_in_range) {
    // Target is outside paddle range - move toward it
    float paddle_center_y = paddle_y + paddle_h / 2.0f;
    if (target_y < paddle_center_y) {
      desired_state = InputState::UP;
    } else {
      desired_state = InputState::DOWN;
    }
  }

  // Generate event to transition from current_input_ to desired_state
  if (desired_state != current_input_) {
    // Priority: release old button first, then press new button in next update
    if (current_input_ == InputState::UP) {
      // Need to release UP
      current_input_ = InputState::NONE;
      return InputEvent(InputType::UP, player_num_, false, 0);
    } else if (current_input_ == InputState::DOWN) {
      // Need to release DOWN
      current_input_ = InputState::NONE;
      return InputEvent(InputType::DOWN, player_num_, false, 0);
    } else if (desired_state == InputState::UP) {
      // Press UP
      current_input_ = InputState::UP;
      return InputEvent(InputType::UP, player_num_, true, 0);
    } else if (desired_state == InputState::DOWN) {
      // Press DOWN
      current_input_ = InputState::DOWN;
      return InputEvent(InputType::DOWN, player_num_, true, 0);
    }
  }

  // No state change needed
  return null_event;
}

// ========== PRNG Helpers ==========

uint32_t PongAI::rng_() {
  // xorshift32
  rng_state_ ^= rng_state_ << 13;
  rng_state_ ^= rng_state_ >> 17;
  rng_state_ ^= rng_state_ << 5;
  return rng_state_;
}

float PongAI::rand01_() { return static_cast<float>(rng_()) / static_cast<float>(UINT32_MAX); }

float PongAI::rand_range_(float min, float max) { return min + rand01_() * (max - min); }

}  // namespace esphome::game_pong
