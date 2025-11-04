// Breakout game implementation
// Ported from: https://github.com/richrd/esphome-clock-os/tree/main/clockos/packages/games/breakout

#include "game_breakout.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace esphome::game_breakout {

static const char *const TAG = "game.breakout";

GameBreakout::GameBreakout()
    : frame_(0),
      pause_frames_(PAUSE_DURATION),
      paddle_w_(PADDLE_W_INITIAL),
      speed_(SPEED_INITIAL),
      score_(0),
      score_ticker_(0),
      level_(0),
      level_started_(false),
      shield_amount_(0),
      shooter_level_(0),
      paddle_x_(0),
      paddle_y_(0),
      paddle_hit_(false),
      autoplay_(false),
      input_position_(25),  // Start in middle
      left_held_(false),
      right_held_(false) {
  // Initialize balls
  for (int i = 0; i < MAX_BALLS; i++) {
    balls_[i] = {0, 0, 0, 0, false};
  }
  balls_[0].alive = true;

  // Initialize bricks with positions
  align_brick_positions_();

  // Initialize colors
  color_on_ = lv_color_hex(0xFFFFFF);
  color_off_ = lv_color_hex(0x000000);
}

void GameBreakout::on_bind(lv_obj_t *canvas) {
  GameBase::on_bind(canvas);
  ESP_LOGI(TAG, "Breakout game bound to canvas");
}

void GameBreakout::on_resize(const Rect &r) {
  GameBase::on_resize(r);
  ESP_LOGI(TAG, "Breakout canvas resized to %dx%d", r.w, r.h);
  paddle_y_ = r.h - PADDLE_H;
}

void GameBreakout::reset() {
  reset_game_();
  state_.reset();
}

void GameBreakout::on_input(const InputEvent &event) {
  switch (event.type) {
    case InputType::LEFT:
      // Track held state for continuous movement
      left_held_ = event.pressed;
      break;
    case InputType::RIGHT:
      // Track held state for continuous movement
      right_held_ = event.pressed;
      break;
    case InputType::A:
    case InputType::B:
    case InputType::START:
      // Toggle autoplay on button press only
      if (event.pressed) {
        autoplay_ = !autoplay_;
        ESP_LOGI(TAG, "Autoplay: %s", autoplay_ ? "ON" : "OFF");
      }
      break;
    case InputType::ROTATE_CW:
      // Rotary encoder - treat each click as a single increment
      if (event.pressed && input_position_ < 50)
        input_position_++;
      break;
    case InputType::ROTATE_CCW:
      // Rotary encoder - treat each click as a single decrement
      if (event.pressed && input_position_ > 0)
        input_position_--;
      break;
    default:
      break;
  }
}

// ========== Game Logic Helpers ==========

void GameBreakout::clear_bricks_() {
  for (int i = 0; i < BRICK_COUNT; i++) {
    bricks_[i].hp = 0;
    bricks_[i].type = NORMAL;
  }
}

void GameBreakout::clear_projectiles_() { projectiles_.clear(); }

void GameBreakout::reset_game_() {
  score_ = 0;
  score_ticker_ = 0;
  state_.lives = LIVES_INITIAL;
  level_ = 0;  // 0 triggers level 1 setup
  pause_frames_ = PAUSE_DURATION;
  shield_amount_ = 0;
  shooter_level_ = 0;
  paddle_w_ = PADDLE_W_INITIAL;
}

bool GameBreakout::is_level_cleared_() {
  for (int i = 0; i < BRICK_COUNT; i++) {
    if (bricks_[i].hp > 0) {
      return false;
    }
  }
  return true;
}

void GameBreakout::place_ball_on_paddle_() {
  balls_[0].alive = true;
  balls_[0].x = paddle_x_ + (paddle_w_ / 2);
  balls_[0].y = area_.h - PADDLE_H - BALL_SIZE;
}

void GameBreakout::reset_balls_() {
  balls_[0].alive = true;
  balls_[0].direction_y = -1;  // Moving up to avoid immediate paddle hit

  for (int i = 1; i < MAX_BALLS; i++) {
    balls_[i].alive = false;
  }
}

