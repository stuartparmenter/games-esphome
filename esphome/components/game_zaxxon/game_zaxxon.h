// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#pragma once

#include "../lvgl_game_runner/game_base.h"
#include "../lvgl_game_runner/game_state.h"
#include <vector>

namespace esphome::game_zaxxon {

using lvgl_game_runner::GameBase;
using lvgl_game_runner::GameState;
using lvgl_game_runner::InputEvent;
using lvgl_game_runner::InputType;

// Game constants
static constexpr float PLAYER_SPEED = 80.0f;        // Horizontal speed (pixels/sec)
static constexpr float ALTITUDE_SPEED = 100.0f;     // Vertical speed (pixels/sec)
static constexpr float SCROLL_SPEED = 40.0f;        // World scroll speed (pixels/sec)
static constexpr float PROJECTILE_SPEED = 100.0f;   // Projectile speed (pixels/sec)
static constexpr float MAX_ALTITUDE = 120.0f;       // Maximum flight altitude
static constexpr float PLAYER_SIZE = 8.0f;          // Player ship size
static constexpr float PROJECTILE_SIZE = 3.0f;      // Projectile size
static constexpr int MAX_PROJECTILES = 5;           // Max simultaneous projectiles
static constexpr int MAX_OBSTACLES = 30;            // Max obstacles on screen
static constexpr float SEGMENT_LENGTH = 100.0f;     // Distance between level segments
static constexpr float OBSTACLE_MIN_SIZE = 15.0f;   // Minimum obstacle size
static constexpr float OBSTACLE_MAX_SIZE = 40.0f;   // Maximum obstacle size
static constexpr float FIRE_COOLDOWN = 0.25f;       // Time between shots (seconds)
static constexpr float SPAWN_DISTANCE = 400.0f;     // How far ahead to spawn obstacles
static constexpr float DESPAWN_DISTANCE = -100.0f;  // When to remove obstacles behind player

// Player state (X = horizontal, Y = altitude, Z = depth/forward)
struct Player {
  float x;           // Horizontal position (left/right)
  float y;           // Altitude (height above ground)
  float z;           // Depth position (forward progression through level)
  float velocity_x;  // Horizontal velocity
  float velocity_y;  // Altitude velocity

  Player() : x(0), y(60.0f), z(0), velocity_x(0), velocity_y(0) {}

  void reset(float start_x, float start_y, float start_z) {
    x = start_x;
    y = start_y;
    z = start_z;
    velocity_x = 0;
    velocity_y = 0;
  }
};

// Projectile (X = horizontal, Y = altitude, Z = depth)
struct Projectile {
  float x;
  float y;
  float z;
  bool active;

  Projectile() : x(0), y(0), z(0), active(false) {}

  void fire(float start_x, float start_y, float start_z) {
    x = start_x;
    y = start_y;
    z = start_z;
    active = true;
  }
};

// Obstacle types
enum class ObstacleType {
  WALL,     // Full height wall (0 to MAX_ALTITUDE)
  BARRIER,  // Low barrier (floor obstacle)
  TOWER,    // Tall tower (goes high)
  ENEMY,    // Flying enemy (can be destroyed)
  FUEL      // Fuel tank (bonus points)
};

// Obstacle (X = horizontal, Z = depth, Y = height range)
struct Obstacle {
  float x;      // Horizontal position
  float z;      // Depth position
  float y_min;  // Minimum height
  float y_max;  // Maximum height
  float width;  // Horizontal width
  float depth;  // Depth size (Z dimension)
  ObstacleType type;
  bool destroyed;

  Obstacle() : x(0), z(0), y_min(0), y_max(0), width(0), depth(0), type(ObstacleType::WALL), destroyed(false) {}

