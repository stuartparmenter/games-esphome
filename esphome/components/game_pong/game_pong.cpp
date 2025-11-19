// Pong game implementation
// Ported from: https://github.com/stuartparmenter/hub75-studio

#include "game_pong.h"
#include "pong_ai.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace esphome::game_pong {

static const char *const TAG = "game.pong";

GamePong::GamePong()
    : scored_(false),
      last_scored_right_(false),
      score_left_(0),
      score_right_(0),
      ball_x_(0.0f),
      ball_y_(0.0f),
      vx_(0.0f),
      vy_(0.0f),
      left_y_(0.0f),
      right_y_(0.0f),
      left_vy_(0.0f),
      right_vy_(0.0f),
      serve_idx_(0) {
  // Initialize colors
  color_fg_ = lv_color_hex(0xFFFFFF);
  color_bg_ = lv_color_hex(0x000000);
}

void GamePong::on_bind(lv_obj_t *canvas) {
  GameBase::on_bind(canvas);
  ESP_LOGI(TAG, "Pong game bound to canvas");
}

void GamePong::on_resize(const Rect &r) {
  GameBase::on_resize(r);
  ESP_LOGI(TAG, "Pong canvas resized to %dx%d", r.w, r.h);

  if (area_.w > 0 && area_.h > 0) {
    // Calculate dynamic game element sizes based on canvas dimensions
    // Paddle height is the primary scaling unit
    paddle_h_ = area_.h / PADDLE_HEIGHT_DIVISOR;
    if (paddle_h_ < 8)
      paddle_h_ = 8;

    // Scale other elements based on paddle height
    paddle_w_ = (int) (paddle_h_ * PADDLE_WIDTH_RATIO);
    if (paddle_w_ < 2)
      paddle_w_ = 2;

    // Paddle margin is same as paddle width
    paddle_margin_x_ = paddle_w_;

    // Ball size (square, based on paddle height)
    ball_w_ = (int) (paddle_h_ * BALL_SIZE_RATIO);
    ball_h_ = ball_w_;
    if (ball_w_ < 2)
      ball_w_ = 2;
    if (ball_h_ < 2)
      ball_h_ = 2;

    // Ball speed (scales with canvas size)
    ball_speed_x_ = area_.w * BALL_SPEED_X_RATIO;
    ball_speed_y_ = area_.h * BALL_SPEED_Y_RATIO;

    // Ensure minimum speed
    if (ball_speed_x_ < 0.5f)
      ball_speed_x_ = 0.5f;
    if (ball_speed_y_ < 0.5f)
      ball_speed_y_ = 0.5f;

    // Paddle speed (scales with canvas height)
    player_speed_ = area_.h * PLAYER_SPEED_RATIO;
    if (player_speed_ < 1.0f)
      player_speed_ = 1.0f;

    ESP_LOGI(TAG, "Pong scaled: paddle=%dx%d, ball=%dx%d, margin=%d, speed=%.2fx%.2f, player_speed=%.2f", paddle_w_,
             paddle_h_, ball_w_, ball_h_, paddle_margin_x_, ball_speed_x_, ball_speed_y_, player_speed_);

    // Reset paddles to center
    left_y_ = (area_.h - paddle_h_) / 2.0f;
    right_y_ = (area_.h - paddle_h_) / 2.0f;
  }

  reset_ball_();
  needs_render_ = true;
}

void GamePong::reset() {
  score_left_ = 0;
  score_right_ = 0;
  scored_ = false;
  last_scored_right_ = false;
  state_.reset();

  initial_render_ = true;
  needs_render_ = true;
  last_drawn_score_left_ = 0;
  last_drawn_score_right_ = 0;
  last_paused_ = false;
  last_ball_x_ = -1;
  last_ball_y_ = -1;
  last_left_y_ = -1;
  last_right_y_ = -1;
  last_ball_over_score_ = false;

  // Reset input state for all players
  input_p1_up_held_ = false;
  input_p1_down_held_ = false;
  input_p2_up_held_ = false;
  input_p2_down_held_ = false;

  reset_ball_();
}