void GameBreakout::setup_next_level_() {
  level_++;
  pause_frames_ = PAUSE_DURATION;
  level_started_ = false;

  // Increase speed progressively
  if (level_ > 1) {
    speed_ = SPEED_INITIAL * powf(SPEED_INCREASE_FACTOR, level_ - 1);
    if (speed_ > SPEED_MAX) {
      speed_ = SPEED_MAX;
    }
  } else {
    speed_ = SPEED_INITIAL;
  }

  clear_bricks_();
  align_brick_positions_();
  clear_projectiles_();
  reset_balls_();

  // Reset shooter powerup each level
  shooter_level_ = 0;

  int brick_hp = level_;
  if (brick_hp > BRICK_MAX_HP) {
    brick_hp = BRICK_MAX_HP;
  }

  int start_brick = 8;
  int end_brick = BRICK_COUNT - 24;

  // Add new row at level 4
  if (level_ > 3) {
    end_brick = BRICK_COUNT - 16;
  }

  // Add new row at level 6
  if (level_ > 5) {
    end_brick = BRICK_COUNT - 8;
  }

  // Add new row at level 11+
  if (level_ > 10) {
    end_brick = BRICK_COUNT;
  }

  for (int i = start_brick; i < end_brick; i++) {
    bricks_[i].hp = brick_hp;
  }

  // Place random special bricks according to level number
  int assigned = 0;
  int max_assigned = level_;

  if (max_assigned < 2) {
    max_assigned = 2;
  }
  if (max_assigned > 40) {
    max_assigned = 40;
  }

  while (assigned < max_assigned) {
    int idx = 8 + (esp_random() % (end_brick - 8));
    if (bricks_[idx].hp > 0 && bricks_[idx].type == NORMAL) {
      BrickType rtype_choices[] = {SHIELD,       EXTRA_BALL, WIDER_PADDLE,   EXTRA_LIFE,
                                   WONKY_BRICKS, SHOOTER,    POWERUP_SHUFFLE};
      int rtype_idx = esp_random() % (sizeof(rtype_choices) / sizeof(rtype_choices[0]));
      bricks_[idx].type = rtype_choices[rtype_idx];

      if (bricks_[idx].type == POWERUP_SHUFFLE) {
        bricks_[idx].hp = 2;
      }

      assigned++;
    }
  }
}

void GameBreakout::add_new_ball_() {
  for (int i = 0; i < MAX_BALLS; i++) {
    if (!balls_[i].alive) {
      balls_[i].alive = true;
      balls_[i].x = paddle_x_ + (paddle_w_ / 2);
      balls_[i].y = 0;                                // Place new ball at the top of the screen
      balls_[i].direction_x = (i % 2 == 0) ? 1 : -1;  // Alternate direction for variety
      balls_[i].direction_y = 1;                      // Start moving downwards
      break;                                          // Only add one ball at a time
    }
  }
}

void GameBreakout::shoot_projectile_() {
  if (projectiles_.size() >= MAX_PROJECTILES) {
    return;
  }

  if (shooter_level_ == 1) {
    Projectile p;
    p.x = paddle_x_ + (paddle_w_ / 2);
    p.y = paddle_y_ - 2;
    projectiles_.push_back(p);
  } else if (shooter_level_ == 2) {
    // Shoot from alternating sides of the paddle
    Projectile p1;
    p1.x = paddle_x_ + 2;
    p1.y = paddle_y_ - 2;
    projectiles_.push_back(p1);

    Projectile p2;
    p2.x = paddle_x_ + paddle_w_ - 3;
    p2.y = paddle_y_ - 2;
    projectiles_.push_back(p2);
  }
}

