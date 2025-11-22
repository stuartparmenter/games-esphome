// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#include "game_zaxxon.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>

namespace esphome::game_zaxxon {

static const char *const TAG = "game.zaxxon";

GameZaxxon::GameZaxxon()
    : world_scroll_z_(0),
      next_spawn_z_(SPAWN_DISTANCE),
      segment_counter_(0),
      fire_timer_(0),
      scroll_speed_(SCROLL_SPEED),
      up_held_(false),
      down_held_(false),
      left_held_(false),
      right_held_(false),
      fire_held_(false),
      autoplay_(false),
      canvas_w_(0),
      canvas_h_(0),
      ground_y_(0) {
  // Initialize colors (classic Zaxxon palette)
  color_ground_ = lv_color_hex(0x000040);      // Dark blue ground
  color_grid_ = lv_color_hex(0x0000AA);        // Blue grid lines
  color_player_ = lv_color_hex(0x00FF00);      // Green player ship
  color_shadow_ = lv_color_hex(0x003300);      // Dark green shadow
  color_projectile_ = lv_color_hex(0xFF0000);  // Red projectiles
  color_wall_ = lv_color_hex(0xAAAAAA);        // Gray walls
  color_barrier_ = lv_color_hex(0x00AAAA);     // Cyan barriers
  color_tower_ = lv_color_hex(0xAA00AA);       // Magenta towers
  color_enemy_ = lv_color_hex(0xFFAA00);       // Orange enemies
  color_fuel_ = lv_color_hex(0xFFFF00);        // Yellow fuel
  color_text_ = lv_color_hex(0xFFFFFF);        // White text

  projectiles_.reserve(MAX_PROJECTILES);
  obstacles_.reserve(MAX_OBSTACLES);
}

void GameZaxxon::on_bind(lv_obj_t *canvas) {
  GameBase::on_bind(canvas);
  ESP_LOGI(TAG, "Zaxxon game bound to canvas");
}

void GameZaxxon::on_resize(const Rect &r) {
  GameBase::on_resize(r);
  get_canvas_size(canvas_w_, canvas_h_);

  // Calculate exact 4:3 playfield with pillarbox/letterbox
  // Use k-based calculation: W=4k, H=3k ensures perfect 4:3 ratio with integer pixels
  float canvas_aspect = (float) canvas_w_ / canvas_h_;
  float target_aspect = 4.0f / 3.0f;

  if (canvas_aspect > target_aspect) {
    // Canvas is wider than 4:3 - pillarbox (black bars on sides)
    int k = canvas_h_ / 3;  // Find largest k where 3k ≤ canvas_h_
    playfield_w_ = 4 * k;   // W = 4k
    playfield_h_ = 3 * k;   // H = 3k (exact 4:3)
    playfield_x_ = (canvas_w_ - playfield_w_) / 2;
    playfield_y_ = 0;
  } else {
    // Canvas is taller than 4:3 - letterbox (black bars top/bottom)
    int k = canvas_w_ / 4;  // Find largest k where 4k ≤ canvas_w_
    playfield_w_ = 4 * k;   // W = 4k
    playfield_h_ = 3 * k;   // H = 3k (exact 4:3)
    playfield_x_ = 0;
    playfield_y_ = (canvas_h_ - playfield_h_) / 2;
  }

  // Calculate isometric projection scale to fit world coordinates in playfield
  // World X range: 0 to playfield_w_ (player and obstacles use pixel coordinates)
  // World Z range: -50 to +500 (DESPAWN_DISTANCE to SPAWN_DISTANCE + margin)
  // In isometric projection, both X and Z contribute to screen_x
  // We need: (max_world_x + max_relative_z) * scale = playfield_w_
  const float MAX_RELATIVE_Z = 500.0f;  // Far rendering distance
  iso_scale_ = playfield_w_ / (playfield_w_ + MAX_RELATIVE_Z);

  // Calculate scale factors based on playfield (baseline: 240x180 for 4:3)
  float width_scale = playfield_w_ / 240.0f;
  float height_scale = playfield_h_ / 180.0f;
  float min_scale = std::min(width_scale, height_scale);

  // Scale altitude based on playfield height (leave room for HUD)
  scaled_max_altitude_ = playfield_h_ * 0.55f;

  // Scale obstacles based on playfield width (keep them proportional)
  scaled_obstacle_min_size_ = OBSTACLE_MIN_SIZE * width_scale;
  scaled_obstacle_max_size_ = OBSTACLE_MAX_SIZE * width_scale;

  // Scale player and projectile sizes
  scaled_player_size_ = PLAYER_SIZE * min_scale;
  scaled_projectile_size_ = PROJECTILE_SIZE * min_scale;

  // Ground position relative to playfield bottom (leave room for fuel gauge)
  int fuel_gauge_space = std::max(8, (int) (12 * height_scale));
  ground_y_ = playfield_y_ + playfield_h_ - fuel_gauge_space - std::max(5, (int) (10 * height_scale));

  // HUD scaling - thin altimeter on LEFT side
  hud_meter_width_ = std::max(6, (int) (8 * min_scale));  // Thin like original
  hud_meter_y_ = playfield_y_ + std::max(5, (int) (10 * height_scale));
  hud_meter_height_ = std::min((int) (80 * height_scale), ground_y_ - hud_meter_y_ - 5);

  // Fuel gauge at playfield bottom
  fuel_meter_height_ = std::max(4, (int) (6 * height_scale));
  fuel_meter_y_ = playfield_y_ + playfield_h_ - fuel_meter_height_ - 2;

  ESP_LOGD(TAG, "Canvas %dx%d -> Playfield %dx%d at (%d,%d), ground_y=%d, max_alt=%.1f", canvas_w_, canvas_h_,
           playfield_w_, playfield_h_, playfield_x_, playfield_y_, ground_y_, scaled_max_altitude_);
}

void GameZaxxon::reset() {
  state_.reset();
  state_.lives = 3;

  // Reset player to starting position (left side of playfield, middle altitude)
  player_.reset(playfield_w_ * 0.2f, scaled_max_altitude_ / 2.0f, 0);

  // Clear projectiles and obstacles
  projectiles_.clear();
  obstacles_.clear();

  // Reset world state
  world_scroll_z_ = 0;
  next_spawn_z_ = SPAWN_DISTANCE;
  segment_counter_ = 0;
  fuel_ = 100.0f;  // Start with full fuel

  // Reset timers
  fire_timer_ = 0;
  scroll_speed_ = SCROLL_SPEED;

  // Reset input state
  up_held_ = false;
  down_held_ = false;
  left_held_ = false;
  right_held_ = false;
  fire_held_ = false;

  ESP_LOGI(TAG, "Game reset");
}

void GameZaxxon::on_input(const InputEvent &event) {
  // Toggle autoplay with SELECT button
  if (event.type == InputType::SELECT && event.pressed) {
    autoplay_ = !autoplay_;
    ESP_LOGI(TAG, "Autoplay %s", autoplay_ ? "enabled" : "disabled");
    return;
  }

  // Restart on game over
  if (state_.game_over && event.pressed) {
    reset();
    return;
  }

  // Track held inputs for continuous movement
  switch (event.type) {
    case InputType::UP:
      up_held_ = event.pressed;
      break;
    case InputType::DOWN:
      down_held_ = event.pressed;
      break;
    case InputType::LEFT:
      left_held_ = event.pressed;
      break;
    case InputType::RIGHT:
      right_held_ = event.pressed;
      break;
    case InputType::A:
    case InputType::B:
      fire_held_ = event.pressed;
      break;
    default:
      break;
  }
}

void GameZaxxon::step(float dt) {
  if (paused_)
    return;

  // Cap delta time to prevent large jumps
  if (dt > 0.1f)
    dt = 0.1f;

  // Game over state
  if (state_.game_over) {
    // Draw game over screen
    lv_canvas_fill_bg(canvas_, lv_color_hex(0x000000), LV_OPA_COVER);
    draw_playfield_background_();
    draw_ground_();

    // Show final score
    char score_text[32];
    snprintf(score_text, sizeof(score_text), "Score: %d", state_.score);
    draw_text(0, canvas_h_ / 2 - 15, "GAME OVER", color_text_, LV_TEXT_ALIGN_CENTER);
    draw_text(0, canvas_h_ / 2, score_text, color_text_, LV_TEXT_ALIGN_CENTER);
    draw_text(0, canvas_h_ / 2 + 15, "Press key", color_text_, LV_TEXT_ALIGN_CENTER);

    // Invalidate only playfield area for performance
    lv_area_t area;
    area.x1 = playfield_x_;
    area.y1 = playfield_y_;
    area.x2 = playfield_x_ + playfield_w_ - 1;
    area.y2 = playfield_y_ + playfield_h_ - 1;
    lv_obj_invalidate_area(canvas_, &area);
    return;
  }

  // Update world scrolling (camera moves forward = world_scroll_z increases)
  world_scroll_z_ += scroll_speed_ * dt;

  // Advance player Z to match world scroll (player maintains relative position)
  player_.z = world_scroll_z_;

  // Deplete fuel over time (classic Zaxxon mechanic)
  fuel_ -= 5.0f * dt;  // Lose ~5% per second
  if (fuel_ <= 0) {
    fuel_ = 0;
    state_.game_over = true;
    ESP_LOGI(TAG, "Game over - out of fuel! Final score: %d", state_.score);
    return;
  }

  // Update game entities
  update_player_(dt);
  update_projectiles_(dt);
  update_obstacles_(dt);
  check_collisions_();

  // Spawn new obstacles when we've scrolled far enough
  while (next_spawn_z_ < world_scroll_z_ + SPAWN_DISTANCE) {
    spawn_obstacle_();
    next_spawn_z_ += SEGMENT_LENGTH;
    segment_counter_++;
  }

  // Update fire cooldown
  if (fire_timer_ > 0) {
    fire_timer_ -= dt;
  }

  // Fire projectile when button held and cooldown expired
  if (fire_held_ && fire_timer_ <= 0 && projectiles_.size() < MAX_PROJECTILES) {
    Projectile proj;
    proj.fire(player_.x + PLAYER_SIZE / 2, player_.y, player_.z);
    projectiles_.push_back(proj);
    fire_timer_ = FIRE_COOLDOWN;
  }

  // Progressive difficulty
  float difficulty_factor = 1.0f + (state_.score / 2000.0f);
  scroll_speed_ = SCROLL_SPEED * difficulty_factor;

  // Render - black background like classic Zaxxon
  lv_canvas_fill_bg(canvas_, lv_color_hex(0x000000), LV_OPA_COVER);
  draw_playfield_background_();
  draw_ground_();
  draw_obstacles_();
  draw_player_();
  draw_projectiles_();
  draw_hud_();

  // Invalidate only playfield area for performance (33% reduction on 128x64)
  lv_area_t area;
  area.x1 = playfield_x_;
  area.y1 = playfield_y_;
  area.x2 = playfield_x_ + playfield_w_ - 1;
  area.y2 = playfield_y_ + playfield_h_ - 1;
  lv_obj_invalidate_area(canvas_, &area);
}

void GameZaxxon::spawn_obstacle_() {
  if (obstacles_.size() >= MAX_OBSTACLES)
    return;

  // Random obstacle type based on segment pattern
  int type_rand = (segment_counter_ + rand()) % 100;
  ObstacleType type;

  if (segment_counter_ % 5 == 0) {
    // Every 5th segment might have fuel
    type = ObstacleType::FUEL;
  } else if (type_rand < 30) {
    type = ObstacleType::BARRIER;  // 30% barriers
  } else if (type_rand < 55) {
    type = ObstacleType::TOWER;  // 25% towers
  } else if (type_rand < 75) {
    type = ObstacleType::ENEMY;  // 20% enemies
  } else {
    type = ObstacleType::WALL;  // 25% walls
  }

  // Random horizontal position within playfield (avoid edges)
  float x = scaled_obstacle_min_size_ + (rand() % (int) (playfield_w_ - 2 * scaled_obstacle_max_size_));

  // Random size
  float size = scaled_obstacle_min_size_ + (rand() % (int) (scaled_obstacle_max_size_ - scaled_obstacle_min_size_));

  // Spawn at next_spawn_z_
  Obstacle obs;
  obs.spawn(x, next_spawn_z_, type, size);

  // Override altitude ranges with scaled values
  switch (type) {
    case ObstacleType::BARRIER:
      obs.y_min = 0;
      obs.y_max = scaled_max_altitude_ * 0.25f;
      break;
    case ObstacleType::TOWER:
      obs.y_min = 0;
      obs.y_max = scaled_max_altitude_ * 0.8f;
      break;
    case ObstacleType::ENEMY:
      obs.y_min = scaled_max_altitude_ * 0.3f;
      obs.y_max = scaled_max_altitude_ * 0.5f;
      break;
    case ObstacleType::FUEL:
      obs.y_min = scaled_max_altitude_ * 0.2f;
      obs.y_max = scaled_max_altitude_ * 0.4f;
      break;
    case ObstacleType::WALL:
    default:
      obs.y_min = 0;
      obs.y_max = scaled_max_altitude_;
      break;
  }

  obstacles_.push_back(obs);
}

void GameZaxxon::update_player_(float dt) {
  // Autoplay AI (simple obstacle avoidance)
  if (autoplay_) {
    // Fly at middle altitude
    float target_y = scaled_max_altitude_ / 2.0f;
    if (player_.y < target_y - 10) {
      up_held_ = true;
      down_held_ = false;
    } else if (player_.y > target_y + 10) {
      down_held_ = true;
      up_held_ = false;
    } else {
      up_held_ = false;
      down_held_ = false;
    }

    // Simple horizontal centering within playfield
    float target_x = playfield_w_ / 3.0f;
    if (player_.x < target_x - 10) {
      right_held_ = true;
      left_held_ = false;
    } else if (player_.x > target_x + 10) {
      left_held_ = true;
      right_held_ = false;
    } else {
      left_held_ = false;
      right_held_ = false;
    }

    // Auto-fire
    fire_held_ = obstacles_.size() > 0;
  }

  // Update altitude velocity (Y axis = up/down)
  if (up_held_ && !down_held_) {
    player_.velocity_y = ALTITUDE_SPEED;
  } else if (down_held_ && !up_held_) {
    player_.velocity_y = -ALTITUDE_SPEED;
  } else {
    player_.velocity_y = 0;
  }

  // Update horizontal velocity (X axis = left/right)
  if (left_held_ && !right_held_) {
    player_.velocity_x = -PLAYER_SPEED;
  } else if (right_held_ && !left_held_) {
    player_.velocity_x = PLAYER_SPEED;
  } else {
    player_.velocity_x = 0;
  }

  // Apply velocities
  player_.y += player_.velocity_y * dt;
  player_.x += player_.velocity_x * dt;

  // Clamp altitude (can't go below ground or above max)
  player_.y = std::max(0.0f, std::min(scaled_max_altitude_, player_.y));

  // Clamp horizontal position (stay within playfield)
  player_.x = std::max(0.0f, std::min((float) (playfield_w_ - scaled_player_size_), player_.x));
}

void GameZaxxon::update_projectiles_(float dt) {
  // Update projectile positions (travel forward in +Z direction)
  for (auto &proj : projectiles_) {
    if (proj.active) {
      proj.z += PROJECTILE_SPEED * dt;

      // Deactivate if too far ahead
      if (proj.z > world_scroll_z_ + SPAWN_DISTANCE * 1.5f) {
        proj.active = false;
      }
    }
  }

  // Remove inactive projectiles
  projectiles_.erase(
      std::remove_if(projectiles_.begin(), projectiles_.end(), [](const Projectile &p) { return !p.active; }),
      projectiles_.end());
}

void GameZaxxon::update_obstacles_(float dt) {
  // Obstacles don't move - world scrolls past them
  // Remove obstacles that are behind the player
  obstacles_.erase(std::remove_if(obstacles_.begin(), obstacles_.end(),
                                  [this](const Obstacle &o) { return o.z < world_scroll_z_ + DESPAWN_DISTANCE; }),
                   obstacles_.end());
}

void GameZaxxon::check_collisions_() {
  // Check player collision with obstacles
  for (const auto &obs : obstacles_) {
    if (obs.collides_with(player_.x, player_.y, player_.z, scaled_player_size_)) {
      // Check if it's fuel (bonus) or obstacle (damage)
      if (obs.type == ObstacleType::FUEL) {
        // Refill fuel (classic Zaxxon mechanic)
        fuel_ = std::min(100.0f, fuel_ + 50.0f);  // Refill 50%, capped at 100%
        state_.add_score(200);
        ESP_LOGD(TAG, "Fuel collected! Fuel now: %.1f%%", fuel_);
        // Mark as destroyed so we don't collide again
        const_cast<Obstacle &>(obs).destroyed = true;
      } else {
        // Hit obstacle - lose life
        state_.lose_life();
        if (state_.lives == 0) {
          ESP_LOGI(TAG, "Game over! Final score: %d", state_.score);
        } else {
          ESP_LOGI(TAG, "Player hit! Lives remaining: %d", state_.lives);
          // Reset player position to safe spot
          player_.y = scaled_max_altitude_ / 2.0f;
          player_.x = canvas_w_ * 0.2f;
        }
        return;
      }
    }
  }

  // Check projectile collision with obstacles
  for (auto &proj : projectiles_) {
    if (!proj.active)
      continue;

    for (auto &obs : obstacles_) {
      if (obs.destroyed)
        continue;

      if (obs.collides_with(proj.x, proj.y, proj.z, scaled_projectile_size_)) {
        // Hit obstacle
        proj.active = false;

        // Only enemies and fuel tanks can be destroyed
        if (obs.type == ObstacleType::ENEMY) {
          obs.destroyed = true;
          state_.add_score(100);
          ESP_LOGD(TAG, "Enemy destroyed! Score: %d", state_.score);
        } else if (obs.type == ObstacleType::FUEL) {
          obs.destroyed = true;
          fuel_ = std::min(100.0f, fuel_ + 50.0f);  // Refill 50%, capped at 100%
          state_.add_score(200);
          ESP_LOGD(TAG, "Fuel shot! Fuel now: %.1f%%", fuel_);
        }
        break;
      }
    }
  }
}

void GameZaxxon::draw_playfield_background_() {
  // Draw playfield as isometric parallelogram (blue game area)
  // This creates the classic Zaxxon perspective view

  // Define 4 corners of playfield in world space
  // Near edge (close to camera) and far edge (receding into distance)
  float near_z = world_scroll_z_ - 50;                   // Slightly in front of player
  float far_z = world_scroll_z_ + SPAWN_DISTANCE + 100;  // Beyond visible area

  // Calculate screen positions for 4 corners
  int near_left_x, near_left_y;
  int near_right_x, near_right_y;
  int far_left_x, far_left_y;
  int far_right_x, far_right_y;

  // Project corners (Y=0 since this is ground level)
  to_isometric_(0, 0, near_z, near_left_x, near_left_y);
  to_isometric_(playfield_w_, 0, near_z, near_right_x, near_right_y);
  to_isometric_(playfield_w_, 0, far_z, far_right_x, far_right_y);
  to_isometric_(0, 0, far_z, far_left_x, far_left_y);

  // Draw parallelogram with LVGL polygon
  lv_point_t points[] = {{(lv_coord_t) near_left_x, (lv_coord_t) near_left_y},
                         {(lv_coord_t) near_right_x, (lv_coord_t) near_right_y},
                         {(lv_coord_t) far_right_x, (lv_coord_t) far_right_y},
                         {(lv_coord_t) far_left_x, (lv_coord_t) far_left_y}};

  lv_draw_rect_dsc_t rect_dsc;
  lv_draw_rect_dsc_init(&rect_dsc);
  rect_dsc.bg_color = color_ground_;
  rect_dsc.bg_opa = LV_OPA_COVER;
  rect_dsc.border_width = 0;

  lv_canvas_draw_polygon(canvas_, points, 4, &rect_dsc);
}

void GameZaxxon::draw_ground_() {
  // Draw isometric grid on ground plane
  // Grid lines recede into distance (upper-right direction in isometric view)

  int grid_spacing = 30;

  // Draw horizontal grid lines (parallel to X axis)
  for (int z = -100; z < (int) SPAWN_DISTANCE; z += grid_spacing) {
    float world_z = world_scroll_z_ + z;
    int x1, y1, x2, y2;

    // Line across playfield at this Z depth
    to_isometric_(0, 0, world_z, x1, y1);
    to_isometric_(playfield_w_, 0, world_z, x2, y2);

    if (y1 < ground_y_ && y1 > 0) {
      draw_line(x1, y1, x2, y2, color_grid_);
    }
  }

  // Draw vertical grid lines (parallel to Z axis)
  for (int x = 0; x < playfield_w_; x += grid_spacing) {
    int x1, y1, x2, y2;

    // Line from near to far at this X position
    to_isometric_(x, 0, world_scroll_z_ - 50, x1, y1);
    to_isometric_(x, 0, world_scroll_z_ + SPAWN_DISTANCE, x2, y2);

    if (y1 > 0 && y2 > 0) {
      draw_line(x1, y1, x2, y2, color_grid_);
    }
  }
}

void GameZaxxon::draw_obstacles_() {
  // Sort obstacles by Z (draw far to near for proper layering)
  std::vector<const Obstacle *> sorted_obs;
  for (const auto &obs : obstacles_) {
    if (!obs.destroyed) {
      sorted_obs.push_back(&obs);
    }
  }

  std::sort(sorted_obs.begin(), sorted_obs.end(), [](const Obstacle *a, const Obstacle *b) { return a->z < b->z; });

  // Draw each obstacle as isometric block
  for (const auto *obs : sorted_obs) {
    // Choose color based on type
    lv_color_t color;
    switch (obs->type) {
      case ObstacleType::ENEMY:
        color = color_enemy_;
        break;
      case ObstacleType::FUEL:
        color = color_fuel_;
        break;
      case ObstacleType::BARRIER:
        color = color_barrier_;
        break;
      case ObstacleType::TOWER:
        color = color_tower_;
        break;
      case ObstacleType::WALL:
      default:
        color = color_wall_;
        break;
    }

    draw_iso_block_(obs->x, obs->z, obs->y_min, obs->y_max, obs->width, obs->depth, color);
  }
}

void GameZaxxon::draw_iso_block_(float x, float z, float y_min, float y_max, float width, float depth,
                                 lv_color_t color) {
  // Draw isometric block showing top, front, and side faces using LVGL polygon drawing

  // Calculate 8 corners of the block in screen space
  int x1, y1, x2, y2, x3, y3, x4, y4;  // Bottom corners
  int x5, y5, x6, y6, x7, y7, x8, y8;  // Top corners

  // Bottom face corners (at y_min altitude)
  to_isometric_(x, y_min, z, x1, y1);                  // Near-left
  to_isometric_(x + width, y_min, z, x2, y2);          // Near-right
  to_isometric_(x + width, y_min, z + depth, x3, y3);  // Far-right
  to_isometric_(x, y_min, z + depth, x4, y4);          // Far-left

  // Top face corners (at y_max altitude)
  to_isometric_(x, y_max, z, x5, y5);                  // Near-left
  to_isometric_(x + width, y_max, z, x6, y6);          // Near-right
  to_isometric_(x + width, y_max, z + depth, x7, y7);  // Far-right
  to_isometric_(x, y_max, z + depth, x8, y8);          // Far-left

  // Initialize drawing descriptors
  lv_draw_rect_dsc_t rect_dsc;
  lv_draw_rect_dsc_init(&rect_dsc);

  // Draw top face (parallelogram: 5-6-7-8) - lighter
  lv_point_t top_points[] = {{(lv_coord_t) x5, (lv_coord_t) y5},
                             {(lv_coord_t) x6, (lv_coord_t) y6},
                             {(lv_coord_t) x7, (lv_coord_t) y7},
                             {(lv_coord_t) x8, (lv_coord_t) y8}};
  rect_dsc.bg_color = lv_color_lighten(color, LV_OPA_40);
  rect_dsc.bg_opa = LV_OPA_COVER;
  rect_dsc.border_width = 0;
  lv_canvas_draw_polygon(canvas_, top_points, 4, &rect_dsc);

  // Draw front face (parallelogram: 1-2-6-5) - base color
  lv_point_t front_points[] = {{(lv_coord_t) x1, (lv_coord_t) y1},
                               {(lv_coord_t) x2, (lv_coord_t) y2},
                               {(lv_coord_t) x6, (lv_coord_t) y6},
                               {(lv_coord_t) x5, (lv_coord_t) y5}};
  rect_dsc.bg_color = color;
  lv_canvas_draw_polygon(canvas_, front_points, 4, &rect_dsc);

  // Draw side face (parallelogram: 2-3-7-6) - darker
  lv_point_t side_points[] = {{(lv_coord_t) x2, (lv_coord_t) y2},
                              {(lv_coord_t) x3, (lv_coord_t) y3},
                              {(lv_coord_t) x7, (lv_coord_t) y7},
                              {(lv_coord_t) x6, (lv_coord_t) y6}};
  rect_dsc.bg_color = lv_color_darken(color, LV_OPA_40);
  lv_canvas_draw_polygon(canvas_, side_points, 4, &rect_dsc);

  // Draw outline for definition (top face edges only)
  lv_draw_line_dsc_t line_dsc;
  lv_draw_line_dsc_init(&line_dsc);
  line_dsc.color = lv_color_hex(0x000000);
  line_dsc.width = 1;

  lv_point_t line_points[2];

  line_points[0] = {(lv_coord_t) x5, (lv_coord_t) y5};
  line_points[1] = {(lv_coord_t) x6, (lv_coord_t) y6};
  lv_canvas_draw_line(canvas_, line_points, 2, &line_dsc);

  line_points[0] = {(lv_coord_t) x6, (lv_coord_t) y6};
  line_points[1] = {(lv_coord_t) x7, (lv_coord_t) y7};
  lv_canvas_draw_line(canvas_, line_points, 2, &line_dsc);

  line_points[0] = {(lv_coord_t) x7, (lv_coord_t) y7};
  line_points[1] = {(lv_coord_t) x8, (lv_coord_t) y8};
  lv_canvas_draw_line(canvas_, line_points, 2, &line_dsc);

  line_points[0] = {(lv_coord_t) x8, (lv_coord_t) y8};
  line_points[1] = {(lv_coord_t) x5, (lv_coord_t) y5};
  lv_canvas_draw_line(canvas_, line_points, 2, &line_dsc);
}

void GameZaxxon::draw_player_() {
  // Calculate player position in screen space
  int player_x, player_y;
  to_isometric_(player_.x, player_.y, player_.z, player_x, player_y);

  // Draw shadow on ground (directly below player)
  int shadow_x, shadow_y;
  to_isometric_(player_.x, 0, player_.z, shadow_x, shadow_y);

  // Shadow as small filled rectangle
  fill_rect(shadow_x, shadow_y, scaled_player_size_, scaled_player_size_ / 2, color_shadow_);

  // Draw player ship as simple aircraft shape
  // Main body (rectangle)
  fill_rect(player_x, player_y, scaled_player_size_, scaled_player_size_, color_player_);

  // Wings (horizontal line)
  int wing_extend = std::max(2.0f, scaled_player_size_ * 0.4f);
  draw_line(player_x - wing_extend, player_y + scaled_player_size_ / 2, player_x + scaled_player_size_ + wing_extend,
            player_y + scaled_player_size_ / 2, color_player_);

  // Nose (small forward point)
  draw_pixel(player_x + scaled_player_size_, player_y + scaled_player_size_ / 2, lv_color_hex(0xFFFFFF));
}

void GameZaxxon::draw_projectiles_() {
  for (const auto &proj : projectiles_) {
    if (!proj.active)
      continue;

    int proj_x, proj_y;
    to_isometric_(proj.x, proj.y, proj.z, proj_x, proj_y);

    // Draw projectile as small filled square
    fill_rect(proj_x, proj_y, scaled_projectile_size_, scaled_projectile_size_, color_projectile_);
  }
}

void GameZaxxon::draw_hud_() {
  // Draw score at top center of playfield
  char score_text[32];
  snprintf(score_text, sizeof(score_text), "S:%d", state_.score);
  draw_text(0, playfield_y_ + 2, score_text, color_text_, LV_TEXT_ALIGN_CENTER);

  // Draw lives at top right of playfield (if there's room)
  if (canvas_h_ > 32) {
    char lives_text[16];
    snprintf(lives_text, sizeof(lives_text), "L:%d", state_.lives);
    draw_text(playfield_x_ + playfield_w_ - 25, playfield_y_ + 2, lives_text, color_text_, LV_TEXT_ALIGN_LEFT);
  }

  // Draw thin altimeter on LEFT side (classic Zaxxon style)
  if (hud_meter_height_ > 20) {
    int meter_x = playfield_x_ + 2;  // Just inside left playfield edge

    // Meter outline
    draw_rect(meter_x, hud_meter_y_, hud_meter_width_, hud_meter_height_, color_text_);

    // Altitude indicator bar (fills from bottom)
    float altitude_ratio = player_.y / scaled_max_altitude_;
    int bar_h = altitude_ratio * hud_meter_height_;
    if (bar_h > 0) {
      fill_rect(meter_x + 1, hud_meter_y_ + hud_meter_height_ - bar_h, hud_meter_width_ - 2, bar_h, color_player_);
    }
  }

  // Draw horizontal fuel gauge at BOTTOM (classic Zaxxon style)
  int fuel_bar_width = playfield_w_ - 20;  // Leave margins
  int fuel_bar_x = playfield_x_ + 10;

  // Fuel gauge outline
  draw_rect(fuel_bar_x, fuel_meter_y_, fuel_bar_width, fuel_meter_height_, color_text_);

  // Fuel level bar
  float fuel_ratio = fuel_ / 100.0f;
  int fuel_fill_w = fuel_ratio * (fuel_bar_width - 4);
  if (fuel_fill_w > 0) {
    // Color changes based on fuel level (red when low)
    lv_color_t fuel_color;
    if (fuel_ < 20.0f) {
      fuel_color = lv_color_hex(0xFF0000);  // Red when critical
    } else if (fuel_ < 50.0f) {
      fuel_color = lv_color_hex(0xFFAA00);  // Orange when low
    } else {
      fuel_color = color_fuel_;  // Normal color
    }
    fill_rect(fuel_bar_x + 2, fuel_meter_y_ + 2, fuel_fill_w, fuel_meter_height_ - 4, fuel_color);
  }
}

void GameZaxxon::to_isometric_(float world_x, float world_y, float world_z, int &screen_x, int &screen_y) {
  // Isometric projection with uniform scaling to fit world in exact 4:3 playfield
  // Both X and Z contribute equally to screen position, scaled together

  // Adjust Z relative to camera/scroll position
  float relative_z = world_z - world_scroll_z_;

  // Apply isometric projection with uniform scaling
  // iso_scale_ ensures (world_x + relative_z) fits exactly within playfield_w_
  // This keeps all rendering within the 4:3 boundaries
  screen_x = playfield_x_ + (world_x + relative_z) * iso_scale_;

  // Y projection: objects higher up appear higher on screen, with isometric depth
  // Apply same scale to maintain consistent isometric angle
  screen_y = ground_y_ - world_y + (world_x - relative_z) * iso_scale_ * 0.5f;
}

}  // namespace esphome::game_zaxxon
