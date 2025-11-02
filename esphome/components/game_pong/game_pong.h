// Pong game implementation
// Ported from: apollo-m1/packages/pages/pong.yaml
// Features human-like AI with reaction delays, acceleration, jitter, and intentional misses

#pragma once

#include "esphome/components/lvgl_game_runner/game_base.h"
#include "esphome/components/lvgl_game_runner/game_state.h"
#include <cstdint>

namespace esphome::game_pong {

using lvgl_game_runner::GameBase;
using lvgl_game_runner::GameState;
using lvgl_game_runner::InputEvent;
using lvgl_game_runner::InputType;

/**
 * Classic Pong game with sophisticated AI.
 * Features human-like AI with reaction delays, acceleration, jitter, panic boost, and intentional misses.
 */
class GamePong : public GameBase {
 public:
  GamePong();
  ~GamePong() override = default;

  void on_bind(lv_obj_t *canvas) override;
  void on_resize(const Rect &r) override;
  void step(float dt) override;
  void on_input(const InputEvent &event) override;
  void reset() override;

 private:
  // Configuration constants
  static constexpr int PADDLE_W = 3;
  static constexpr int PADDLE_H = 12;
  static constexpr int PADDLE_MARGIN_X = 2;
  static constexpr int BALL_W = 4;
  static constexpr int BALL_H = 4;
  static constexpr float BALL_SPEED_X = 1.30f;
  static constexpr float BALL_SPEED_Y = 0.90f;

  // Human-like AI configuration
  static constexpr int AI_L_REACT_MS = 120;       // Left paddle reaction delay
  static constexpr int AI_R_REACT_MS = 80;        // Right paddle reaction delay
  static constexpr float AI_L_MAX_SPEED = 2.0f;   // Left paddle max speed
  static constexpr float AI_R_MAX_SPEED = 1.3f;   // Right paddle max speed
  static constexpr float AI_ACCEL = 0.25f;        // Acceleration rate
  static constexpr float AI_ERR_GAIN = 0.18f;     // Error multiplier for tracking
  static constexpr float AI_JITTER_PX = 1.2f;     // Random noise in tracking
  static constexpr int AI_PANIC_ZONE = 10;        // Horizontal distance to trigger panic
  static constexpr float AI_PANIC_MULT = 1.7f;    // Speed multiplier in panic zone
  static constexpr float AI_BIAS_LEFT = -1.0f;    // Constant tracking bias for left
  static constexpr float AI_BIAS_RIGHT = 1.0f;    // Constant tracking bias for right
  static constexpr float AI_MISS_CHANCE = 0.06f;  // 6% chance of intentional miss

  // Game state
  GameState state_;
  bool scored_;
  bool last_scored_right_;
  int score_left_;
  int score_right_;

  // Ball state
  float ball_x_;
  float ball_y_;
  float vx_;
  float vy_;

  // Paddle state
  float left_y_;
  float right_y_;
  float left_vy_;
  float right_vy_;

  // Reaction timers (frames remaining before paddle reacts)
  int left_react_frames_;
  int right_react_frames_;

  // Serve mechanics
  int serve_idx_;
  static constexpr float SERVE_ANGLES[6] = {-1.0f, -0.6f, -0.3f, 0.3f, 0.6f, 1.0f};

  // Intentional miss flags (per rally)
  bool will_miss_left_;
  bool will_miss_right_;

  // PRNG state (xorshift32)
  uint32_t rng_state_;

  // Colors
  lv_color_t color_fg_;
  lv_color_t color_bg_;

  // Game logic helpers
  void reset_ball_();
  void serve_ball_();
  void update_paddle_(bool is_left, float &top_y, float &vy, int &react_frames, float bias, float max_speed_base);
  bool check_paddle_collision_(float ball_top, float ball_bottom, float paddle_y);
  uint32_t rng_();
  float rand01_();
  float rand_sym_();  // Returns value in [-1, 1]
  int ms_to_frames_(int ms);

  // Rendering helpers
  void render_();
  void draw_paddle_(int x, int y);
  void draw_ball_();
  void draw_score_();
};

}  // namespace esphome::game_pong
