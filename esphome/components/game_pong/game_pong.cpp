// Pong game implementation
// Ported from: https://github.com/stuartparmenter/hub75-studio

#include "game_pong.h"
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
      left_react_frames_(0),
      right_react_frames_(0),
      serve_idx_(0),
      will_miss_left_(false),
      will_miss_right_(false),
      rng_state_(2463534242) {
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

    // Paddle speeds (scale with canvas height)
    ai_left_max_speed_ = area_.h * AI_L_SPEED_RATIO;
    ai_right_max_speed_ = area_.h * AI_R_SPEED_RATIO;
    player_speed_ = area_.h * PLAYER_SPEED_RATIO;

    // Ensure minimum paddle speeds
    if (ai_left_max_speed_ < 1.0f)
      ai_left_max_speed_ = 1.0f;
    if (ai_right_max_speed_ < 1.0f)
      ai_right_max_speed_ = 1.0f;
    if (player_speed_ < 1.0f)
      player_speed_ = 1.0f;

    ESP_LOGI(TAG,
             "Pong scaled: paddle=%dx%d, ball=%dx%d, margin=%d, speed=%.2fx%.2f, ai_speed=%.2f/%.2f, "
             "player_speed=%.2f",
             paddle_w_, paddle_h_, ball_w_, ball_h_, paddle_margin_x_, ball_speed_x_, ball_speed_y_,
             ai_left_max_speed_, ai_right_max_speed_, player_speed_);

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

  // Reset input state
  input_up_held_ = false;
  input_down_held_ = false;

  reset_ball_();
}

void GamePong::on_input(const InputEvent &event) {
  // Handle START button (only on press)
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
        input_up_held_ = false;
        input_down_held_ = false;
      }
      needs_render_ = true;  // Trigger render to show/hide pause text
    }
    return;
  }

  if (state_.game_over || paused_ || !player_control_)
    return;  // Ignore inputs if game over, paused, or AI mode

  // Player controls left paddle - track button state
  switch (event.type) {
    case InputType::UP:
      input_up_held_ = event.pressed;
      break;
    case InputType::DOWN:
      input_down_held_ = event.pressed;
      break;
    default:
      break;
  }
}

// ========== PRNG Helpers ==========

uint32_t GamePong::rng_() {
  // xorshift32
  rng_state_ ^= rng_state_ << 13;
  rng_state_ ^= rng_state_ >> 17;
  rng_state_ ^= rng_state_ << 5;
  return rng_state_;
}

float GamePong::rand01_() {
  return (rng_() & 0xFFFFFF) / 16777216.0f;  // [0, 1)
}

float GamePong::rand_sym_() {
  return rand01_() * 2.0f - 1.0f;  // [-1, 1]
}

int GamePong::ms_to_frames_(int ms) {
  // Convert milliseconds to frames (assuming ~30 FPS)
  int frames = (ms + 16) / 33;  // 33ms per frame at 30 FPS
  return frames < 0 ? 0 : frames;
}

// ========== Game Logic ==========

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

  // Set reaction timers
  left_react_frames_ = ms_to_frames_(AI_L_REACT_MS);
  right_react_frames_ = ms_to_frames_(AI_R_REACT_MS);

  // Determine if the receiver will intentionally miss this rally
  will_miss_left_ = false;
  will_miss_right_ = false;
  bool to_right = (vx_ > 0);
  float miss_p = AI_MISS_CHANCE;
  if (to_right) {
    if (rand01_() < miss_p)
      will_miss_right_ = true;
  } else {
    if (rand01_() < miss_p)
      will_miss_left_ = true;
  }
}

