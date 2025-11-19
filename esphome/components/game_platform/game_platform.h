#pragma once

#include "esphome/components/lvgl_game_runner/game_base.h"
#include "esphome/components/lvgl_game_runner/ai_controller.h"
#include "esphome/components/lvgl_game_runner/game_state.h"
#include <vector>
#include <array>
#include <memory>

namespace esphome {
namespace game_platform {

static const char *const TAG = "game_platform";

// Maximum constants
static constexpr int MAX_PLAYERS = 4;
static constexpr int MAX_ENEMIES = 50;
static constexpr int MAX_POWERUPS = 20;
static constexpr int MAX_PROJECTILES = 30;
static constexpr int MAX_PARTICLES = 100;

// Difficulty levels
enum class Difficulty : uint8_t {
  EASY = 0,
  NORMAL = 1,
  HARD = 2,
  EXTREME = 3
};

// AI difficulty levels
enum class AIDifficulty : uint8_t {
  BEGINNER = 0,
  INTERMEDIATE = 1,
  ADVANCED = 2,
  EXPERT = 3
};

// Level types
enum class LevelType : uint8_t {
  NORMAL = 0,
  BOSS = 1,
  HIDDEN = 2,
  BONUS = 3
};

// Tile types for level generation
enum class TileType : uint8_t {
  EMPTY = 0,
  SOLID = 1,
  PLATFORM = 2,        // One-way platform (can jump through from below)
  SPIKE = 3,
  LADDER = 4,
  BREAKABLE = 5,
  BOUNCY = 6,
  ICE = 7,             // Slippery surface
  CONVEYOR_LEFT = 8,
  CONVEYOR_RIGHT = 9,
  CHECKPOINT = 10,
  GOAL = 11,
  HIDDEN_BLOCK = 12,   // Reveals when hit from below
  MOVING_PLATFORM = 13,
  CRUMBLING = 14,      // Falls after stepped on
  WATER = 15
};

// Enemy types
enum class EnemyType : uint8_t {
  WALKER = 0,          // Walks back and forth
  FLYER = 1,           // Flies in patterns
  JUMPER = 2,          // Hops around
  SHOOTER = 3,         // Shoots projectiles
  CHASER = 4,          // Chases nearest player
  BOSS_GIANT = 5,      // Large boss that stomps
  BOSS_FLYING = 6,     // Flying boss with projectiles
  BOSS_SPAWNER = 7,    // Boss that spawns minions
  TURRET = 8,          // Stationary shooter
  PATROL = 9           // Patrols fixed path
};

// Powerup types
enum class PowerupType : uint8_t {
  // Positive powerups
  HEALTH = 0,
  EXTRA_LIFE = 1,
  SPEED_BOOST = 2,
  JUMP_BOOST = 3,
  INVINCIBILITY = 4,
  DOUBLE_JUMP = 5,
  COIN = 6,
  KEY = 7,             // For locked doors
  SHIELD = 8,
  MAGNET = 9,          // Attracts coins

  // Powerdowns (negative effects)
  SLOW = 10,
  REVERSE_CONTROLS = 11,
  LOW_JUMP = 12,
  SHRINK = 13,
  CONFUSION = 14       // Random control swaps
};

// Projectile types
enum class ProjectileType : uint8_t {
  PLAYER_SHOT = 0,
  ENEMY_SHOT = 1,
  BOSS_SHOT = 2,
  FIREBALL = 3
};

// Player state
struct Player {
  float x, y;                    // Position
  float vx, vy;                  // Velocity
  float last_x, last_y;          // For rendering optimization
  int width, height;             // Hitbox size
  uint8_t lives;
  uint32_t score;
  bool active;
  bool on_ground;
  bool on_ladder;
  bool in_water;
  bool facing_right;
  int jumps_remaining;           // For double jump
  float invincibility_timer;
  float speed_modifier;
  float jump_modifier;
  bool has_shield;
  bool has_magnet;
  bool controls_reversed;
  int keys_collected;
  uint8_t checkpoint_x, checkpoint_y;  // Respawn point
  uint8_t player_num;            // 0-3
  lv_color_t color;

  void reset() {
    vx = vy = 0.0f;
    active = true;
    on_ground = false;
    on_ladder = false;
    in_water = false;
    facing_right = true;
    jumps_remaining = 1;
    invincibility_timer = 0.0f;
    speed_modifier = 1.0f;
    jump_modifier = 1.0f;
    has_shield = false;
    has_magnet = false;
    controls_reversed = false;
    keys_collected = 0;
  }
};

// Enemy state
struct Enemy {
  float x, y;
  float vx, vy;
  float last_x, last_y;
  int width, height;
  EnemyType type;
  bool active;
  int health;
  int max_health;
  float state_timer;
  float shoot_timer;
  int patrol_start_x, patrol_end_x;
  int patrol_start_y, patrol_end_y;
  bool moving_right;
  bool moving_down;
  uint8_t phase;                 // For boss patterns