void GameBreakout::on_brick_hit_(int brick_id) {
  if (bricks_[brick_id].hp <= 0) {
    return;
  }

  bricks_[brick_id].hp--;
  score_ = score_ + POINTS_PER_BRICK;

  if (bricks_[brick_id].type == EXTRA_BALL) {
    bricks_[brick_id].hp = 0;
    add_new_ball_();
  }

  if (bricks_[brick_id].type == EXTRA_LIFE) {
    bricks_[brick_id].hp = 0;
    if (state_.lives < LIVES_MAX) {
      state_.lives++;
    }
  }

  if (bricks_[brick_id].type == SHIELD) {
    bricks_[brick_id].hp = 0;
    shield_amount_ += 1;
  }

  if (bricks_[brick_id].type == WIDER_PADDLE) {
    bricks_[brick_id].hp = 0;
    paddle_w_ += PADDLE_W_INCREASE;
    if (paddle_w_ > PADDLE_W_MAX) {
      paddle_w_ = PADDLE_W_MAX;
    }
  }

  if (bricks_[brick_id].type == WONKY_BRICKS) {
    bricks_[brick_id].hp = 0;
    randomise_brick_positions_();
  }

  if (bricks_[brick_id].type == SHOOTER) {
    bricks_[brick_id].hp = 0;
    shooter_level_ += 1;
    if (shooter_level_ > 2) {
      shooter_level_ = 2;
    }
  }

  if (bricks_[brick_id].type == POWERUP_SHUFFLE) {
    BrickType rtype_choices[] = {SHIELD, EXTRA_BALL, WIDER_PADDLE, EXTRA_LIFE, WONKY_BRICKS, SHOOTER, POWERUP_SHUFFLE};

    // Randomize all existing powerup brick types
    for (int i = 0; i < BRICK_COUNT; i++) {
      if (bricks_[i].hp > 0) {
        switch (bricks_[i].type) {
          case SHIELD:
          case EXTRA_BALL:
          case WIDER_PADDLE:
          case EXTRA_LIFE:
          case WONKY_BRICKS:
          case SHOOTER:
          case POWERUP_SHUFFLE:
            // Assign a new random powerup type
            bricks_[i].type = rtype_choices[esp_random() % (sizeof(rtype_choices) / sizeof(rtype_choices[0]))];
            break;
          default:
            break;
        }
      }
    }

    if (bricks_[brick_id].type == POWERUP_SHUFFLE) {
      // If the current brick is still shuffle, reset its HP so it can be triggered again
      bricks_[brick_id].hp = 2;
    }
  }
}

void GameBreakout::update_projectiles_() {
  for (int i = 0; i < projectiles_.size();) {
    projectiles_[i].y -= 2;  // Move projectile upwards
    if (projectiles_[i].y < 0) {
      // Remove projectile if it goes off-screen
      projectiles_.erase(projectiles_.begin() + i);
    } else {
      i++;
    }
  }

  // Check for projectile collisions with bricks
  for (int p = 0; p < projectiles_.size();) {
    bool hit = false;
    for (int i = 0; i < BRICK_COUNT; i++) {
      Brick &brick = bricks_[i];
      if (brick.hp == 0)
        continue;
      if (projectiles_[p].x >= brick.x && projectiles_[p].x < brick.x + BRICK_W && projectiles_[p].y >= brick.y &&
          projectiles_[p].y < brick.y + BRICK_H) {
        on_brick_hit_(i);
        // Remove projectile
        projectiles_.erase(projectiles_.begin() + p);
        hit = true;
        break;
      }
    }
    if (!hit) {
      p++;
    }
  }
}

bool GameBreakout::any_balls_alive_() {
  for (int i = 0; i < MAX_BALLS; i++) {
    if (balls_[i].alive) {
      return true;
    }
  }
  return false;
}

void GameBreakout::align_brick_positions_() {
  for (int i = 0; i < BRICK_COUNT; i++) {
    bricks_[i].x = (i % 8) * (BRICK_W + 1);
    bricks_[i].y = (i / 8) * (BRICK_H + 1);
    bricks_[i].hp = 0;
    bricks_[i].type = NORMAL;
  }
}

void GameBreakout::randomise_brick_positions_() {
  for (int i = 0; i < BRICK_COUNT; i++) {
    // Only apply to about 33% of bricks
    if ((esp_random() % 3) == 0) {
      int delta_x = (esp_random() % 3) - 1;  // -1, 0, or +1
      int delta_y = (esp_random() % 3) - 1;  // -1, 0, or +1
      bricks_[i].x += delta_x * 1;
      bricks_[i].y += delta_y * 1;
    }
  }
}

// ========== Main Game Loop ==========

