// Breakout game implementation
// Ported from: https://github.com/richrd/esphome-clock-os/tree/main/clockos/packages/games/breakout

#pragma once

#include "esphome/components/lvgl_game_runner/game_base.h"
#include "esphome/components/lvgl_game_runner/game_state.h"
#include <esp_random.h>
#include <vector>
#include <cstdint>

namespace esphome::game_breakout {

using lvgl_game_runner::GameBase;
using lvgl_game_runner::GameState;
using lvgl_game_runner::InputEvent;
using lvgl_game_runner::InputType;

/**
 * Classic Breakout/Arkanoid game.
 * Features multiple brick types, powerups, progressive difficulty, and multi-ball mechanics.
 */
class GameBreakout : public GameBase {
 public:
  GameBreakout();
  ~GameBreakout() override = default;

  void on_bind(lv_obj_t *canvas) override;
  void on_resize(const Rect &r) override;
  void step(float dt) override;
  void on_input(const InputEvent &event) override;
  void reset() override;

 private:
  // Configuration constants
  static constexpr int BALL_SIZE = 3;
  static constexpr int PADDLE_W_INITIAL = 18;
  static constexpr int PADDLE_W_MAX = 44;
  static constexpr int PADDLE_W_INCREASE = 4;
  static constexpr int PADDLE_W_DECREASE = 3;
  static constexpr int PADDLE_H = 3;
  static constexpr int BRICK_W = 15;
  static constexpr int BRICK_H = 7;
  static constexpr int BRICK_MAX_HP = 5;
  static constexpr int PAUSE_DURATION = 100;  // frames at 30fps = ~3 seconds
  static constexpr int POINTS_PER_BRICK = 5;
  static constexpr int POINTS_PER_PADDLE_HIT = 10;
  static constexpr float SPEED_INITIAL = 0.9f;
  static constexpr float SPEED_MAX = 5.0f;
  static constexpr float SPEED_INCREASE_FACTOR = 1.04f;
  static constexpr int LIVES_INITIAL = 3;
  static constexpr int LIVES_MAX = 6;
  static constexpr int MAX_BALLS = 10;
  static constexpr int MAX_PROJECTILES = 8;
  static constexpr int SHOOTER_COOLDOWN_FRAMES = 15;  // ~0.5s at 30fps
  static constexpr int BRICK_COUNT = 48;              // 8 columns x 6 rows

  // Brick types
  enum BrickType {
    NORMAL = 0,
    SHIELD = 1,
    EXTRA_BALL = 2,
    WIDER_PADDLE = 3,
    EXTRA_LIFE = 4,
    WONKY_BRICKS = 5,
    SHOOTER = 6,
    STATIC = 7,
    POWERUP_SHUFFLE = 8,
  };

  // Game data structures
  struct Ball {
    float x;
    float y;
    int direction_x;
    int direction_y;
    bool alive;
  };

  struct Projectile {
    float x;
    float y;
  };

  struct Brick {
    int x;
    int y;
    int hp;
    BrickType type;
  };

  // Game state
  GameState state_;
  int frame_;
  int pause_frames_;
  int paddle_w_;
  float speed_;
  int score_;
  int score_ticker_;
  int level_;
  bool level_started_;
  int shield_amount_;
  int shooter_level_;
  int paddle_x_;
  int paddle_y_;
  bool paddle_hit_;
  Ball balls_[MAX_BALLS];
  std::vector<Projectile> projectiles_;
  Brick bricks_[BRICK_COUNT];

  // Input state
  bool autoplay_;
  float input_position_;  // Simulated knob position (0-50, float for smooth movement)
  bool left_held_;        // Track if left direction is held
  bool right_held_;       // Track if right direction is held

  // Colors
  lv_color_t color_on_;
  lv_color_t color_off_;

  // Game logic helpers
  void clear_bricks_();
  void clear_projectiles_();
  void reset_game_();
  bool is_level_cleared_();
  void place_ball_on_paddle_();
  void reset_balls_();
  void setup_next_level_();
  void add_new_ball_();
  void shoot_projectile_();
  void on_brick_hit_(int brick_id);
  void update_projectiles_();
  bool any_balls_alive_();
  void align_brick_positions_();
  void randomise_brick_positions_();

  // Rendering helpers
  void render_();
  void draw_heart_(int x, int y);
  void draw_lives_left_();
  void draw_score_();
  void draw_level_();
  void draw_special_brick_corners_(int x, int y);
  void draw_unbreakable_brick_(int x, int y);
  void draw_bricks_();
  void draw_paddle_();
  void draw_shield_();
  void draw_projectiles_();
  void draw_balls_();
  void draw_overlay_text_();
};

}  // namespace esphome::game_breakout