  void reset() {
    vx = vy = 0.0f;
    active = true;
    state_timer = 0.0f;
    shoot_timer = 0.0f;
    moving_right = true;
    moving_down = true;
    phase = 0;
  }
};

// Powerup/powerdown state
struct Powerup {
  float x, y;
  float last_x, last_y;
  int width, height;
  PowerupType type;
  bool active;
  bool is_hidden;                // For hidden blocks
  float duration;                // How long effect lasts (0 = instant)

  bool is_negative() const {
    return type >= PowerupType::SLOW;
  }
};

// Projectile state
struct Projectile {
  float x, y;
  float vx, vy;
  ProjectileType type;
  bool active;
  uint8_t owner_player;          // 255 for enemy projectiles

  void reset() {
    active = false;
  }
};

// Moving platform state
struct MovingPlatform {
  float x, y;
  float last_x, last_y;
  int width, height;
  float start_x, start_y;
  float end_x, end_y;
  float speed;
  bool moving_forward;
  bool active;
};

// Particle for visual effects
struct Particle {
  float x, y;
  float vx, vy;
  float life;
  lv_color_t color;
  bool active;
};

// AI Controller for platform game
class PlatformAI : public lvgl_game_runner::AIController {
 public:
  explicit PlatformAI(uint8_t player_num, AIDifficulty difficulty);

  lvgl_game_runner::InputEvent update(float dt, const lvgl_game_runner::GameState &state,
                                      const lvgl_game_runner::GameBase *game) override;
  void reset() override;

 private:
  uint8_t player_num_;
  AIDifficulty difficulty_;
  float reaction_delay_;
  float decision_timer_;
  float current_delay_;
  bool holding_jump_;
  bool holding_left_;
  bool holding_right_;
  lvgl_game_runner::InputType last_input_;

  // AI decision making
  void make_decision_(const Player &player, const std::vector<Enemy> &enemies,
                     const std::vector<Powerup> &powerups, int goal_x);
  float get_reaction_time_() const;
  float get_accuracy_() const;
};

// Main game class
class GamePlatform : public lvgl_game_runner::GameBase {
 public:
  GamePlatform();

  // Lifecycle methods
  void on_bind(lv_obj_t *canvas) override;
  void on_resize(const lvgl_game_runner::Rect &r) override;
  void step(float dt) override;
  void reset() override;
  void on_input(const lvgl_game_runner::InputEvent &event) override;
  void pause() override;
  void resume() override;

  // Configuration setters
  void set_seed(uint32_t seed) { base_seed_ = seed; }
  void set_level_dimensions(int width, int height) {
    level_width_tiles_ = width;
    level_height_tiles_ = height;
  }
  void set_difficulty(int difficulty) { difficulty_ = static_cast<Difficulty>(difficulty); }
  void set_ai_difficulty(int difficulty) { ai_difficulty_ = static_cast<AIDifficulty>(difficulty); }
  void set_initial_lives(int lives) { initial_lives_ = lives; }
  void set_hidden_levels_enabled(bool enabled) { hidden_levels_enabled_ = enabled; }
  void set_boss_level_frequency(int freq) { boss_level_frequency_ = freq; }
  void set_player_speed_multiplier(float mult) { player_speed_mult_ = mult; }
  void set_jump_strength_multiplier(float mult) { jump_strength_mult_ = mult; }
  void set_gravity_multiplier(float mult) { gravity_mult_ = mult; }
  void set_double_jump_enabled(bool enabled) { double_jump_enabled_ = enabled; }
  void set_wall_jump_enabled(bool enabled) { wall_jump_enabled_ = enabled; }
  void set_friendly_fire(bool enabled) { friendly_fire_ = enabled; }
  void set_shared_lives(bool enabled) { shared_lives_ = enabled; }
  void set_max_enemies(int max) { max_enemies_per_level_ = max; }
  void set_powerup_frequency(float freq) { powerup_frequency_ = freq; }

  // Override for multiplayer
  uint8_t get_max_players() const override { return MAX_PLAYERS; }

  // Public accessors for AI
  const Player &get_player(int index) const { return players_[index]; }
  const std::vector<Enemy> &get_enemies() const { return enemies_; }
  const std::vector<Powerup> &get_powerups() const { return powerups_; }
  int get_goal_x() const { return goal_x_; }
  int get_camera_x() const { return camera_x_; }
  int get_camera_y() const { return camera_y_; }

 protected:
  // Random number generation
  uint32_t xorshift32_();
  float rand01_();
  int rand_range_(int min, int max);

  // Level generation
  void generate_level_();
  void generate_normal_level_();
  void generate_boss_level_();
  void generate_hidden_level_();
  void place_platforms_();
  void place_enemies_();
  void place_powerups_();
  void place_moving_platforms_();
  LevelType determine_level_type_();

  // Physics and collision
  void update_physics_(float dt);
  void update_player_physics_(Player &player, float dt);
  void update_enemy_physics_(Enemy &enemy, float dt);
  void update_projectiles_(float dt);
  void update_moving_platforms_(float dt);
  void update_particles_(float dt);
  bool check_tile_collision_(float x, float y, int w, int h, bool check_platforms = true);
  TileType get_tile_at_(int pixel_x, int pixel_y);
  void set_tile_at_(int tile_x, int tile_y, TileType type);