void GameBreakout::step(float dt) {
  if (!canvas_ || state_.game_over)
    return;

  frame_++;

  // Setup level 1 on first frame
  if (level_ == 0) {
    setup_next_level_();
    state_.lives = LIVES_INITIAL;
  }

  // Check if level cleared
  bool level_cleared = is_level_cleared_();
  if (level_cleared) {
    ESP_LOGW(TAG, "Level %d cleared!", level_);
    setup_next_level_();
  }

  // Animate score ticker
  if (score_ticker_ < score_) {
    int score_difference = score_ - score_ticker_;
    if (score_difference >= 100) {
      score_ticker_ += 100;
    } else if (score_difference >= 10) {
      score_ticker_ += 10;
    } else {
      score_ticker_ += 1;
    }
  }

  state_.score = score_ticker_;

  // Apply continuous movement from held directions
  // Speed: 100 positions/second = full range (0-50) in 0.5 seconds
  constexpr float PADDLE_SPEED = 100.0f;  // positions per second
  if (left_held_ && !right_held_) {
    input_position_ -= PADDLE_SPEED * dt;
    if (input_position_ < 0)
      input_position_ = 0;
  } else if (right_held_ && !left_held_) {
    input_position_ += PADDLE_SPEED * dt;
    if (input_position_ > 50)
      input_position_ = 50;
  }

  // Calculate paddle position
  paddle_x_ = input_position_ * ((float) (area_.w - paddle_w_) / 50);
  paddle_y_ = area_.h - PADDLE_H;
  paddle_hit_ = false;

  // Autoplay AI
  if (autoplay_ && level_started_) {
    int ball_position_x = area_.w / 2;  // Middle by default
    int ball_direction_x = 0;
    for (int b = 0; b < MAX_BALLS; b++) {
      if (balls_[b].alive) {
        ball_position_x = balls_[b].x;
        ball_direction_x = balls_[b].direction_x;
        break;
      }
    }

    paddle_x_ = ball_position_x - (paddle_w_ / 2) - (ball_direction_x * 2);
    if (paddle_x_ < 0) {
      paddle_x_ = 0;
    } else if (paddle_x_ + paddle_w_ > area_.w) {
      paddle_x_ = area_.w - paddle_w_;
    }
  }

  // Handle pause logic (used when game is about to start or has ended)
  if (pause_frames_) {
    pause_frames_--;
    place_ball_on_paddle_();

    // If all frames consumed, check for game over
    if (!pause_frames_ && state_.lives == 0) {
      state_.game_over = true;
      reset_game_();
    }
  } else {
    // Game loop logic
    level_started_ = true;

    // Shoot
    if (shooter_level_ && (frame_ % 15 == 0)) {
      shoot_projectile_();
    }

    update_projectiles_();

    // Iterate over all balls and handle movement and collisions
    for (int b = 0; b < MAX_BALLS; b++) {
      Ball &ball = balls_[b];
      if (!ball.alive)
        continue;

      ball.x += (ball.direction_x * speed_);
      ball.y += (ball.direction_y * speed_);

      // Wall collisions
      if (ball.x < 0) {  // Left
        ball.direction_x = 1;
      }
      if (ball.y < 0) {  // Top
        ball.direction_y = 1;
      }
      if (ball.x + BALL_SIZE > area_.w) {  // Right
        ball.direction_x = -1;
      }

      // Check if ball hits paddle
      if (ball.direction_y == 1                        // Ball is moving downwards
          && ball.x + BALL_SIZE >= paddle_x_           // Ball right edge past paddle left edge
          && ball.x <= paddle_x_ + paddle_w_           // Ball left edge before paddle right edge
          && ball.y + BALL_SIZE > area_.h - PADDLE_H)  // Ball bottom edge past paddle top edge
      {
        ball.direction_y = -1;
        score_ = score_ + POINTS_PER_PADDLE_HIT;
        paddle_hit_ = true;

        if (ball.x + (BALL_SIZE / 2) > paddle_x_ + (paddle_w_ / 2)) {
          ball.direction_x = 1;
        } else {
          ball.direction_x = -1;
        }
      } else if (ball.y + BALL_SIZE > area_.h) {
        if (shield_amount_) {
          ball.direction_y = -1;
          shield_amount_--;
        } else {
          ball.alive = false;

          // Shorten paddle as penalty
          paddle_w_ -= PADDLE_W_DECREASE;
          if (paddle_w_ < PADDLE_W_INITIAL) {
            paddle_w_ = PADDLE_W_INITIAL;
          }

          if (!any_balls_alive_()) {
            state_.lives--;
            place_ball_on_paddle_();
            ball.direction_y = -1;
            pause_frames_ = PAUSE_DURATION;
          }
        }
      }

      // Brick collisions
      for (int i = 0; i < BRICK_COUNT; i++) {
        Brick &brick = bricks_[i];

        if (brick.hp == 0) {
          continue;
        }

        // Horizontal collisions
        if (ball.y >= brick.y && ball.y + BALL_SIZE <= brick.y + BRICK_H) {
          // Collision on left edge (ball moving right)
          if (ball.direction_x == 1) {
            if (ball.x + BALL_SIZE >= brick.x && ball.x + BALL_SIZE <= brick.x + BRICK_W) {
              ball.direction_x = -1;
              on_brick_hit_(i);
            }
          }
          // Collision on right edge (ball moving left)
          else if (ball.direction_x == -1) {
            if (ball.x <= brick.x + BRICK_W && ball.x >= brick.x) {
              ball.direction_x = 1;
              on_brick_hit_(i);
            }
          }
        }
        // Vertical collisions
        if (ball.x >= brick.x && ball.x + BALL_SIZE <= brick.x + BRICK_W) {
          // Collision on top edge (ball moving down)
          if (ball.direction_y == 1) {
            if (ball.y + BALL_SIZE >= brick.y && ball.y + BALL_SIZE <= brick.y + BRICK_H) {
              ball.direction_y = -1;
              on_brick_hit_(i);
            }
          }
          // Collision on bottom edge (ball moving up)
          else if (ball.direction_y == -1) {
            if (ball.y <= brick.y + BRICK_H && ball.y >= brick.y) {
              ball.direction_y = 1;
              on_brick_hit_(i);
            }
          }
        }
      }
    }
  }

  // Render the game
  render_();
}