void GamePong::on_input(const InputEvent &event) {
  // Ignore NONE events (used by AI when no action needed)
  if (event.type == InputType::NONE)
    return;

  // Handle START button (only on press, any player can trigger)
  if (event.type == InputType::START && event.pressed) {
    if (state_.game_over) {
      // Restart game if game over
      this->reset();
    } else {
      // Toggle pause/resume if game is running
      if (paused_) {
        this->resume();
      } else {
        this->pause();
        // Clear input state when pausing to avoid stuck inputs
        input_p1_up_held_ = false;
        input_p1_down_held_ = false;
        input_p2_up_held_ = false;
        input_p2_down_held_ = false;
      }
      needs_render_ = true;  // Trigger render to show/hide pause text
    }
    return;
  }

  if (state_.game_over || paused_)
    return;  // Ignore inputs if game over or paused

  // Ignore inputs from external sources for AI-controlled players
  // AI inputs are injected via update_ai_() and use the processing_ai_inputs_ flag
  if (!processing_ai_inputs_ && !is_human_player(event.player))
    return;

  // Route inputs based on player number
  // Player 1 controls left paddle, Player 2 controls right paddle
  // Note: Both human and AI inputs are processed here
  // AI inputs are injected via update_ai_(), human inputs come from external sources
  if (event.player == 1) {
    switch (event.type) {
      case InputType::UP:
        input_p1_up_held_ = event.pressed;
        break;
      case InputType::DOWN:
        input_p1_down_held_ = event.pressed;
        break;
      default:
        break;
    }
  } else if (event.player == 2) {
    switch (event.type) {
      case InputType::UP:
        input_p2_up_held_ = event.pressed;
        break;
      case InputType::DOWN:
        input_p2_down_held_ = event.pressed;
        break;
      default:
        break;
    }
  }
}

// ========== Game Logic ==========

void GamePong::update_ai_() {
  // Create AI controllers if needed
  if (!is_human_player(1) && !ai_player1_) {
    ai_player1_ = std::make_unique<PongAI>(1);
  }
  if (!is_human_player(2) && !ai_player2_) {
    ai_player2_ = std::make_unique<PongAI>(2);
  }

  // Destroy AI controllers if no longer needed
  if (is_human_player(1) && ai_player1_) {
    ai_player1_.reset();
  }
  if (is_human_player(2) && ai_player2_) {
    ai_player2_.reset();
  }

  // Update AI players and inject their inputs
  // Set flag to allow AI inputs to bypass human player check
  processing_ai_inputs_ = true;

  if (ai_player1_) {
    auto event = ai_player1_->update(0.0f, state_, this);
    on_input(event);
  }
  if (ai_player2_) {
    auto event = ai_player2_->update(0.0f, state_, this);
    on_input(event);
  }

  processing_ai_inputs_ = false;
}

void GamePong::reset_ball_() {
  // Center the ball (only if canvas is initialized)
  if (area_.w > 0 && area_.h > 0) {
    ball_x_ = (area_.w - ball_w_) * 0.5f;
    ball_y_ = (area_.h - ball_h_) * 0.5f;
    serve_ball_();
  }
}

void GamePong::serve_ball_() {
  // Serve toward the player who conceded last point
  float sx = ball_speed_x_;
  vx_ = last_scored_right_ ? -fabsf(sx) : fabsf(sx);

  // Vary vertical speed using a deterministic cycle
  int i = serve_idx_ % 6;
  serve_idx_ = (serve_idx_ + 1) % 6000;
  vy_ = ball_speed_y_ * 0.6f * SERVE_ANGLES[i];

  // Reset paddle dynamics
  left_vy_ = 0;
  right_vy_ = 0;

  // Reset AI controllers on new serve
  if (ai_player1_) {
    ai_player1_->reset();
  }
  if (ai_player2_) {
    ai_player2_->reset();
  }
}

bool GamePong::check_paddle_collision_(float ball_top, float ball_bottom, float paddle_y) {
  float paddle_top = paddle_y;
  float paddle_bottom = paddle_y + paddle_h_;
  return (ball_bottom >= paddle_top) && (ball_top <= paddle_bottom);
}