void GamePong::update_paddle_(bool is_left, float &top_y, float &vy, int &react_frames, float bias,
                              float max_speed_base) {
  // Decide if ball moves toward this paddle
  bool toward = is_left ? (vx_ < 0) : (vx_ > 0);

  // Desired tracking center with tiny jitter & bias
  float jitter = AI_JITTER_PX * rand_sym_();
  float ball_center_y = ball_y_ + ball_h_ / 2.0f;
  float target = ball_center_y + bias + jitter;

  // If ball moves away: drift toward mid slowly
  float mid = area_.h / 2.0f;
  if (!toward)
    target = mid + bias * 0.5f;

  // Reaction delay: do nothing while timer > 0
  if (toward && react_frames > 0) {
    react_frames--;
    // Light damping to settle vy
    vy *= 0.9f;
    top_y += vy;
  } else {
    // Panic boost when horizontally close
    int px = is_left ? (int) ball_x_ : (int) (area_.w - (ball_x_ + ball_w_));
    float max_speed = max_speed_base;
    if (px <= AI_PANIC_ZONE)
      max_speed *= AI_PANIC_MULT;

    // Proportional-ish target with slight overshoot tendency
    float center = top_y + (paddle_h_ / 2.0f);
    float err = (target - center);
    float desired_speed = err * AI_ERR_GAIN;

    // Clamp desired speed
    if (desired_speed > max_speed)
      desired_speed = max_speed;
    if (desired_speed < -max_speed)
      desired_speed = -max_speed;

    // Accelerate toward desired speed
    float accel = AI_ACCEL;
    if (desired_speed > vy) {
      vy += accel;
      if (vy > desired_speed)
        vy = desired_speed;
    } else {
      vy -= accel;
      if (vy < desired_speed)
        vy = desired_speed;
    }

    top_y += vy;
  }

  // Clamp inside screen
  if (top_y < 0)
    top_y = 0;
  if (top_y > (area_.h - paddle_h_))
    top_y = area_.h - paddle_h_;
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

  // Update paddles
  if (player_control_) {
    // Player controls left paddle based on held buttons
    if (input_up_held_ && !input_down_held_) {
      left_vy_ = -player_speed_;
    } else if (input_down_held_ && !input_up_held_) {
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
  } else {
    // AI controls left paddle
    update_paddle_(true, left_y_, left_vy_, left_react_frames_, AI_BIAS_LEFT, ai_left_max_speed_);
  }

  // AI always controls right paddle
  update_paddle_(false, right_y_, right_vy_, right_react_frames_, AI_BIAS_RIGHT, ai_right_max_speed_);

  // Paddle plane positions
  int left_x = paddle_margin_x_;
  int right_x = area_.w - paddle_margin_x_ - paddle_w_;

  float ball_top = ny;
  float ball_bottom = ny + ball_h_;

  // LEFT paddle collision
  if (nx <= (left_x + paddle_w_)) {
    bool overlap = check_paddle_collision_(ball_top, ball_bottom, left_y_);

    // Force miss if that was chosen for this rally
    if (will_miss_left_)
      overlap = false;

    if (overlap) {
      nx = left_x + paddle_w_;
      vx_ = fabsf(ball_speed_x_);
      // Add spin from paddle movement
      float offset = ((ny + ball_h_ / 2.0f) - (left_y_ + paddle_h_ / 2.0f)) / (paddle_h_ / 2.0f);
      vy_ += 0.25f * offset + 0.35f * left_vy_;
    }
  }

  // RIGHT paddle collision
  if ((nx + ball_w_) >= right_x) {
    bool overlap = check_paddle_collision_(ball_top, ball_bottom, right_y_);

    if (will_miss_right_)
      overlap = false;

    if (overlap) {
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

    // Full invalidation for initial render
    lv_obj_invalidate(canvas_);
  } else {
    // Fast incremental update using direct buffer manipulation

    // Erase and redraw ball if it moved
    int ball_x_int = (int) ball_x_;
    int ball_y_int = (int) ball_y_;
    if (ball_x_int != last_ball_x_ || ball_y_int != last_ball_y_) {
      if (last_ball_x_ >= 0 && last_ball_y_ >= 0) {
        erase_ball_fast_(last_ball_x_, last_ball_y_);
      }
      draw_ball_fast_();
      last_ball_x_ = ball_x_int;
      last_ball_y_ = ball_y_int;
    }

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