  void spawn(float spawn_x, float spawn_z, ObstacleType spawn_type, float size) {
    x = spawn_x;
    z = spawn_z;
    type = spawn_type;
    width = size;
    depth = size;
    destroyed = false;

    // Set height range based on type
    switch (type) {
      case ObstacleType::BARRIER:
        y_min = 0;
        y_max = MAX_ALTITUDE * 0.25f;
        break;
      case ObstacleType::TOWER:
        y_min = 0;
        y_max = MAX_ALTITUDE * 0.8f;
        break;
      case ObstacleType::ENEMY:
        y_min = MAX_ALTITUDE * 0.3f;
        y_max = MAX_ALTITUDE * 0.5f;
        break;
      case ObstacleType::FUEL:
        y_min = MAX_ALTITUDE * 0.2f;
        y_max = MAX_ALTITUDE * 0.4f;
        break;
      case ObstacleType::WALL:
      default:
        y_min = 0;
        y_max = MAX_ALTITUDE;
        break;
    }
  }

  bool is_active() const { return !destroyed; }

  bool collides_with(float px, float py, float pz, float psize) const {
    if (destroyed)
      return false;

    // Check 3D bounding box collision
    bool x_overlap = px + psize > x && px < x + width;
    bool z_overlap = pz + psize > z && pz < z + depth;
    bool y_overlap = py + psize > y_min && py < y_max;

    return x_overlap && z_overlap && y_overlap;
  }
};

class GameZaxxon : public GameBase {
 public:
  GameZaxxon();
  ~GameZaxxon() override = default;

  void on_bind(lv_obj_t *canvas) override;
  void on_resize(const Rect &r) override;
  void step(float dt) override;
  void on_input(const InputEvent &event) override;
  void reset() override;

 private:
  // Helper functions
  void spawn_obstacle_();
  void update_player_(float dt);
  void update_projectiles_(float dt);
  void update_obstacles_(float dt);
  void check_collisions_();
  void draw_playfield_background_();
  void draw_ground_();
  void draw_obstacles_();
  void draw_player_();
  void draw_projectiles_();
  void draw_hud_();
  void to_isometric_(float world_x, float world_y, float world_z, int &screen_x, int &screen_y);
  void draw_iso_block_(float x, float z, float y_min, float y_max, float width, float depth, lv_color_t color);

  // Game state
  GameState state_;
  Player player_;
  std::vector<Projectile> projectiles_;
  std::vector<Obstacle> obstacles_;

  // World state
  float world_scroll_z_;  // Current Z position of camera/world
  float next_spawn_z_;    // Z position for next obstacle spawn
  int segment_counter_;   // Current segment number
  float fuel_;            // Current fuel level (0-100)

  // Timing
  float fire_timer_;
  float scroll_speed_;

  // Input state
  bool up_held_;
  bool down_held_;
  bool left_held_;
  bool right_held_;
  bool fire_held_;
  bool autoplay_;

  // Cached dimensions
  int canvas_w_;
  int canvas_h_;
  int ground_y_;     // Ground baseline on screen
  int playfield_x_;  // Left edge of playfield (for black bars)
  int playfield_y_;  // Top edge of playfield
  int playfield_w_;  // Width of playfield
  int playfield_h_;  // Height of playfield

  // Scaled constants (calculated based on canvas size)
  float scaled_max_altitude_;
  float scaled_obstacle_min_size_;
  float scaled_obstacle_max_size_;
  float scaled_player_size_;
  float scaled_projectile_size_;
  int hud_meter_height_;
  int hud_meter_y_;
  int hud_meter_width_;    // Altimeter width (thin, on left)
  int fuel_meter_height_;  // Fuel gauge height (at bottom)
  int fuel_meter_y_;       // Fuel gauge Y position
  float iso_scale_;        // Isometric projection scale to fit world in playfield

  // Colors
  lv_color_t color_ground_;
  lv_color_t color_grid_;
  lv_color_t color_player_;
  lv_color_t color_shadow_;
  lv_color_t color_projectile_;
  lv_color_t color_wall_;
  lv_color_t color_barrier_;
  lv_color_t color_tower_;
  lv_color_t color_enemy_;
  lv_color_t color_fuel_;
  lv_color_t color_text_;
};

}  // namespace esphome::game_zaxxon