  // Collision detection
  void check_collisions_();
  void check_player_enemy_collision_(Player &player);
  void check_player_powerup_collision_(Player &player);
  void check_player_projectile_collision_(Player &player);
  void check_player_player_collision_();
  void check_projectile_enemy_collision_();
  bool rectangles_overlap_(float x1, float y1, int w1, int h1,
                          float x2, float y2, int w2, int h2);

  // Player actions
  void player_jump_(Player &player);
  void player_shoot_(Player &player);
  void apply_powerup_(Player &player, const Powerup &powerup);
  void damage_player_(Player &player);
  void respawn_player_(Player &player);

  // Enemy AI
  void update_enemies_(float dt);
  void update_enemy_ai_(Enemy &enemy, float dt);
  void enemy_shoot_(Enemy &enemy);
  void damage_enemy_(Enemy &enemy, int damage);
  void spawn_boss_minions_(Enemy &boss);

  // AI players
  void update_ai_players_(float dt);

  // Game state
  void check_level_complete_();
  void advance_to_next_level_();
  void check_game_over_();
  void add_score_(uint32_t points);

  // Rendering
  void render_();
  void render_background_();
  void render_tiles_();
  void render_moving_platforms_();
  void render_enemies_();
  void render_powerups_();
  void render_players_();
  void render_projectiles_();
  void render_particles_();
  void render_hud_();
  void render_pause_screen_();
  void render_game_over_();

  // Rendering helpers
  void draw_tile_(int tile_x, int tile_y);
  void draw_player_(const Player &player);
  void draw_enemy_(const Enemy &enemy);
  void draw_powerup_(const Powerup &powerup);
  void erase_rect_(int x, int y, int w, int h);

  // Camera
  void update_camera_();
  int world_to_screen_x_(float world_x);
  int world_to_screen_y_(float world_y);
  bool is_on_screen_(float x, float y, int w, int h);

  // Utility
  void spawn_particles_(float x, float y, int count, lv_color_t color);
  void play_sound_(int sound_id);

 private:
  // Configuration
  uint32_t base_seed_;
  uint32_t rng_state_;
  int level_width_tiles_;
  int level_height_tiles_;
  Difficulty difficulty_;
  AIDifficulty ai_difficulty_;
  int initial_lives_;
  bool hidden_levels_enabled_;
  int boss_level_frequency_;
  float player_speed_mult_;
  float jump_strength_mult_;
  float gravity_mult_;
  bool double_jump_enabled_;
  bool wall_jump_enabled_;
  bool friendly_fire_;
  bool shared_lives_;
  int max_enemies_per_level_;
  float powerup_frequency_;

  // Computed physics values
  float base_speed_;
  float base_jump_;
  float gravity_;
  int tile_size_;

  // Game state
  int current_level_;
  LevelType current_level_type_;
  bool level_complete_;
  float level_complete_timer_;
  int total_score_;
  int shared_lives_count_;
  bool game_over_;
  bool victory_;

  // Level data
  std::vector<TileType> level_tiles_;
  int level_pixel_width_;
  int level_pixel_height_;
  int goal_x_, goal_y_;
  int spawn_x_, spawn_y_;

  // Camera
  float camera_x_, camera_y_;
  float target_camera_x_, target_camera_y_;

  // Game objects
  std::array<Player, MAX_PLAYERS> players_;
  std::vector<Enemy> enemies_;
  std::vector<Powerup> powerups_;
  std::vector<Projectile> projectiles_;
  std::vector<MovingPlatform> moving_platforms_;
  std::vector<Particle> particles_;

  // AI controllers
  std::array<std::unique_ptr<PlatformAI>, MAX_PLAYERS> ai_controllers_;

  // Timing
  float update_timer_;
  float physics_accumulator_;
  static constexpr float PHYSICS_DT = 1.0f / 60.0f;

  // Rendering state
  bool needs_render_;
  bool initial_render_;
  bool needs_full_redraw_;

  // Colors
  lv_color_t color_bg_;
  lv_color_t color_tile_solid_;
  lv_color_t color_tile_platform_;
  lv_color_t color_tile_spike_;
  lv_color_t color_tile_ladder_;
  lv_color_t color_tile_goal_;
  lv_color_t color_tile_checkpoint_;
  lv_color_t color_tile_water_;
  lv_color_t color_tile_ice_;
  lv_color_t color_tile_bouncy_;
  lv_color_t color_enemy_;
  lv_color_t color_boss_;
  lv_color_t color_powerup_;
  lv_color_t color_powerdown_;
  lv_color_t color_projectile_;
  lv_color_t color_text_;

  // Player colors
  static constexpr uint32_t PLAYER_COLORS[MAX_PLAYERS] = {
    0xFF0000,  // Red - Player 1
    0x00FF00,  // Green - Player 2
    0x0000FF,  // Blue - Player 3
    0xFFFF00   // Yellow - Player 4
  };
};

}  // namespace game_platform
}  // namespace esphome