// ========== Rendering ==========

void GameBreakout::render_() {
  if (!canvas_)
    return;

  // Clear canvas
  lv_canvas_fill_bg(canvas_, color_off_, LV_OPA_COVER);

  // Draw game elements
  draw_lives_left_();
  draw_score_();
  draw_level_();
  draw_bricks_();
  draw_paddle_();
  draw_projectiles_();
  draw_shield_();
  draw_balls_();
  draw_overlay_text_();

  lv_obj_invalidate(canvas_);
}

void GameBreakout::draw_heart_(int x, int y) {
  //  ## ##
  // #######
  // #######
  //  #####
  //   ###
  //    #
  draw_line(x + 1, y, x + 2, y, color_on_);
  draw_line(x + 4, y, x + 5, y, color_on_);
  fill_rect(x, y + 1, 7, 2, color_on_);
  draw_line(x + 1, y + 3, x + 5, y + 3, color_on_);
  draw_line(x + 2, y + 4, x + 4, y + 4, color_on_);
  draw_pixel(x + 3, y + 5, color_on_);
}

void GameBreakout::draw_lives_left_() {
  for (int i = 0; i < state_.lives && i < 6; i++) {
    draw_heart_(1 + i * 8, 1);
  }
}

void GameBreakout::draw_score_() {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", score_ticker_);
  draw_text(area_.w - 2, 0, buf, color_on_, LV_TEXT_ALIGN_RIGHT);
}

void GameBreakout::draw_level_() {
  char buf[16];
  snprintf(buf, sizeof(buf), "L%d", level_);
  draw_text(0, 0, buf, color_on_, LV_TEXT_ALIGN_CENTER);
}

void GameBreakout::draw_special_brick_corners_(int x, int y) {
  // Draw 2x2 L-shaped corners
  // Top-left
  draw_pixel(x, y, color_on_);
  draw_pixel(x + 1, y, color_on_);
  draw_pixel(x, y + 1, color_on_);

  // Top-right
  draw_pixel(x + BRICK_W - 2, y, color_on_);
  draw_pixel(x + BRICK_W - 1, y, color_on_);
  draw_pixel(x + BRICK_W - 1, y + 1, color_on_);

  // Bottom-left
  draw_pixel(x, y + BRICK_H - 2, color_on_);
  draw_pixel(x, y + BRICK_H - 1, color_on_);
  draw_pixel(x + 1, y + BRICK_H - 1, color_on_);

  // Bottom-right
  draw_pixel(x + BRICK_W - 2, y + BRICK_H - 1, color_on_);
  draw_pixel(x + BRICK_W - 1, y + BRICK_H - 1, color_on_);
  draw_pixel(x + BRICK_W - 1, y + BRICK_H - 2, color_on_);
}

void GameBreakout::draw_unbreakable_brick_(int x, int y) {
  fill_rect(x, y, BRICK_W, BRICK_H, color_on_);
  // Clear out 2x2 squares in each corner and keep 1px border intact
  fill_rect(x + 1, y + 1, 2, 2, color_off_);
  fill_rect(x + BRICK_W - 3, y + 1, 2, 2, color_off_);
  fill_rect(x + 1, y + BRICK_H - 3, 2, 2, color_off_);
  fill_rect(x + BRICK_W - 3, y + BRICK_H - 3, 2, 2, color_off_);
}

