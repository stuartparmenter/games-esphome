// Pong game implementation
// Ported from: apollo-m1/packages/pages/pong.yaml
// Features simple AI that tracks the ball with random error

#pragma once

#include "esphome/components/lvgl_game_runner/game_base.h"
#include "esphome/components/lvgl_game_runner/game_state.h"
#include <cstdint>
#include <memory>

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

  // Pong supports 2 players
  uint8_t get_max_players() const override { return 2; }

  // Accessor methods for AI
  const Rect &get_area() const { return area_; }
  float get_ball_x() const { return ball_x_; }
  float get_ball_y() const { return ball_y_; }
  float get_ball_vx() const { return vx_; }
  float get_ball_vy() const { return vy_; }
  int get_ball_w() const { return ball_w_; }
  int get_ball_h() const { return ball_h_; }
  int get_paddle_h() const { return paddle_h_; }
  float get_left_paddle_y() const { return left_y_; }
  float get_right_paddle_y() const { return right_y_; }

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
  float player_speed_{2.5f};

  // Player speed scaling
  static constexpr float PLAYER_SPEED_RATIO = 0.030f;  // Player paddle speed as ratio of canvas height

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
  bool last_ball_over_score_{false};

  // Player control (player 1 = left, player 2 = right)
  bool input_p1_up_held_{false};    // Player 1 UP button state
  bool input_p1_down_held_{false};  // Player 1 DOWN button state
  bool input_p2_up_held_{false};    // Player 2 UP button state
  bool input_p2_down_held_{false};  // Player 2 DOWN button state

  // AI controllers (managed by game, created when needed)
  std::unique_ptr<class PongAI> ai_player1_;
  std::unique_ptr<class PongAI> ai_player2_;

  // Serve mechanics
  int serve_idx_;
  static constexpr float SERVE_ANGLES[6] = {-1.0f, -0.6f, -0.3f, 0.3f, 0.6f, 1.0f};

  // Colors
  lv_color_t color_fg_;
  lv_color_t color_bg_;

  // Game logic helpers
  void reset_ball_();
  void serve_ball_();
  void update_ai_();  // Update AI controllers and inject their inputs
  bool check_paddle_collision_(float ball_top, float ball_bottom, float paddle_y);

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