void GamePong::step(float dt) {
  if (!canvas_)
    return;

  // Check if we need to render pause/unpause text even when paused
  if (needs_render_ && (paused_ || state_.game_over)) {
    render_();
    needs_render_ = false;
    return;
  }

  if (paused_ || state_.game_over)
    return;

  // Update AI controllers (they inject inputs via on_input)
  update_ai_();

  // Integrate ball
  float nx = ball_x_ + vx_;
  float ny = ball_y_ + vy_;

  // Top/bottom bounce
  if (ny <= 0) {
    ny = 0;
    vy_ = -vy_;
  } else if (ny >= (area_.h - ball_h_)) {
    ny = area_.h - ball_h_;
    vy_ = -vy_;
  }

  // Update left paddle based on input state
  if (input_p1_up_held_ && !input_p1_down_held_) {
    left_vy_ = -player_speed_;
  } else if (input_p1_down_held_ && !input_p1_up_held_) {
    left_vy_ = player_speed_;
  } else {
    // Both or neither held - stop
    left_vy_ = 0.0f;
  }

  left_y_ += left_vy_;

  // Clamp inside screen
  if (left_y_ < 0)
    left_y_ = 0;
  if (left_y_ > (area_.h - paddle_h_))
    left_y_ = area_.h - paddle_h_;

  // Update right paddle based on input state
  if (input_p2_up_held_ && !input_p2_down_held_) {
    right_vy_ = -player_speed_;
  } else if (input_p2_down_held_ && !input_p2_up_held_) {
    right_vy_ = player_speed_;
  } else {
    // Both or neither held - stop
    right_vy_ = 0.0f;
  }

  right_y_ += right_vy_;

  // Clamp inside screen
  if (right_y_ < 0)
    right_y_ = 0;
  if (right_y_ > (area_.h - paddle_h_))
    right_y_ = area_.h - paddle_h_;

  // Paddle plane positions
  int left_x = paddle_margin_x_;
  int right_x = area_.w - paddle_margin_x_ - paddle_w_;

  float ball_top = ny;
  float ball_bottom = ny + ball_h_;

  // LEFT paddle collision
  if (nx <= (left_x + paddle_w_)) {
    if (check_paddle_collision_(ball_top, ball_bottom, left_y_)) {
      nx = left_x + paddle_w_;
      vx_ = fabsf(ball_speed_x_);
      // Add spin from paddle movement
      float offset = ((ny + ball_h_ / 2.0f) - (left_y_ + paddle_h_ / 2.0f)) / (paddle_h_ / 2.0f);
      vy_ += 0.25f * offset + 0.35f * left_vy_;
    }
  }

  // RIGHT paddle collision
  if ((nx + ball_w_) >= right_x) {
    if (check_paddle_collision_(ball_top, ball_bottom, right_y_)) {
      nx = right_x - ball_w_;
      vx_ = -fabsf(ball_speed_x_);
      float offset = ((ny + ball_h_ / 2.0f) - (right_y_ + paddle_h_ / 2.0f)) / (paddle_h_ / 2.0f);
      vy_ += 0.25f * offset + 0.35f * right_vy_;
    }
  }

  // Edge scoring
  if (nx <= 0) {
    score_right_++;
    state_.score = score_right_;  // Track right score in state
    last_scored_right_ = true;
    scored_ = true;
  } else if ((nx + ball_w_) >= area_.w) {
    score_left_++;
    state_.score = score_left_;  // Track left score in state
    last_scored_right_ = false;
    scored_ = true;
  }

  if (!scored_) {
    ball_x_ = nx;
    ball_y_ = ny;
    needs_render_ = true;  // Ball or paddles moved
  } else {
    // Stop ball and prepare for next serve
    vx_ = 0;
    vy_ = 0;

    // Brief pause then reset (simulated with counter)
    static int score_delay = 0;
    score_delay++;
    if (score_delay >= 10) {  // ~300ms at 30 FPS
      score_delay = 0;
      scored_ = false;
      reset_ball_();
      needs_render_ = true;
    }
  }

  // Render if needed
  if (needs_render_) {
    render_();
    needs_render_ = false;
  }
}

// ========== Rendering ==========