void GameBreakout::draw_bricks_() {
  for (int i = 0; i < BRICK_COUNT; i++) {
    Brick &brick = bricks_[i];

    if (brick.hp == 0) {
      continue;
    }

    int bx = brick.x;
    int by = brick.y;

    // Special rendering for different brick types
    if (brick.type == EXTRA_BALL) {
      draw_special_brick_corners_(bx, by);
      // Draw a ball (circle) on the left side
      draw_line(bx + 3, by + 1, bx + 5, by + 1, color_on_);
      fill_rect(bx + 2, by + 2, 5, 3, color_on_);
      draw_line(bx + 3, by + 5, bx + 5, by + 5, color_on_);
      // Draw plus sign on the right side
      int plus_cx = bx + BRICK_W - 5;
      int plus_cy = by + BRICK_H / 2;
      draw_line(plus_cx - 2, plus_cy, plus_cx + 2, plus_cy, color_on_);
      draw_line(plus_cx, plus_cy - 2, plus_cx, plus_cy + 2, color_on_);
      continue;
    }

    if (brick.type == SHIELD) {
      draw_special_brick_corners_(bx, by);
      int line_y = by + BRICK_H - 1;
      draw_line(bx + 4, line_y, bx + BRICK_W - 5, line_y, color_on_);
      continue;
    }

    if (brick.type == WIDER_PADDLE) {
      draw_special_brick_corners_(bx, by);
      int arrow_y = by + BRICK_H / 2;
      // Left arrow
      draw_line(bx + 2, arrow_y, bx + 5, arrow_y - 2, color_on_);
      draw_line(bx + 2, arrow_y, bx + 5, arrow_y + 2, color_on_);
      // Right arrow
      draw_line(bx + BRICK_W - 3, arrow_y, bx + BRICK_W - 6, arrow_y - 2, color_on_);
      draw_line(bx + BRICK_W - 3, arrow_y, bx + BRICK_W - 6, arrow_y + 2, color_on_);
      continue;
    }

    if (brick.type == EXTRA_LIFE) {
      draw_special_brick_corners_(bx, by);
      int heart_x = bx + (BRICK_W / 2) - 3;
      int heart_y = by + (BRICK_H / 2) - 2;
      draw_heart_(heart_x, heart_y);
      continue;
    }

    if (brick.type == WONKY_BRICKS) {
      // Draw wonky brick with wavy pattern
      for (int wx = 0; wx < BRICK_W; wx++) {
        for (int wy = 0; wy < BRICK_H; wy++) {
          if (((wx + wy + (frame_ / 2)) % 4) < 2) {
            draw_pixel(bx + wx, by + wy, color_on_);
          }
        }
      }
      continue;
    }

    if (brick.type == STATIC) {
      // Draw random pixels
      for (int p = 0; p < 35; ++p) {
        int rx = bx + (esp_random() % BRICK_W);
        int ry = by + (esp_random() % BRICK_H);
        draw_pixel(rx, ry, color_on_);
      }
      continue;
    }

    if (brick.type == SHOOTER) {
      draw_special_brick_corners_(bx, by);
      int line_y = by + BRICK_H - 1;
      draw_line(bx + 4, line_y, bx + BRICK_W - 5, line_y, color_on_);
      int center_x = bx + BRICK_W / 2;
      draw_pixel(center_x, line_y - 2, color_on_);
      draw_pixel(center_x, line_y - 4, color_on_);
      draw_pixel(center_x, line_y - 6, color_on_);
      continue;
    }

    if (brick.type == POWERUP_SHUFFLE) {
      draw_special_brick_corners_(bx, by);
      // Draw a question mark
      draw_line(bx + 6, by, bx + 8, by, color_on_);
      draw_pixel(bx + 5, by + 1, color_on_);
      draw_line(bx + 9, by + 1, bx + 9, by + 2, color_on_);
      draw_pixel(bx + 8, by + 3, color_on_);
      draw_pixel(bx + 7, by + 4, color_on_);
      draw_pixel(bx + 7, by + 6, color_on_);
      continue;
    }

    // Normal bricks - render based on HP
    if (brick.hp > 4) {
      fill_rect(bx, by, BRICK_W, BRICK_H, color_on_);
    } else if (brick.hp == 4) {
      draw_rect(bx, by, BRICK_W, BRICK_H, color_on_);
      fill_rect(bx + 2, by + 2, BRICK_W - 4, BRICK_H - 4, color_on_);
    } else if (brick.hp == 3) {
      draw_rect(bx, by, BRICK_W, BRICK_H, color_on_);
      fill_rect(bx + 2, by + 2, 2, BRICK_H - 4, color_on_);
      draw_line(bx + 5, by + 2, bx + 5, by + 4, color_on_);
      draw_line(bx + 7, by + 2, bx + 7, by + 4, color_on_);
      draw_line(bx + 9, by + 2, bx + 9, by + 4, color_on_);
      fill_rect(bx + 11, by + 2, 2, BRICK_H - 4, color_on_);
    } else if (brick.hp == 2) {
      draw_rect(bx, by, BRICK_W, BRICK_H, color_on_);
      draw_rect(bx + 2, by + 2, BRICK_W - 4, BRICK_H - 4, color_on_);
    } else if (brick.hp < 0) {
      draw_unbreakable_brick_(bx, by);
    } else {
      draw_rect(bx, by, BRICK_W, BRICK_H, color_on_);
    }
  }
}

