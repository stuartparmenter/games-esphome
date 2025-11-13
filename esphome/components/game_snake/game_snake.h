// Snake game implementation
// Inspired by: https://github.com/richrd/esphome-clock-os/tree/main/clockos/packages/games/snake

#pragma once

#include <cstdint>
#include <vector>
#include "esphome/components/lvgl_game_runner/game_base.h"
#include "esphome/components/lvgl_game_runner/game_state.h"

namespace esphome::game_snake {

using lvgl_game_runner::GameBase;
using lvgl_game_runner::GameState;
using lvgl_game_runner::InputEvent;
using lvgl_game_runner::InputType;

/**
 * Classic Snake game.
 * Grid-based gameplay with configurable grid size (scales to canvas).
 */
class GameSnake : public GameBase {
 public:
  GameSnake();
  ~GameSnake() override = default;

  void on_bind(lv_obj_t *canvas) override;
  void on_resize(const Rect &r) override;
  void step(float dt) override;
  void on_input(const InputEvent &event) override;
  void reset() override;

 private:
  // Grid configuration
  static constexpr int GRID_COLS = 25;
  static constexpr int GRID_ROWS = 11;

  // Game state
  struct Position {
    int x;
    int y;
    bool operator==(const Position &other) const { return x == other.x && y == other.y; }
  };

  static constexpr Position NULL_POSITION = {-1, -1};

  enum class Direction { UP, DOWN, LEFT, RIGHT };

  std::vector<Position> snake_;
  Position snake_tail_{NULL_POSITION};
  Position pickup_;
  Position last_pickup_{NULL_POSITION};

  Direction direction_{Direction::RIGHT};
  Direction next_direction_{Direction::RIGHT};
  GameState state_;
  bool initial_render_{true};
  bool needs_render_{true};
  uint32_t last_drawn_score_{0};

  // Timing
  float update_timer_{0.0f};
  float update_interval_{0.15f};  // seconds per move

  // Config
  bool walls_enabled_{true};
  bool autoplay_{false};

  // Rendering
  int cell_width_{1};
  int cell_height_{1};
  lv_color_t color_snake_;
  lv_color_t color_pickup_;
  lv_color_t color_bg_;
  lv_color_t color_border_;

  // Game logic
  void move_snake_();
  void spawn_pickup_();
  bool check_collision_(const Position &pos);
  bool check_self_collision_(const Position &pos);
  Direction get_autoplay_direction_();

  // Rendering
  void render_();
  void draw_cell_(int gx, int gy, lv_color_t color);
  void draw_border_();
  void draw_score_();
};

}  // namespace esphome::game_snake
