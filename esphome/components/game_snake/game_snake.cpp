// Snake game implementation
// Inspired by: https://github.com/richrd/esphome-clock-os/tree/main/clockos/packages/games/snake

#include "game_snake.h"
#include <algorithm>
#include <cstdlib>
#include "esphome/core/log.h"

namespace esphome::game_snake {

static const char *const TAG = "game.snake";

GameSnake::GameSnake() {
  // Initialize colors (will be set properly in on_bind)
  color_snake_ = lv_color_hex(0x00FF00);   // Green
  color_pickup_ = lv_color_hex(0xFF0000);  // Red
  color_bg_ = lv_color_hex(0x000000);      // Black
  color_border_ = lv_color_hex(0x404040);  // Dark gray
}

void GameSnake::on_bind(lv_obj_t *canvas) {
  GameBase::on_bind(canvas);
  ESP_LOGI(TAG, "Snake game bound to canvas");
}

void GameSnake::on_resize(const Rect &r) {
  GameBase::on_resize(r);

  // Calculate grid dynamically based on canvas size
  if (area_.w > 0 && area_.h > 0) {
    // Calculate cell size based on smallest dimension / MIN_GRID_CELLS
    // This ensures cells are perfectly square
    int min_dimension = (area_.w < area_.h) ? area_.w : area_.h;
    int cell_size = min_dimension / MIN_GRID_CELLS;

    // Ensure at least 1 pixel per cell
    if (cell_size < 1)
      cell_size = 1;

    // Both width and height are the same (square cells)
    cell_width_ = cell_size;
    cell_height_ = cell_size;

    // Calculate how many cells fit in each dimension
    grid_cols_ = area_.w / cell_size;
    grid_rows_ = area_.h / cell_size;

    // Calculate centering offset for any remaining pixels
    grid_offset_x_ = (area_.w - (grid_cols_ * cell_size)) / 2;
    grid_offset_y_ = (area_.h - (grid_rows_ * cell_size)) / 2;

    ESP_LOGI(TAG, "Snake grid: %dx%d cells, cell size: %dx%d px, offset: (%d,%d)", grid_cols_, grid_rows_,
             cell_width_, cell_height_, grid_offset_x_, grid_offset_y_);
  }
}

void GameSnake::reset() {
  ESP_LOGI(TAG, "Resetting Snake game");

  // Initialize snake in the center
  snake_.clear();
  snake_.push_back({grid_cols_ / 2, grid_rows_ / 2});
  snake_.push_back({grid_cols_ / 2 - 1, grid_rows_ / 2});
  snake_.push_back({grid_cols_ / 2 - 2, grid_rows_ / 2});

  direction_ = Direction::RIGHT;
  next_direction_ = Direction::RIGHT;

  snake_tail_ = NULL_POSITION;
  last_pickup_ = NULL_POSITION;

  state_.reset();
  update_timer_ = 0.0f;

  initial_render_ = true;
  needs_render_ = true;
  last_drawn_score_ = 0;

  spawn_pickup_();
}

void GameSnake::on_input(const InputEvent &event) {
  if (!event.pressed)
    return;

  if (event.type == InputType::START) {
    // Restart game on START button
    this->reset();
    return;
  }

  if (state_.game_over)
    return;  // Ignore inputs if game over

  // Map input to direction changes
  Direction new_dir = next_direction_;

  switch (event.type) {
    case InputType::UP:
      if (direction_ != Direction::DOWN)
        new_dir = Direction::UP;
      break;
    case InputType::DOWN:
      if (direction_ != Direction::UP)
        new_dir = Direction::DOWN;
      break;
    case InputType::LEFT:
      if (direction_ != Direction::RIGHT)
        new_dir = Direction::LEFT;
      break;
    case InputType::RIGHT:
      if (direction_ != Direction::LEFT)
        new_dir = Direction::RIGHT;
      break;

    case InputType::ROTATE_CW:
      // Rotate direction 90 degrees clockwise
      switch (direction_) {
        case Direction::UP:
          new_dir = Direction::RIGHT;
          break;
        case Direction::RIGHT:
          new_dir = Direction::DOWN;
          break;
        case Direction::DOWN:
          new_dir = Direction::LEFT;
          break;
        case Direction::LEFT:
          new_dir = Direction::UP;
          break;
      }
      break;

    case InputType::ROTATE_CCW:
      // Rotate direction 90 degrees counter-clockwise
      switch (direction_) {
        case Direction::UP:
          new_dir = Direction::LEFT;
          break;
        case Direction::LEFT:
          new_dir = Direction::DOWN;
          break;
        case Direction::DOWN:
          new_dir = Direction::RIGHT;
          break;
        case Direction::RIGHT:
          new_dir = Direction::UP;
          break;
      }
      break;

    default:
      break;
  }

  next_direction_ = new_dir;
}

void GameSnake::step(float dt) {
  if (paused_ || state_.game_over)
    return;

  // Update timer
  update_timer_ += dt;

  // Move snake at fixed intervals
  if (update_timer_ >= update_interval_) {
    update_timer_ -= update_interval_;

    // Apply direction change
    direction_ = next_direction_;

    // Autoplay mode
    if (autoplay_) {
      direction_ = get_autoplay_direction_();
    }

    // Move snake (this will set needs_render_ flag)
    move_snake_();

    // Render all changes in one call
    if (needs_render_) {
      render_();
      needs_render_ = false;
    }
  }
}

void GameSnake::move_snake_() {
  if (snake_.empty())
    return;

  // Calculate new head position
  Position new_head = snake_.front();

  switch (direction_) {
    case Direction::UP:
      new_head.y--;
      break;
    case Direction::DOWN:
      new_head.y++;
      break;
    case Direction::LEFT:
      new_head.x--;
      break;
    case Direction::RIGHT:
      new_head.x++;
      break;
  }

  // Check wall collision
  if (walls_enabled_) {
    if (new_head.x < 0 || new_head.x >= grid_cols_ || new_head.y < 0 || new_head.y >= grid_rows_) {
      state_.game_over = true;
      needs_render_ = true;
      ESP_LOGI(TAG, "Game Over! Hit wall. Final score: %u", state_.score);
      return;
    }
  } else {
    // Wrap around
    if (new_head.x < 0)
      new_head.x = grid_cols_ - 1;
    if (new_head.x >= grid_cols_)
      new_head.x = 0;
    if (new_head.y < 0)
      new_head.y = grid_rows_ - 1;
    if (new_head.y >= grid_rows_)
      new_head.y = 0;
  }

  // Check self collision
  if (check_self_collision_(new_head)) {
    state_.game_over = true;
    needs_render_ = true;
    ESP_LOGI(TAG, "Game Over! Hit self. Final score: %u", state_.score);
    return;
  }

  // Add new head
  snake_.insert(snake_.begin(), new_head);

  // Check if pickup collected
  if (new_head == pickup_) {
    state_.add_score(10);
    spawn_pickup_();
    needs_render_ = true;
    ESP_LOGD(TAG, "Pickup collected! Score: %u", state_.score);

    // When we eat a pickup, we don't remove the tail
    // Set snake_tail_ to NULL to indicate no tail to erase
    snake_tail_ = NULL_POSITION;

    // Speed up slightly
    if (update_interval_ > 0.05f) {
      update_interval_ *= 0.95f;
    }
  } else {
    // Remove tail if no pickup
    snake_tail_ = snake_.back();
    snake_.pop_back();
  }

  // Mark that we need to render the snake movement
  needs_render_ = true;
}

void GameSnake::spawn_pickup_() {
  // Optimized: use modulo to limit attempts based on grid fill ratio
  const int total_cells = grid_cols_ * grid_rows_;
  const int empty_cells = total_cells - static_cast<int>(snake_.size());

  // If grid is nearly full, fall back to first empty cell search
  if (empty_cells < 10) {
    for (int y = 0; y < grid_rows_; y++) {
      for (int x = 0; x < grid_cols_; x++) {
        Position pos = {x, y};
        bool occupied = false;
        for (const auto &part : snake_) {
          if (part == pos) {
            occupied = true;
            break;
          }
        }
        if (!occupied) {
          pickup_ = pos;
          return;
        }
      }
    }
    pickup_ = {0, 0};  // Grid full
    return;
  }

  // Random placement with limited attempts
  const int max_attempts = 20;  // Reduced from 100
  for (int i = 0; i < max_attempts; i++) {
    pickup_.x = rand() % grid_cols_;
    pickup_.y = rand() % grid_rows_;

    // Quick check if position is occupied
    bool occupied = false;
    for (const auto &part : snake_) {
      if (part == pickup_) {
        occupied = true;
        break;
      }
    }

    if (!occupied)
      return;
  }

  // Fallback: find first empty cell
  for (int y = 0; y < grid_rows_; y++) {
    for (int x = 0; x < grid_cols_; x++) {
      Position pos = {x, y};
      bool occupied = false;
      for (const auto &part : snake_) {
        if (part == pos) {
          occupied = true;
          break;
        }
      }
      if (!occupied) {
        pickup_ = pos;
        return;
      }
    }
  }
}

bool GameSnake::check_collision_(const Position &pos) {
  if (walls_enabled_) {
    if (pos.x < 0 || pos.x >= grid_cols_ || pos.y < 0 || pos.y >= grid_rows_)
      return true;
  }
  return check_self_collision_(pos);
}

bool GameSnake::check_self_collision_(const Position &pos) {
  for (const auto &part : snake_) {
    if (part == pos)
      return true;
  }
  return false;
}

GameSnake::Direction GameSnake::get_autoplay_direction_() {
  // Simple AI: move toward pickup while avoiding collisions
  if (snake_.empty())
    return direction_;

  const Position &head = snake_.front();
  int dx = pickup_.x - head.x;
  int dy = pickup_.y - head.y;

  // Try to move in the direction of the pickup
  std::vector<Direction> priorities;

  if (abs(dx) > abs(dy)) {
    // Horizontal movement priority
    if (dx > 0)
      priorities = {Direction::RIGHT, dy > 0 ? Direction::DOWN : Direction::UP, Direction::LEFT};
    else
      priorities = {Direction::LEFT, dy > 0 ? Direction::DOWN : Direction::UP, Direction::RIGHT};
  } else {
    // Vertical movement priority
    if (dy > 0)
      priorities = {Direction::DOWN, dx > 0 ? Direction::RIGHT : Direction::LEFT, Direction::UP};
    else
      priorities = {Direction::UP, dx > 0 ? Direction::RIGHT : Direction::LEFT, Direction::DOWN};
  }

  // Test each direction
  for (Direction dir : priorities) {
    // Can't reverse
    if ((direction_ == Direction::UP && dir == Direction::DOWN) ||
        (direction_ == Direction::DOWN && dir == Direction::UP) ||
        (direction_ == Direction::LEFT && dir == Direction::RIGHT) ||
        (direction_ == Direction::RIGHT && dir == Direction::LEFT))
      continue;

    // Check if this direction is safe
    Position test_pos = head;
    switch (dir) {
      case Direction::UP:
        test_pos.y--;
        break;
      case Direction::DOWN:
        test_pos.y++;
        break;
      case Direction::LEFT:
        test_pos.x--;
        break;
      case Direction::RIGHT:
        test_pos.x++;
        break;
    }

    // Wrap if needed
    if (!walls_enabled_) {
      if (test_pos.x < 0)
        test_pos.x = grid_cols_ - 1;
      if (test_pos.x >= grid_cols_)
        test_pos.x = 0;
      if (test_pos.y < 0)
        test_pos.y = grid_rows_ - 1;
      if (test_pos.y >= grid_rows_)
        test_pos.y = 0;
    }

    if (!check_collision_(test_pos))
      return dir;
  }

  // No safe direction, keep current
  return direction_;
}

void GameSnake::render_() {
  if (!canvas_)
    return;

  if (initial_render_) {
    // Clear canvas
    lv_canvas_fill_bg(canvas_, color_bg_, LV_OPA_COVER);

    // Draw border if walls enabled
    if (walls_enabled_) {
      draw_border_();
    }

    // Draw pickup (use slow path for initial render, it's fine)
    draw_cell_(pickup_.x, pickup_.y, color_pickup_);

    // Draw snake
    for (const auto &part : snake_) {
      draw_cell_(part.x, part.y, color_snake_);
    }

    // Draw initial score
    draw_score_();

    initial_render_ = false;
    last_drawn_score_ = state_.score;
    last_pickup_ = pickup_;

    // Full invalidation for initial render
    lv_obj_invalidate(canvas_);
  } else {
    // Fast incremental update using direct buffer manipulation

    // Draw new snake head
    if (!snake_.empty()) {
      const Position &head = snake_.front();
      draw_cell_fast_(head.x, head.y, color_snake_);
    }

    // Erase tail if it was removed
    if (snake_tail_ != NULL_POSITION) {
      draw_cell_fast_(snake_tail_.x, snake_tail_.y, color_bg_);
    }

    // Redraw pickup if it moved (no need to erase old - snake head is there)
    if (last_pickup_ != pickup_) {
      draw_cell_fast_(pickup_.x, pickup_.y, color_pickup_);
      last_pickup_ = pickup_;
    }

    // Redraw score if it changed or game over
    if (state_.score != last_drawn_score_ || state_.game_over) {
      clear_score_area_fast_();  // Clears and invalidates
      draw_score_();              // Draws text on top (already invalidated)
      last_drawn_score_ = state_.score;
    }
  }
}

void GameSnake::draw_cell_(int gx, int gy, lv_color_t color) {
  const int px = grid_offset_x_ + gx * cell_width_;
  const int py = grid_offset_y_ + gy * cell_height_;

  lv_draw_rect_dsc_t rect_dsc;
  lv_draw_rect_dsc_init(&rect_dsc);
  rect_dsc.bg_color = color;
  rect_dsc.bg_opa = LV_OPA_COVER;
  rect_dsc.border_width = 0;

  lv_canvas_draw_rect(canvas_, px, py, cell_width_, cell_height_, &rect_dsc);
}

void GameSnake::draw_cell_fast_(int gx, int gy, lv_color_t color) {
  const int px = grid_offset_x_ + gx * cell_width_;
  const int py = grid_offset_y_ + gy * cell_height_;
  fill_rect_fast(px, py, cell_width_, cell_height_, color);  // Also invalidates
}

void GameSnake::clear_score_area_fast_() {
  // Clear only a small area for the score text (80x14 instead of 100x50)
  // This reduces from 5000 to ~1120 pixels
  fill_rect_fast(2, 2, 80, 14, color_bg_);
}

void GameSnake::draw_border_() {
  lv_draw_rect_dsc_t rect_dsc;
  lv_draw_rect_dsc_init(&rect_dsc);
  rect_dsc.bg_opa = LV_OPA_TRANSP;
  rect_dsc.border_color = color_border_;
  rect_dsc.border_width = 1;
  rect_dsc.border_opa = LV_OPA_COVER;

  // Draw border 1 pixel outside the actual game grid
  const int grid_pixel_width = grid_cols_ * cell_width_;
  const int grid_pixel_height = grid_rows_ * cell_height_;
  lv_canvas_draw_rect(canvas_, grid_offset_x_ - 1, grid_offset_y_ - 1, grid_pixel_width + 2, grid_pixel_height + 2,
                      &rect_dsc);
}

void GameSnake::draw_score_() {
  char buf[32];
  snprintf(buf, sizeof(buf), "Score: %d", state_.score);
  draw_text(2, 2, buf, color_snake_, LV_TEXT_ALIGN_LEFT);

  if (state_.game_over) {
    // Draw game over text
    draw_text(0, area_.h / 2 - 10, "GAME OVER", color_snake_, LV_TEXT_ALIGN_CENTER);
    snprintf(buf, sizeof(buf), "Score: %d", state_.score);
    draw_text(0, area_.h / 2 + 4, buf, color_snake_, LV_TEXT_ALIGN_CENTER);
  }
}

}  // namespace esphome::game_snake