void GameBreakout::draw_paddle_() {
  if (paddle_hit_) {
    fill_rect(paddle_x_, paddle_y_, paddle_w_, PADDLE_H, color_on_);
  } else {
    draw_rect(paddle_x_, paddle_y_, paddle_w_, PADDLE_H, color_on_);
  }
}

void GameBreakout::draw_shield_() {
  if (shield_amount_) {
    int shield_y = area_.h - 1;
    int x = 0;
    while (x < area_.w) {
      int dash = std::min(shield_amount_, area_.w - x);
      draw_line(x, shield_y, x + dash - 1, shield_y, color_on_);
      x += dash + 1;  // dash width + 1px gap
    }
  }
}

void GameBreakout::draw_projectiles_() {
  for (const auto &p : projectiles_) {
    fill_rect(p.x, p.y, 1, 4, color_on_);
  }
}

void GameBreakout::draw_balls_() {
  for (int b = 0; b < MAX_BALLS; b++) {
    if (balls_[b].alive) {
      fill_rect(balls_[b].x, balls_[b].y, BALL_SIZE, BALL_SIZE, color_on_);
    }
  }
}

void GameBreakout::draw_overlay_text_() {
  if (pause_frames_ > 0) {
    int overlay_h_padding = 10;
    int overlay_height = 30;
    int overlay_y_offset = 4;
    int text_overlay_y = (area_.h / 2 - overlay_height / 2) - overlay_y_offset;

    // Draw overlay background
    fill_rect(overlay_h_padding - 2, text_overlay_y - 2, area_.w - overlay_h_padding * 2 + 4, overlay_height + 4,
              color_off_);
    draw_rect(overlay_h_padding, text_overlay_y, area_.w - overlay_h_padding * 2, overlay_height, color_on_);

    // Determine text to show
    char text1[32] = "";
    char text2[32] = "";
    int text_center_x = area_.w / 2;
    int text_center_y = area_.h / 2 - overlay_y_offset;

    if (!level_started_) {
      snprintf(text1, sizeof(text1), "LEVEL %d", level_);
      snprintf(text2, sizeof(text2), "GET READY!");
    } else if (state_.lives > 0) {
      snprintf(text1, sizeof(text1), "BALLS: %d", state_.lives);
    } else {
      snprintf(text1, sizeof(text1), "SCORE:");
      snprintf(text2, sizeof(text2), "%d", score_);
    }

    // Draw text
    if (text2[0] != '\0') {
      draw_text(0, text_center_y - 8, text1, color_on_, LV_TEXT_ALIGN_CENTER);
      draw_text(0, text_center_y + 2, text2, color_on_, LV_TEXT_ALIGN_CENTER);
    } else {
      draw_text(0, text_center_y - 4, text1, color_on_, LV_TEXT_ALIGN_CENTER);
    }

    // Show pause progress bar
    int progress_bar_padding = 2;
    int progress_bar_max = (area_.w - overlay_h_padding * 2) - progress_bar_padding * 2;
    int progress_bar_width = (progress_bar_max * (PAUSE_DURATION - pause_frames_)) / PAUSE_DURATION;
    fill_rect(overlay_h_padding + progress_bar_padding, text_overlay_y + overlay_height - progress_bar_padding - 4,
              progress_bar_width, 2, color_on_);
  }
}

}  // namespace esphome::game_breakout
