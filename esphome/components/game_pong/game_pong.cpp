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
      ball_x_(30.0f),
      ball_y_(30.0f),
      vx_(BALL_SPEED_X),
      vy_(BALL_SPEED_Y),
      left_y_(28.0f),
      right_y_(20.0f),
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
  reset_ball_();
}

void GamePong::reset() {
  score_left_ = 0;
  score_right_ = 0;
  scored_ = false;
  last_scored_right_ = false;
  state_.reset();
  reset_ball_();
}

void GamePong::on_input(const InputEvent &event) {
  // No direct input - AI vs AI
  // Could add player control in the future
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
  // Center the ball
  ball_x_ = (area_.w - BALL_W) * 0.5f;
  ball_y_ = (area_.h - BALL_H) * 0.5f;

  serve_ball_();
}

void GamePong::serve_ball_() {
  // Serve toward the player who conceded last point
  float sx = BALL_SPEED_X;
  vx_ = last_scored_right_ ? -fabsf(sx) : fabsf(sx);

  // Vary vertical speed using a deterministic cycle
  int i = serve_idx_ % 6;
  serve_idx_ = (serve_idx_ + 1) % 6000;
  vy_ = BALL_SPEED_Y * 0.6f * SERVE_ANGLES[i];

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
  float ball_center_y = ball_y_ + BALL_H / 2.0f;
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
    int px = is_left ? (int) ball_x_ : (int) (area_.w - (ball_x_ + BALL_W));
    float max_speed = max_speed_base;
    if (px <= AI_PANIC_ZONE)
      max_speed *= AI_PANIC_MULT;

    // Proportional-ish target with slight overshoot tendency
    float center = top_y + (PADDLE_H / 2.0f);
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
  if (top_y > (area_.h - PADDLE_H))
    top_y = area_.h - PADDLE_H;
}

bool GamePong::check_paddle_collision_(float ball_top, float ball_bottom, float paddle_y) {
  float paddle_top = paddle_y;
  float paddle_bottom = paddle_y + PADDLE_H;
  return (ball_bottom >= paddle_top) && (ball_top <= paddle_bottom);
}

void GamePong::step(float dt) {
  if (!canvas_ || state_.game_over)
    return;

  // Integrate ball
  float nx = ball_x_ + vx_;
  float ny = ball_y_ + vy_;

  // Top/bottom bounce
  if (ny <= 0) {
    ny = 0;
    vy_ = -vy_;
  } else if (ny >= (area_.h - BALL_H)) {
    ny = area_.h - BALL_H;
    vy_ = -vy_;
  }

  // Update AI paddles
  update_paddle_(true, left_y_, left_vy_, left_react_frames_, AI_BIAS_LEFT, AI_L_MAX_SPEED);
  update_paddle_(false, right_y_, right_vy_, right_react_frames_, AI_BIAS_RIGHT, AI_R_MAX_SPEED);

  // Paddle plane positions
  int left_x = PADDLE_MARGIN_X;
  int right_x = area_.w - PADDLE_MARGIN_X - PADDLE_W;

  float ball_top = ny;
  float ball_bottom = ny + BALL_H;

  // LEFT paddle collision
  if (nx <= (left_x + PADDLE_W)) {
    bool overlap = check_paddle_collision_(ball_top, ball_bottom, left_y_);

    // Force miss if that was chosen for this rally
    if (will_miss_left_)
      overlap = false;

    if (overlap) {
      nx = left_x + PADDLE_W;
      vx_ = fabsf(BALL_SPEED_X);
      // Add spin from paddle movement
      float offset = ((ny + BALL_H / 2.0f) - (left_y_ + PADDLE_H / 2.0f)) / (PADDLE_H / 2.0f);
      vy_ += 0.25f * offset + 0.35f * left_vy_;
    }
  }

  // RIGHT paddle collision
  if ((nx + BALL_W) >= right_x) {
    bool overlap = check_paddle_collision_(ball_top, ball_bottom, right_y_);

    if (will_miss_right_)
      overlap = false;

    if (overlap) {
      nx = right_x - BALL_W;
      vx_ = -fabsf(BALL_SPEED_X);
      float offset = ((ny + BALL_H / 2.0f) - (right_y_ + PADDLE_H / 2.0f)) / (PADDLE_H / 2.0f);
      vy_ += 0.25f * offset + 0.35f * right_vy_;
    }
  }

  // Edge scoring
  if (nx <= 0) {
    score_right_++;
    state_.score = score_right_;  // Track right score in state
    last_scored_right_ = true;
    scored_ = true;
  } else if ((nx + BALL_W) >= area_.w) {
    score_left_++;
    state_.score = score_left_;  // Track left score in state
    last_scored_right_ = false;
    scored_ = true;
  }

  if (!scored_) {
    ball_x_ = nx;
    ball_y_ = ny;
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
    }
  }

  // Render the game
  render_();
}

// ========== Rendering ==========

void GamePong::render_() {
  if (!canvas_)
    return;

  // Clear canvas
  lv_canvas_fill_bg(canvas_, color_bg_, LV_OPA_COVER);

  // Draw score
  draw_score_();

  // Draw paddles
  draw_paddle_(PADDLE_MARGIN_X, (int) left_y_);
  draw_paddle_(area_.w - PADDLE_MARGIN_X - PADDLE_W, (int) right_y_);

  // Draw ball
  draw_ball_();

  lv_obj_invalidate(canvas_);
}

void GamePong::draw_paddle_(int x, int y) { fill_rect(x, y, PADDLE_W, PADDLE_H, color_fg_); }

void GamePong::draw_ball_() {
  // Draw ball as rounded rectangle
  fill_rect((int) ball_x_, (int) ball_y_, BALL_W, BALL_H, color_fg_);
}

void GamePong::draw_score_() {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d - %d", score_left_, score_right_);
  draw_text(0, 2, buf, color_fg_, LV_TEXT_ALIGN_CENTER);
}

}  // namespace esphome::game_pong