void GamePong::render_() {
  if (!canvas_)
    return;

  if (initial_render_) {
    // Clear canvas
    lv_canvas_fill_bg(canvas_, color_bg_, LV_OPA_COVER);

    // Draw score
    draw_score_();

    // Draw paddles
    draw_paddle_(paddle_margin_x_, (int) left_y_);
    draw_paddle_(area_.w - paddle_margin_x_ - paddle_w_, (int) right_y_);

    // Draw ball
    draw_ball_();

    initial_render_ = false;
    last_drawn_score_left_ = score_left_;
    last_drawn_score_right_ = score_right_;
    last_ball_x_ = (int) ball_x_;
    last_ball_y_ = (int) ball_y_;
    last_left_y_ = (int) left_y_;
    last_right_y_ = (int) right_y_;
    last_ball_over_score_ = false;  // Will be updated on next frame

    // Full invalidation for initial render
    lv_obj_invalidate(canvas_);
  } else {
    // Fast incremental update using direct buffer manipulation

    // Check if ball overlaps score area (top center)
    int ball_x_int = (int) ball_x_;
    int ball_y_int = (int) ball_y_;
    int score_left = area_.w / 2 - 30;
    int score_right = area_.w / 2 + 30;
    int score_top = 2;
    int score_bottom = 16;

    bool ball_over_score = (ball_x_int + ball_w_ >= score_left && ball_x_int <= score_right &&
                           ball_y_int + ball_h_ >= score_top && ball_y_int <= score_bottom);

    // Erase and redraw ball if it moved
    if (ball_x_int != last_ball_x_ || ball_y_int != last_ball_y_) {
      if (last_ball_x_ >= 0 && last_ball_y_ >= 0) {
        erase_ball_fast_(last_ball_x_, last_ball_y_);
      }
      draw_ball_fast_();
      last_ball_x_ = ball_x_int;
      last_ball_y_ = ball_y_int;
    }

    // If ball just moved away from score area, redraw score
    if (last_ball_over_score_ && !ball_over_score) {
      clear_score_area_fast_();
      draw_score_();
    }
    last_ball_over_score_ = ball_over_score;

    // Erase and redraw left paddle if it moved
    int left_y_int = (int) left_y_;
    if (left_y_int != last_left_y_) {
      if (last_left_y_ >= 0) {
        erase_paddle_fast_(paddle_margin_x_, last_left_y_);
      }
      draw_paddle_fast_(paddle_margin_x_, left_y_int);
      last_left_y_ = left_y_int;
    }

    // Erase and redraw right paddle if it moved
    int right_y_int = (int) right_y_;
    if (right_y_int != last_right_y_) {
      if (last_right_y_ >= 0) {
        erase_paddle_fast_(area_.w - paddle_margin_x_ - paddle_w_, last_right_y_);
      }
      draw_paddle_fast_(area_.w - paddle_margin_x_ - paddle_w_, right_y_int);
      last_right_y_ = right_y_int;
    }

    // Redraw score if it changed
    if (score_left_ != last_drawn_score_left_ || score_right_ != last_drawn_score_right_) {
      clear_score_area_fast_();
      draw_score_();
      last_drawn_score_left_ = score_left_;
      last_drawn_score_right_ = score_right_;
    }

    // Handle pause state changes
    if (paused_ != last_paused_) {
      if (!paused_) {
        // Transitioning from paused to unpaused - clear the pause text
        clear_center_text_area_();
      } else {
        // Transitioning to paused - draw pause text
        draw_score_();
      }
      last_paused_ = paused_;
    }
  }
}

void GamePong::draw_paddle_(int x, int y) { fill_rect(x, y, paddle_w_, paddle_h_, color_fg_); }

void GamePong::draw_paddle_fast_(int x, int y) { fill_rect_fast(x, y, paddle_w_, paddle_h_, color_fg_); }

void GamePong::erase_paddle_fast_(int x, int y) { fill_rect_fast(x, y, paddle_w_, paddle_h_, color_bg_); }

void GamePong::draw_ball_() {
  // Draw ball as rounded rectangle
  fill_rect((int) ball_x_, (int) ball_y_, ball_w_, ball_h_, color_fg_);
}

void GamePong::draw_ball_fast_() { fill_rect_fast((int) ball_x_, (int) ball_y_, ball_w_, ball_h_, color_fg_); }

void GamePong::erase_ball_fast_(int x, int y) { fill_rect_fast(x, y, ball_w_, ball_h_, color_bg_); }

void GamePong::clear_score_area_fast_() {
  // Clear score area at top center
  fill_rect_fast(area_.w / 2 - 30, 2, 60, 14, color_bg_);
}

void GamePong::clear_center_text_area_() {
  // Clear center area where pause text is drawn
  const int text_w = 60;
  const int text_h = 16;
  const int center_x = (area_.w - text_w) / 2;
  const int center_y = area_.h / 2 - 7 - 2;
  fill_rect_fast(center_x, center_y, text_w, text_h, color_bg_);
}

void GamePong::draw_score_() {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d - %d", score_left_, score_right_);
  draw_text(0, 2, buf, color_fg_, LV_TEXT_ALIGN_CENTER);

  if (paused_) {
    // Draw paused text
    draw_text(0, area_.h / 2 - 7, "PAUSED", color_fg_, LV_TEXT_ALIGN_CENTER);
  }
}

}  // namespace esphome::game_pong
