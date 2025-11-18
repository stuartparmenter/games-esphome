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
  // Configuration constants (base values, will be scaled dynamically)
  // Paddle height is the primary scaling unit: canvas height / 8
  static constexpr int PADDLE_HEIGHT_DIVISOR = 8;       // Paddle height = canvas height / this value
  static constexpr float PADDLE_WIDTH_RATIO = 0.25f;    // Paddle width as ratio of paddle height
  static constexpr float BALL_SIZE_RATIO = 0.33f;       // Ball size as ratio of paddle height
  static constexpr float BALL_SPEED_X_RATIO = 0.015f;   // Ball speed as ratio of canvas width per frame
  static constexpr float BALL_SPEED_Y_RATIO = 0.010f;   // Ball speed as ratio of canvas height per frame

  // Dynamic values (calculated in on_resize)
  int paddle_w_{3};
  int paddle_h_{12};
  int paddle_margin_x_{2};
  int ball_w_{4};
  int ball_h_{4};
  float ball_speed_x_{1.30f};
  float ball_speed_y_{0.90f};
  float ai_left_max_speed_{2.0f};
  float ai_right_max_speed_{1.3f};
  float player_speed_{2.5f};

  // Human-like AI configuration
  static constexpr int AI_L_REACT_MS = 120;       // Left paddle reaction delay
  static constexpr int AI_R_REACT_MS = 80;        // Right paddle reaction delay
  static constexpr float AI_L_SPEED_RATIO = 0.040f;   // Left paddle max speed as ratio of canvas height
  static constexpr float AI_R_SPEED_RATIO = 0.035f;   // Right paddle max speed as ratio of canvas height
  static constexpr float PLAYER_SPEED_RATIO = 0.030f; // Player paddle speed as ratio of canvas height
  static constexpr float AI_ACCEL = 0.50f;        // Acceleration rate
  static constexpr float AI_ERR_GAIN = 0.35f;     // Error multiplier for tracking
  static constexpr float AI_JITTER_PX = 0.8f;     // Random noise in tracking
  static constexpr int AI_PANIC_ZONE = 10;        // Horizontal distance to trigger panic
  static constexpr float AI_PANIC_MULT = 1.7f;    // Speed multiplier in panic zone
  static constexpr float AI_BIAS_LEFT = -0.5f;    // Constant tracking bias for left
  static constexpr float AI_BIAS_RIGHT = 0.5f;    // Constant tracking bias for right
  static constexpr float AI_MISS_CHANCE = 0.06f;  // 6% chance of intentional miss

  // Game state
  GameState state_;
  bool scored_;
  bool last_scored_right_;
  int score_left_;
  int score_right_;
  bool initial_render_{true};
  bool needs_render_{true};
  uint32_t last_drawn_score_left_{0};
  uint32_t last_drawn_score_right_{0};
  bool last_paused_{false};

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

  // Previous positions for incremental rendering
  int last_ball_x_{-1};
  int last_ball_y_{-1};
  int last_left_y_{-1};
  int last_right_y_{-1};

  // Player control
  bool player_control_{true};  // If true, left paddle is player-controlled
  float player_target_vy_{0.0f};  // Player's desired paddle velocity
  bool input_up_held_{false};    // True while UP button is held
  bool input_down_held_{false};  // True while DOWN button is held

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
  void draw_paddle_fast_(int x, int y);
  void erase_paddle_fast_(int x, int y);
  void draw_ball_();
  void draw_ball_fast_();
  void erase_ball_fast_(int x, int y);
  void draw_score_();
  void clear_score_area_fast_();
  void clear_center_text_area_();
};

}  // namespace esphome::game_pong
