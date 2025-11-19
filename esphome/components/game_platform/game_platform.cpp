#include "game_platform.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>

namespace esphome {
namespace game_platform {

// =============================================================================
// PlatformAI Implementation
// =============================================================================

PlatformAI::PlatformAI(uint8_t player_num, AIDifficulty difficulty)
    : player_num_(player_num), difficulty_(difficulty) {
  reset();
}

void PlatformAI::reset() {
  decision_timer_ = 0.0f;
  current_delay_ = get_reaction_time_();
  holding_jump_ = false;
  holding_left_ = false;
  holding_right_ = false;
  last_input_ = lvgl_game_runner::InputType::NONE;
}

float PlatformAI::get_reaction_time_() const {
  switch (difficulty_) {
    case AIDifficulty::BEGINNER: return 0.4f;
    case AIDifficulty::INTERMEDIATE: return 0.25f;
    case AIDifficulty::ADVANCED: return 0.15f;
    case AIDifficulty::EXPERT: return 0.08f;
    default: return 0.25f;
  }
}

float PlatformAI::get_accuracy_() const {
  switch (difficulty_) {
    case AIDifficulty::BEGINNER: return 0.6f;
    case AIDifficulty::INTERMEDIATE: return 0.75f;
    case AIDifficulty::ADVANCED: return 0.88f;
    case AIDifficulty::EXPERT: return 0.95f;
    default: return 0.75f;
  }
}

lvgl_game_runner::InputEvent PlatformAI::update(float dt, const lvgl_game_runner::GameState &state,
                                                const lvgl_game_runner::GameBase *game) {
  const GamePlatform *platform_game = static_cast<const GamePlatform *>(game);
  if (!platform_game) {
    return lvgl_game_runner::InputEvent(lvgl_game_runner::InputType::NONE, player_num_, false);
  }

  const Player &player = platform_game->get_player(player_num_);
  if (!player.active) {
    return lvgl_game_runner::InputEvent(lvgl_game_runner::InputType::NONE, player_num_, false);
  }

  decision_timer_ += dt;
  if (decision_timer_ < current_delay_) {
    return lvgl_game_runner::InputEvent(lvgl_game_runner::InputType::NONE, player_num_, false);
  }

  decision_timer_ = 0.0f;
  current_delay_ = get_reaction_time_();

  // Get game state
  const auto &enemies = platform_game->get_enemies();
  const auto &powerups = platform_game->get_powerups();
  int goal_x = platform_game->get_goal_x();

  // Make decision
  make_decision_(player, enemies, powerups, goal_x);

  // Determine which input to send based on AI state
  if (holding_jump_ && last_input_ != lvgl_game_runner::InputType::A) {
    last_input_ = lvgl_game_runner::InputType::A;
    return lvgl_game_runner::InputEvent(lvgl_game_runner::InputType::A, player_num_, true);
  } else if (holding_right_ && !holding_left_) {
    last_input_ = lvgl_game_runner::InputType::RIGHT;
    return lvgl_game_runner::InputEvent(lvgl_game_runner::InputType::RIGHT, player_num_, true);
  } else if (holding_left_ && !holding_right_) {
    last_input_ = lvgl_game_runner::InputType::LEFT;
    return lvgl_game_runner::InputEvent(lvgl_game_runner::InputType::LEFT, player_num_, true);
  }

  return lvgl_game_runner::InputEvent(lvgl_game_runner::InputType::NONE, player_num_, false);
}

void PlatformAI::make_decision_(const Player &player, const std::vector<Enemy> &enemies,
                                const std::vector<Powerup> &powerups, int goal_x) {
  holding_left_ = false;
  holding_right_ = false;
  holding_jump_ = false;

  // Random accuracy check
  float rand_val = (float)(rand() % 1000) / 1000.0f;
  if (rand_val > get_accuracy_()) {
    // Make a random mistake
    if (rand() % 3 == 0) {
      holding_jump_ = true;
    }
    return;
  }

  // Find nearest threat
  float nearest_enemy_dist = 10000.0f;
  float nearest_enemy_x = 0.0f;
  float nearest_enemy_y = 0.0f;

  for (const auto &enemy : enemies) {
    if (!enemy.active) continue;
    float dx = enemy.x - player.x;
    float dy = enemy.y - player.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < nearest_enemy_dist) {
      nearest_enemy_dist = dist;
      nearest_enemy_x = enemy.x;
      nearest_enemy_y = enemy.y;
    }
  }

  // Find nearest positive powerup
  float nearest_powerup_dist = 10000.0f;
  float nearest_powerup_x = 0.0f;

  for (const auto &powerup : powerups) {
    if (!powerup.active || powerup.is_negative()) continue;
    float dx = powerup.x - player.x;
    float dy = powerup.y - player.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < nearest_powerup_dist) {
      nearest_powerup_dist = dist;
      nearest_powerup_x = powerup.x;
    }
  }

  // Decision priority:
  // 1. Avoid nearby enemies (if very close)
  // 2. Collect nearby powerups
  // 3. Move towards goal

  if (nearest_enemy_dist < 50.0f) {
    // Enemy very close - try to avoid or jump over
    if (nearest_enemy_y > player.y - 10) {
      // Enemy at same level or below - jump
      holding_jump_ = player.on_ground;
    }
    if (nearest_enemy_x < player.x) {
      holding_right_ = true;
    } else {
      holding_left_ = true;
    }
  } else if (nearest_powerup_dist < 100.0f) {
    // Go for powerup
    if (nearest_powerup_x < player.x - 5) {
      holding_left_ = true;
    } else if (nearest_powerup_x > player.x + 5) {
      holding_right_ = true;
    }
  } else {
    // Move towards goal
    if (goal_x > player.x + 10) {
      holding_right_ = true;
    } else if (goal_x < player.x - 10) {
      holding_left_ = true;
    }
  }

  // Jump if there's a gap ahead or obstacle
  if ((holding_right_ || holding_left_) && player.on_ground) {
    // Simple heuristic: jump periodically when moving
    if (rand() % 20 == 0) {
      holding_jump_ = true;
    }
  }

  // Jump if not on ground and falling
  if (!player.on_ground && player.vy > 0 && player.jumps_remaining > 0) {
    // Consider double jump
    if (rand() % 5 == 0) {
      holding_jump_ = true;
    }
  }
}

// =============================================================================
// GamePlatform Constructor
// =============================================================================

GamePlatform::GamePlatform() {
  // Default configuration
  base_seed_ = 12345;
  rng_state_ = base_seed_;
  level_width_tiles_ = 64;
  level_height_tiles_ = 16;
  difficulty_ = Difficulty::NORMAL;
  ai_difficulty_ = AIDifficulty::INTERMEDIATE;
  initial_lives_ = 3;
  hidden_levels_enabled_ = true;
  boss_level_frequency_ = 5;
  player_speed_mult_ = 1.0f;
  jump_strength_mult_ = 1.0f;
  gravity_mult_ = 1.0f;
  double_jump_enabled_ = false;
  wall_jump_enabled_ = false;
  friendly_fire_ = false;
  shared_lives_ = false;
  max_enemies_per_level_ = 10;
  powerup_frequency_ = 1.0f;

  // Initialize colors
  color_bg_ = lv_color_hex(0x000020);
  color_tile_solid_ = lv_color_hex(0x8B4513);
  color_tile_platform_ = lv_color_hex(0x654321);
  color_tile_spike_ = lv_color_hex(0xFF0000);
  color_tile_ladder_ = lv_color_hex(0xD2691E);
  color_tile_goal_ = lv_color_hex(0xFFD700);
  color_tile_checkpoint_ = lv_color_hex(0x00FF00);
  color_tile_water_ = lv_color_hex(0x0066FF);
  color_tile_ice_ = lv_color_hex(0x87CEEB);
  color_tile_bouncy_ = lv_color_hex(0xFF69B4);
  color_enemy_ = lv_color_hex(0xFF4500);
  color_boss_ = lv_color_hex(0x8B0000);
  color_powerup_ = lv_color_hex(0x00FF00);
  color_powerdown_ = lv_color_hex(0x800080);
  color_projectile_ = lv_color_hex(0xFFFF00);
  color_text_ = lv_color_hex(0xFFFFFF);

  // Initialize state
  current_level_ = 1;
  game_over_ = false;
  victory_ = false;
  update_timer_ = 0.0f;
  physics_accumulator_ = 0.0f;
  needs_render_ = true;
  initial_render_ = true;
  needs_full_redraw_ = true;
}

// =============================================================================
// Lifecycle Methods
// =============================================================================

void GamePlatform::on_bind(lv_obj_t *canvas) {
  GameBase::on_bind(canvas);
  ESP_LOGI(TAG, "Platform game bound to canvas");
}

void GamePlatform::on_resize(const lvgl_game_runner::Rect &r) {
  GameBase::on_resize(r);

  // Calculate tile size based on screen dimensions
  // We want about 16 tiles visible vertically
  tile_size_ = std::max(4, area_.h / 16);

  // Calculate physics values based on tile size
  base_speed_ = tile_size_ * 4.0f * player_speed_mult_;  // 4 tiles per second
  base_jump_ = tile_size_ * 12.0f * jump_strength_mult_;  // Jump height
  gravity_ = tile_size_ * 30.0f * gravity_mult_;  // Gravity acceleration

  // Set player dimensions
  for (auto &player : players_) {
    player.width = tile_size_ - 2;
    player.height = tile_size_ * 3 / 2;
  }

  // Recalculate level dimensions
  level_pixel_width_ = level_width_tiles_ * tile_size_;
  level_pixel_height_ = level_height_tiles_ * tile_size_;

  // Initialize AI controllers for non-human players
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (i >= num_human_players_) {
      ai_controllers_[i] = std::make_unique<PlatformAI>(i, ai_difficulty_);
    }
  }

  // Initialize player colors
  for (int i = 0; i < MAX_PLAYERS; i++) {
    players_[i].color = lv_color_hex(PLAYER_COLORS[i]);
    players_[i].player_num = i;
  }

  needs_full_redraw_ = true;
  reset();

  ESP_LOGI(TAG, "Resized: %dx%d, tile_size=%d", area_.w, area_.h, tile_size_);
}

void GamePlatform::reset() {
  // Reset RNG with level-specific seed
  rng_state_ = base_seed_ + current_level_ * 7919;  // Prime multiplier for variety

  // Determine level type
  current_level_type_ = determine_level_type_();

  // Generate level
  generate_level_();

  // Initialize players
  int active_players = std::max(1, static_cast<int>(num_human_players_));
  if (num_human_players_ == 0) {
    active_players = 4;  // All AI demo mode
  }

  // Reset shared lives
  if (shared_lives_) {
    shared_lives_count_ = initial_lives_ * active_players;
  }

  for (int i = 0; i < MAX_PLAYERS; i++) {
    players_[i].reset();
    players_[i].active = (i < active_players);
    players_[i].x = spawn_x_ + (i % 2) * tile_size_;
    players_[i].y = spawn_y_ - (i / 2) * tile_size_ * 2;
    players_[i].last_x = players_[i].x;
    players_[i].last_y = players_[i].y;
    players_[i].checkpoint_x = spawn_x_ / tile_size_;
    players_[i].checkpoint_y = spawn_y_ / tile_size_;

    if (shared_lives_) {
      players_[i].lives = 0;  // Lives are shared
    } else {
      players_[i].lives = initial_lives_;
    }

    players_[i].jumps_remaining = double_jump_enabled_ ? 2 : 1;

    // Reset AI
    if (ai_controllers_[i]) {
      ai_controllers_[i]->reset();
    }
  }

  // Clear projectiles and particles
  projectiles_.clear();
  particles_.clear();

  // Reset camera
  camera_x_ = spawn_x_ - area_.w / 3;
  camera_y_ = spawn_y_ - area_.h / 2;
  target_camera_x_ = camera_x_;
  target_camera_y_ = camera_y_;

  // Reset timers
  update_timer_ = 0.0f;
  physics_accumulator_ = 0.0f;
  level_complete_ = false;
  level_complete_timer_ = 0.0f;
  game_over_ = false;
  victory_ = false;

  needs_render_ = true;
  initial_render_ = true;
  needs_full_redraw_ = true;

  ESP_LOGI(TAG, "Level %d (%s) generated: %dx%d tiles",
           current_level_,
           current_level_type_ == LevelType::BOSS ? "BOSS" :
           current_level_type_ == LevelType::HIDDEN ? "HIDDEN" : "NORMAL",
           level_width_tiles_, level_height_tiles_);
}

void GamePlatform::step(float dt) {
  // Handle pause state
  if (needs_render_ && paused_) {
    render_();
    needs_render_ = false;
    return;
  }

  if (paused_) return;

  // Handle level complete transition
  if (level_complete_) {
    level_complete_timer_ += dt;
    if (level_complete_timer_ >= 2.0f) {
      advance_to_next_level_();
    }
    needs_render_ = true;
  }

  // Handle game over
  if (game_over_) {
    needs_render_ = true;
    if (needs_render_) {
      render_();
      needs_render_ = false;
    }
    return;
  }

  // Update AI players
  update_ai_players_(dt);

  // Fixed timestep physics
  physics_accumulator_ += dt;
  while (physics_accumulator_ >= PHYSICS_DT) {
    physics_accumulator_ -= PHYSICS_DT;
    update_physics_(PHYSICS_DT);
    check_collisions_();
  }

  // Update enemies
  update_enemies_(dt);

  // Update projectiles
  update_projectiles_(dt);

  // Update moving platforms
  update_moving_platforms_(dt);

  // Update particles
  update_particles_(dt);

  // Update camera
  update_camera_();

  // Check level completion
  check_level_complete_();

  // Check game over
  check_game_over_();

  // Render
  needs_render_ = true;
  if (needs_render_) {
    render_();
    needs_render_ = false;
  }
}

void GamePlatform::pause() {
  GameBase::pause();
  needs_render_ = true;
}

void GamePlatform::resume() {
  GameBase::resume();
  needs_render_ = true;
}

void GamePlatform::on_input(const lvgl_game_runner::InputEvent &event) {
  using InputType = lvgl_game_runner::InputType;

  // Handle START for pause/restart
  if (event.type == InputType::START && event.pressed) {
    if (game_over_) {
      current_level_ = 1;
      total_score_ = 0;
      reset();
    } else {
      paused_ ? resume() : pause();
    }
    needs_render_ = true;
    return;
  }

  if (game_over_ || paused_) return;

  // Get player for this input
  uint8_t player_idx = event.player;
  if (player_idx >= MAX_PLAYERS || !players_[player_idx].active) return;

  Player &player = players_[player_idx];

  // Handle input based on type and press/release
  switch (event.type) {
    case InputType::LEFT:
      player.holding_left = event.pressed;
      break;

    case InputType::RIGHT:
      player.holding_right = event.pressed;
      break;

    case InputType::UP:
      player.holding_up = event.pressed;
      break;

    case InputType::DOWN:
      player.holding_down = event.pressed;
      break;

    case InputType::A:  // Jump
      if (event.pressed) {
        player_jump_(player);
      }
      break;

    case InputType::B:  // Action/Shoot
      if (event.pressed) {
        player_shoot_(player);
      }
      break;

    default:
      break;
  }

  // Calculate horizontal velocity based on held buttons
  bool left_pressed = player.controls_reversed ? player.holding_right : player.holding_left;
  bool right_pressed = player.controls_reversed ? player.holding_left : player.holding_right;

  if (left_pressed && !right_pressed) {
    player.vx = -base_speed_;
    player.facing_right = false;
  } else if (right_pressed && !left_pressed) {
    player.vx = base_speed_;
    player.facing_right = true;
  } else {
    player.vx = 0.0f;
  }

  // Handle ladder climbing based on held buttons
  if (player.on_ladder) {
    if (player.holding_up && !player.holding_down) {
      player.vy = -base_speed_ * 0.7f;
    } else if (player.holding_down && !player.holding_up) {
      player.vy = base_speed_ * 0.7f;
    } else {
      player.vy = 0.0f;
    }
  }
}

// =============================================================================
// Random Number Generation
// =============================================================================

uint32_t GamePlatform::xorshift32_() {
  rng_state_ ^= rng_state_ << 13;
  rng_state_ ^= rng_state_ >> 17;
  rng_state_ ^= rng_state_ << 5;
  return rng_state_;
}

float GamePlatform::rand01_() {
  return (xorshift32_() % 10000) / 10000.0f;
}

int GamePlatform::rand_range_(int min, int max) {
  if (min >= max) return min;
  return min + (xorshift32_() % (max - min + 1));
}

// =============================================================================
// Level Generation
// =============================================================================

LevelType GamePlatform::determine_level_type_() {
  // Boss level every N levels
  if (current_level_ % boss_level_frequency_ == 0) {
    return LevelType::BOSS;
  }

  // Hidden level chance (based on seed)
  if (hidden_levels_enabled_ && current_level_ > 2) {
    float hidden_chance = 0.1f + (current_level_ * 0.01f);
    if (rand01_() < hidden_chance) {
      return LevelType::HIDDEN;
    }
  }

  return LevelType::NORMAL;
}

void GamePlatform::generate_level_() {
  // Resize tile array
  level_tiles_.resize(level_width_tiles_ * level_height_tiles_);
  std::fill(level_tiles_.begin(), level_tiles_.end(), TileType::EMPTY);

  switch (current_level_type_) {
    case LevelType::BOSS:
      generate_boss_level_();
      break;
    case LevelType::HIDDEN:
      generate_hidden_level_();
      break;
    default:
      generate_normal_level_();
      break;
  }

  // Place enemies
  place_enemies_();

  // Place powerups
  place_powerups_();

  // Place moving platforms
  place_moving_platforms_();
}

void GamePlatform::generate_normal_level_() {
  float difficulty_factor = 0.5f + (static_cast<int>(difficulty_) * 0.15f);
  difficulty_factor += current_level_ * 0.02f;

  // Ground floor
  for (int x = 0; x < level_width_tiles_; x++) {
    set_tile_at_(x, level_height_tiles_ - 1, TileType::SOLID);
  }

  // Place platforms procedurally
  int num_platforms = 10 + current_level_ * 2;
  int platform_min_len = 3;
  int platform_max_len = 8;

  for (int i = 0; i < num_platforms; i++) {
    int px = rand_range_(2, level_width_tiles_ - 10);
    int py = rand_range_(3, level_height_tiles_ - 3);
    int len = rand_range_(platform_min_len, platform_max_len);

    // Determine platform type
    TileType type = TileType::SOLID;
    float r = rand01_();
    if (r < 0.1f * difficulty_factor) {
      type = TileType::ICE;
    } else if (r < 0.15f * difficulty_factor) {
      type = TileType::BOUNCY;
    } else if (r < 0.25f) {
      type = TileType::PLATFORM;  // One-way
    }

    for (int j = 0; j < len && px + j < level_width_tiles_; j++) {
      set_tile_at_(px + j, py, type);
    }
  }

  // Add some vertical structures
  int num_walls = 3 + current_level_;
  for (int i = 0; i < num_walls; i++) {
    int wx = rand_range_(5, level_width_tiles_ - 5);
    int wy_start = rand_range_(level_height_tiles_ / 2, level_height_tiles_ - 2);
    int height = rand_range_(3, 6);

    for (int j = 0; j < height && wy_start - j >= 0; j++) {
      set_tile_at_(wx, wy_start - j, TileType::SOLID);
    }
  }

  // Add ladders
  int num_ladders = 2 + rand_range_(0, 3);
  for (int i = 0; i < num_ladders; i++) {
    int lx = rand_range_(3, level_width_tiles_ - 3);
    int ly_start = level_height_tiles_ - 2;
    int height = rand_range_(4, 8);

    for (int j = 0; j < height; j++) {
      set_tile_at_(lx, ly_start - j, TileType::LADDER);
    }
  }

  // Add spikes (hazards)
  int num_spikes = static_cast<int>(5 * difficulty_factor);
  for (int i = 0; i < num_spikes; i++) {
    int sx = rand_range_(10, level_width_tiles_ - 5);
    int sy = level_height_tiles_ - 2;

    // Find ground level
    while (sy > 0 && get_tile_at_(sx * tile_size_, sy * tile_size_) == TileType::EMPTY) {
      sy--;
    }
    sy++;  // Place spike on top of ground

    if (sy < level_height_tiles_ - 1) {
      set_tile_at_(sx, sy, TileType::SPIKE);
    }
  }

  // Add water hazards
  if (rand01_() < 0.3f + current_level_ * 0.05f) {
    int water_start = rand_range_(level_width_tiles_ / 3, level_width_tiles_ * 2 / 3);
    int water_len = rand_range_(3, 6);
    for (int x = water_start; x < water_start + water_len && x < level_width_tiles_; x++) {
      set_tile_at_(x, level_height_tiles_ - 1, TileType::WATER);
    }
  }

  // Spawn point (start)
  spawn_x_ = tile_size_ * 2;
  spawn_y_ = (level_height_tiles_ - 3) * tile_size_;

  // Goal (end)
  goal_x_ = (level_width_tiles_ - 3) * tile_size_;
  goal_y_ = level_height_tiles_ - 2;

  // Find valid ground for goal
  while (goal_y_ > 0 && get_tile_at_(goal_x_, goal_y_ * tile_size_) == TileType::EMPTY) {
    goal_y_--;
  }
  set_tile_at_(level_width_tiles_ - 3, goal_y_, TileType::GOAL);

  // Add checkpoint
  int checkpoint_x = level_width_tiles_ / 2;
  int checkpoint_y = level_height_tiles_ - 2;
  while (checkpoint_y > 0 && get_tile_at_(checkpoint_x * tile_size_, checkpoint_y * tile_size_) == TileType::EMPTY) {
    checkpoint_y--;
  }
  set_tile_at_(checkpoint_x, checkpoint_y, TileType::CHECKPOINT);
}

void GamePlatform::generate_boss_level_() {
  // Boss arena - flatter, more open
  // Ground
  for (int x = 0; x < level_width_tiles_; x++) {
    set_tile_at_(x, level_height_tiles_ - 1, TileType::SOLID);
  }

  // Walls on sides
  for (int y = 0; y < level_height_tiles_; y++) {
    set_tile_at_(0, y, TileType::SOLID);
    set_tile_at_(level_width_tiles_ - 1, y, TileType::SOLID);
  }

  // Some platforms for dodging
  int num_platforms = 4;
  for (int i = 0; i < num_platforms; i++) {
    int px = 5 + i * (level_width_tiles_ - 10) / num_platforms;
    int py = level_height_tiles_ / 2 + (i % 2 == 0 ? -2 : 2);
    for (int j = 0; j < 4; j++) {
      set_tile_at_(px + j, py, TileType::PLATFORM);
    }
  }

  // Spawn point (left side)
  spawn_x_ = tile_size_ * 3;
  spawn_y_ = (level_height_tiles_ - 3) * tile_size_;

  // Goal (right side, appears after boss defeated)
  goal_x_ = (level_width_tiles_ - 4) * tile_size_;
  goal_y_ = level_height_tiles_ - 2;
  // Don't place goal tile yet - it appears when boss is defeated
}

void GamePlatform::generate_hidden_level_() {
  // Hidden level - more treasures, secret paths
  generate_normal_level_();  // Base layout

  // Add hidden blocks
  int num_hidden = 5 + current_level_;
  for (int i = 0; i < num_hidden; i++) {
    int hx = rand_range_(3, level_width_tiles_ - 3);
    int hy = rand_range_(3, level_height_tiles_ - 4);

    if (get_tile_at_(hx * tile_size_, hy * tile_size_) == TileType::EMPTY) {
      set_tile_at_(hx, hy, TileType::HIDDEN_BLOCK);
    }
  }

  // Add bonus powerups (more than normal)
  powerup_frequency_ *= 2.0f;
}

void GamePlatform::place_enemies_() {
  enemies_.clear();

  int num_enemies;
  if (current_level_type_ == LevelType::BOSS) {
    num_enemies = 1;  // Just the boss
  } else {
    float difficulty_mult = 1.0f + (static_cast<int>(difficulty_) * 0.3f);
    num_enemies = std::min(max_enemies_per_level_,
                          static_cast<int>((3 + current_level_) * difficulty_mult));
  }

  for (int i = 0; i < num_enemies; i++) {
    Enemy enemy;
    enemy.reset();

    if (current_level_type_ == LevelType::BOSS) {
      // Boss enemy
      enemy.type = static_cast<EnemyType>(
        EnemyType::BOSS_GIANT + (current_level_ / boss_level_frequency_) % 3
      );
      enemy.width = tile_size_ * 3;
      enemy.height = tile_size_ * 3;
      enemy.health = 10 + current_level_ * 2;
      enemy.max_health = enemy.health;
      enemy.x = level_pixel_width_ / 2;
      enemy.y = (level_height_tiles_ - 4) * tile_size_;
    } else {
      // Regular enemy
      float r = rand01_();
      if (r < 0.4f) {
        enemy.type = EnemyType::WALKER;
      } else if (r < 0.6f) {
        enemy.type = EnemyType::JUMPER;
      } else if (r < 0.75f) {
        enemy.type = EnemyType::FLYER;
      } else if (r < 0.85f) {
        enemy.type = EnemyType::SHOOTER;
      } else {
        enemy.type = EnemyType::CHASER;
      }

      enemy.width = tile_size_;
      enemy.height = tile_size_;
      enemy.health = 1;
      enemy.max_health = 1;

      // Random position
      enemy.x = rand_range_(level_width_tiles_ / 4, level_width_tiles_ * 3 / 4) * tile_size_;
      enemy.y = rand_range_(2, level_height_tiles_ - 3) * tile_size_;

      // Set patrol range
      enemy.patrol_start_x = enemy.x - tile_size_ * 3;
      enemy.patrol_end_x = enemy.x + tile_size_ * 3;
    }

    enemy.last_x = enemy.x;
    enemy.last_y = enemy.y;

    enemies_.push_back(enemy);
  }
}

void GamePlatform::place_powerups_() {
  powerups_.clear();

  int num_powerups = static_cast<int>((3 + current_level_ / 2) * powerup_frequency_);

  for (int i = 0; i < num_powerups; i++) {
    Powerup powerup;
    powerup.active = true;
    powerup.is_hidden = false;
    powerup.width = tile_size_;
    powerup.height = tile_size_;

    // Determine type
    float r = rand01_();
    if (r < 0.3f) {
      powerup.type = PowerupType::COIN;
      powerup.duration = 0.0f;
    } else if (r < 0.45f) {
      powerup.type = PowerupType::HEALTH;
      powerup.duration = 0.0f;
    } else if (r < 0.55f) {
      powerup.type = PowerupType::SPEED_BOOST;
      powerup.duration = 5.0f;
    } else if (r < 0.65f) {
      powerup.type = PowerupType::JUMP_BOOST;
      powerup.duration = 5.0f;
    } else if (r < 0.7f) {
      powerup.type = PowerupType::SHIELD;
      powerup.duration = 8.0f;
    } else if (r < 0.75f) {
      powerup.type = PowerupType::INVINCIBILITY;
      powerup.duration = 3.0f;
    } else if (r < 0.8f) {
      powerup.type = PowerupType::EXTRA_LIFE;
      powerup.duration = 0.0f;
    } else if (r < 0.85f) {
      // Powerdowns
      powerup.type = PowerupType::SLOW;
      powerup.duration = 4.0f;
    } else if (r < 0.9f) {
      powerup.type = PowerupType::REVERSE_CONTROLS;
      powerup.duration = 3.0f;
    } else {
      powerup.type = PowerupType::KEY;
      powerup.duration = 0.0f;
    }

    // Position
    powerup.x = rand_range_(5, level_width_tiles_ - 5) * tile_size_;
    powerup.y = rand_range_(3, level_height_tiles_ - 3) * tile_size_;
    powerup.last_x = powerup.x;
    powerup.last_y = powerup.y;

    powerups_.push_back(powerup);
  }
}

void GamePlatform::place_moving_platforms_() {
  moving_platforms_.clear();

  int num_moving = 1 + current_level_ / 3;
  if (current_level_type_ == LevelType::BOSS) {
    num_moving = 0;
  }

  for (int i = 0; i < num_moving; i++) {
    MovingPlatform platform;
    platform.active = true;
    platform.width = tile_size_ * 3;
    platform.height = tile_size_ / 2;

    // Starting position
    platform.start_x = rand_range_(5, level_width_tiles_ / 2) * tile_size_;
    platform.start_y = rand_range_(4, level_height_tiles_ - 4) * tile_size_;

    // End position (horizontal or vertical movement)
    if (rand01_() < 0.6f) {
      // Horizontal
      platform.end_x = platform.start_x + tile_size_ * rand_range_(4, 8);
      platform.end_y = platform.start_y;
    } else {
      // Vertical
      platform.end_x = platform.start_x;
      platform.end_y = platform.start_y + tile_size_ * rand_range_(3, 6);
    }

    platform.x = platform.start_x;
    platform.y = platform.start_y;
    platform.last_x = platform.x;
    platform.last_y = platform.y;
    platform.speed = tile_size_ * 1.5f;
    platform.moving_forward = true;

    moving_platforms_.push_back(platform);
  }
}

// =============================================================================
// Physics and Movement
// =============================================================================

void GamePlatform::update_physics_(float dt) {
  for (auto &player : players_) {
    if (!player.active) continue;
    update_player_physics_(player, dt);
  }
}

void GamePlatform::update_player_physics_(Player &player, float dt) {
  // Store last position for rendering optimization
  player.last_x = player.x;
  player.last_y = player.y;

  // Check if on ladder
  TileType current_tile = get_tile_at_(player.x + player.width / 2, player.y + player.height / 2);
  player.on_ladder = (current_tile == TileType::LADDER);

  // Check if in water
  player.in_water = (current_tile == TileType::WATER);

  // Apply gravity (reduced on ladder or in water)
  if (!player.on_ladder) {
    float gravity_factor = player.in_water ? 0.3f : 1.0f;
    player.vy += gravity_ * gravity_factor * dt;
  }

  // Apply water drag
  if (player.in_water) {
    player.vx *= 0.95f;
    player.vy *= 0.95f;
  }

  // Update timers
  if (player.invincibility_timer > 0) {
    player.invincibility_timer -= dt;
  }

  // Update power-up effect timers
  if (player.speed_boost_timer > 0) {
    player.speed_boost_timer -= dt;
    if (player.speed_boost_timer <= 0) {
      player.speed_modifier = 1.0f;
    }
  }

  if (player.jump_boost_timer > 0) {
    player.jump_boost_timer -= dt;
    if (player.jump_boost_timer <= 0) {
      player.jump_modifier = 1.0f;
    }
  }

  if (player.shield_timer > 0) {
    player.shield_timer -= dt;
    if (player.shield_timer <= 0) {
      player.has_shield = false;
    }
  }

  if (player.magnet_timer > 0) {
    player.magnet_timer -= dt;
    if (player.magnet_timer <= 0) {
      player.has_magnet = false;
    }
  }

  if (player.reverse_timer > 0) {
    player.reverse_timer -= dt;
    if (player.reverse_timer <= 0) {
      player.controls_reversed = false;
    }
  }

  // Apply velocity
  float new_x = player.x + player.vx * dt * player.speed_modifier;
  float new_y = player.y + player.vy * dt;

  // Horizontal collision
  if (!check_tile_collision_(new_x, player.y, player.width, player.height, false)) {
    player.x = new_x;
  } else {
    player.vx = 0;
    // Wall jump check
    if (wall_jump_enabled_ && !player.on_ground && player.vy > 0) {
      player.jumps_remaining = 1;
    }
  }

  // Vertical collision
  bool was_on_ground = player.on_ground;
  player.on_ground = false;

  if (player.vy >= 0) {
    // Moving down - check floor
    if (check_tile_collision_(player.x, new_y, player.width, player.height, true)) {
      // Find exact ground position
      while (!check_tile_collision_(player.x, player.y + 1, player.width, player.height, true)) {
        player.y += 1;
      }
      player.vy = 0;
      player.on_ground = true;
      player.jumps_remaining = double_jump_enabled_ ? 2 : 1;
    } else {
      player.y = new_y;
    }
  } else {
    // Moving up - check ceiling
    if (check_tile_collision_(player.x, new_y, player.width, player.height, false)) {
      player.vy = 0;
      // Check for hidden blocks
      int tile_x = (player.x + player.width / 2) / tile_size_;
      int tile_y = new_y / tile_size_;
      if (get_tile_at_(tile_x * tile_size_, tile_y * tile_size_) == TileType::HIDDEN_BLOCK) {
        set_tile_at_(tile_x, tile_y, TileType::SOLID);
        spawn_particles_(tile_x * tile_size_, tile_y * tile_size_, 10, color_powerup_);
        // Spawn a powerup!
        Powerup bonus;
        bonus.active = true;
        bonus.type = PowerupType::COIN;
        bonus.x = tile_x * tile_size_;
        bonus.y = (tile_y - 1) * tile_size_;
        bonus.width = tile_size_;
        bonus.height = tile_size_;
        bonus.duration = 0.0f;
        powerups_.push_back(bonus);
      }
    } else {
      player.y = new_y;
    }
  }

  // Bouncy tile
  if (player.on_ground) {
    TileType below = get_tile_at_(player.x + player.width / 2, player.y + player.height + 1);
    if (below == TileType::BOUNCY) {
      player.vy = -base_jump_ * 1.5f;
      player.on_ground = false;
    }
  }

  // Ice friction
  if (player.on_ground) {
    TileType below = get_tile_at_(player.x + player.width / 2, player.y + player.height + 1);
    if (below == TileType::ICE) {
      // Slide on ice
      if (player.vx == 0) {
        // Keep sliding
      }
    }
  }

  // Conveyor belts
  if (player.on_ground) {
    TileType below = get_tile_at_(player.x + player.width / 2, player.y + player.height + 1);
    if (below == TileType::CONVEYOR_LEFT) {
      player.x -= tile_size_ * dt;
    } else if (below == TileType::CONVEYOR_RIGHT) {
      player.x += tile_size_ * dt;
    }
  }

  // Spike damage
  TileType standing_on = get_tile_at_(player.x + player.width / 2, player.y + player.height + 1);
  if (standing_on == TileType::SPIKE) {
    damage_player_(player);
  }

  // Checkpoint
  if (standing_on == TileType::CHECKPOINT) {
    player.checkpoint_x = (player.x + player.width / 2) / tile_size_;
    player.checkpoint_y = (player.y + player.height) / tile_size_ - 1;
  }

  // Level bounds
  player.x = std::max(0.0f, std::min(player.x, (float)(level_pixel_width_ - player.width)));
  player.y = std::max(0.0f, std::min(player.y, (float)(level_pixel_height_ - player.height)));

  // Fall death
  if (player.y >= level_pixel_height_ - player.height) {
    damage_player_(player);
  }
}

bool GamePlatform::check_tile_collision_(float x, float y, int w, int h, bool check_platforms) {
  // Check all corners and edges
  int check_points[][2] = {
    {0, 0}, {w/2, 0}, {w-1, 0},           // Top
    {0, h/2}, {w-1, h/2},                  // Middle
    {0, h-1}, {w/2, h-1}, {w-1, h-1}       // Bottom
  };

  for (auto &point : check_points) {
    int px = x + point[0];
    int py = y + point[1];
    TileType tile = get_tile_at_(px, py);

    if (tile == TileType::SOLID || tile == TileType::ICE ||
        tile == TileType::CONVEYOR_LEFT || tile == TileType::CONVEYOR_RIGHT ||
        tile == TileType::BREAKABLE || tile == TileType::BOUNCY) {
      return true;
    }

    if (check_platforms && tile == TileType::PLATFORM) {
      // One-way platform - only collide from above
      int tile_top = (py / tile_size_) * tile_size_;
      if (y + h - 4 <= tile_top) {
        return true;
      }
    }
  }

  return false;
}

TileType GamePlatform::get_tile_at_(int pixel_x, int pixel_y) {
  int tile_x = pixel_x / tile_size_;
  int tile_y = pixel_y / tile_size_;

  if (tile_x < 0 || tile_x >= level_width_tiles_ ||
      tile_y < 0 || tile_y >= level_height_tiles_) {
    return TileType::SOLID;  // Out of bounds is solid
  }

  return level_tiles_[tile_y * level_width_tiles_ + tile_x];
}

void GamePlatform::set_tile_at_(int tile_x, int tile_y, TileType type) {
  if (tile_x < 0 || tile_x >= level_width_tiles_ ||
      tile_y < 0 || tile_y >= level_height_tiles_) {
    return;
  }
  level_tiles_[tile_y * level_width_tiles_ + tile_x] = type;
}

// =============================================================================
// Collision Detection
// =============================================================================

void GamePlatform::check_collisions_() {
  for (auto &player : players_) {
    if (!player.active) continue;

    check_player_enemy_collision_(player);
    check_player_powerup_collision_(player);
    check_player_projectile_collision_(player);
  }

  // Player-player collision for cooperation
  check_player_player_collision_();

  // Projectile-enemy collision
  check_projectile_enemy_collision_();
}

void GamePlatform::check_player_enemy_collision_(Player &player) {
  for (auto &enemy : enemies_) {
    if (!enemy.active) continue;

    if (rectangles_overlap_(player.x, player.y, player.width, player.height,
                           enemy.x, enemy.y, enemy.width, enemy.height)) {
      // Check if stomping (player falling onto enemy)
      if (player.vy > 0 && player.y + player.height - 5 < enemy.y + enemy.height / 2) {
        // Stomp!
        damage_enemy_(enemy, 1);
        player.vy = -base_jump_ * 0.6f;  // Bounce
        add_score_(100);
        spawn_particles_(enemy.x, enemy.y, 8, color_enemy_);
      } else {
        // Player takes damage
        if (player.invincibility_timer <= 0) {
          damage_player_(player);
        }
      }
    }
  }
}

void GamePlatform::check_player_powerup_collision_(Player &player) {
  for (auto &powerup : powerups_) {
    if (!powerup.active) continue;

    if (rectangles_overlap_(player.x, player.y, player.width, player.height,
                           powerup.x, powerup.y, powerup.width, powerup.height)) {
      apply_powerup_(player, powerup);
      powerup.active = false;
      spawn_particles_(powerup.x, powerup.y, 5,
                      powerup.is_negative() ? color_powerdown_ : color_powerup_);
    }
  }
}

void GamePlatform::check_player_projectile_collision_(Player &player) {
  for (auto &proj : projectiles_) {
    if (!proj.active) continue;
    if (proj.type == ProjectileType::PLAYER_SHOT) continue;  // Own shots

    if (rectangles_overlap_(player.x, player.y, player.width, player.height,
                           proj.x, proj.y, tile_size_ / 2, tile_size_ / 2)) {
      if (player.invincibility_timer <= 0 && !player.has_shield) {
        damage_player_(player);
      }
      proj.active = false;
    }
  }
}

void GamePlatform::check_player_player_collision_() {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (!players_[i].active) continue;

    for (int j = i + 1; j < MAX_PLAYERS; j++) {
      if (!players_[j].active) continue;

      Player &p1 = players_[i];
      Player &p2 = players_[j];

      if (rectangles_overlap_(p1.x, p1.y, p1.width, p1.height,
                             p2.x, p2.y, p2.width, p2.height)) {
        // Push players apart
        float overlap_x = (p1.x + p1.width / 2) - (p2.x + p2.width / 2);
        float overlap_y = (p1.y + p1.height / 2) - (p2.y + p2.height / 2);

        // Determine push direction based on greater overlap axis
        if (std::abs(overlap_x) > std::abs(overlap_y)) {
          // Horizontal push
          float push = (p1.width + p2.width) / 2 - std::abs(overlap_x);
          if (overlap_x > 0) {
            p1.x += push / 2;
            p2.x -= push / 2;
          } else {
            p1.x -= push / 2;
            p2.x += push / 2;
          }
        } else {
          // Vertical push - allows player stacking for cooperation
          float push = (p1.height + p2.height) / 2 - std::abs(overlap_y);
          if (overlap_y > 0) {
            p1.y += push / 2;
            p2.y -= push / 2;
            // If player 2 is on top of player 1, treat as ground
            if (p2.y < p1.y) {
              p2.on_ground = true;
              p2.vy = std::min(0.0f, p2.vy);
            }
          } else {
            p1.y -= push / 2;
            p2.y += push / 2;
            if (p1.y < p2.y) {
              p1.on_ground = true;
              p1.vy = std::min(0.0f, p1.vy);
            }
          }
        }
      }
    }
  }
}

void GamePlatform::check_projectile_enemy_collision_() {
  for (auto &proj : projectiles_) {
    if (!proj.active) continue;
    if (proj.type != ProjectileType::PLAYER_SHOT) continue;

    for (auto &enemy : enemies_) {
      if (!enemy.active) continue;

      if (rectangles_overlap_(proj.x, proj.y, tile_size_ / 2, tile_size_ / 2,
                             enemy.x, enemy.y, enemy.width, enemy.height)) {
        damage_enemy_(enemy, 1);
        proj.active = false;
        spawn_particles_(proj.x, proj.y, 5, color_projectile_);
        add_score_(50);
        break;
      }
    }
  }
}

bool GamePlatform::rectangles_overlap_(float x1, float y1, int w1, int h1,
                                       float x2, float y2, int w2, int h2) {
  return !(x1 + w1 <= x2 || x2 + w2 <= x1 ||
           y1 + h1 <= y2 || y2 + h2 <= y1);
}

// =============================================================================
// Player Actions
// =============================================================================

void GamePlatform::player_jump_(Player &player) {
  if (player.on_ladder) {
    // Jump off ladder
    player.on_ladder = false;
    player.vy = -base_jump_ * 0.8f * player.jump_modifier;
    return;
  }

  if (player.jumps_remaining > 0) {
    player.vy = -base_jump_ * player.jump_modifier;
    player.jumps_remaining--;
    player.on_ground = false;
  }
}

void GamePlatform::player_shoot_(Player &player) {
  if (projectiles_.size() >= MAX_PROJECTILES) return;

  Projectile proj;
  proj.active = true;
  proj.type = ProjectileType::PLAYER_SHOT;
  proj.x = player.x + (player.facing_right ? player.width : 0);
  proj.y = player.y + player.height / 3;
  proj.vx = player.facing_right ? tile_size_ * 8 : -tile_size_ * 8;
  proj.vy = 0;
  proj.owner_player = player.player_num;

  projectiles_.push_back(proj);
}

void GamePlatform::apply_powerup_(Player &player, const Powerup &powerup) {
  switch (powerup.type) {
    case PowerupType::COIN:
      add_score_(10);
      break;

    case PowerupType::HEALTH:
      // Restore health (if damaged)
      if (player.invincibility_timer < 0) {
        player.invincibility_timer = 0;
      }
      add_score_(50);
      break;

    case PowerupType::EXTRA_LIFE:
      if (shared_lives_) {
        shared_lives_count_++;
      } else {
        player.lives = std::min((int)player.lives + 1, 9);
      }
      add_score_(500);
      break;

    case PowerupType::SPEED_BOOST:
      player.speed_modifier = 1.5f;
      player.speed_boost_timer = powerup.duration;
      add_score_(25);
      break;

    case PowerupType::JUMP_BOOST:
      player.jump_modifier = 1.3f;
      player.jump_boost_timer = powerup.duration;
      add_score_(25);
      break;

    case PowerupType::INVINCIBILITY:
      player.invincibility_timer = powerup.duration;
      add_score_(100);
      break;

    case PowerupType::DOUBLE_JUMP:
      player.jumps_remaining = 2;
      add_score_(75);
      break;

    case PowerupType::KEY:
      player.keys_collected++;
      add_score_(200);
      break;

    case PowerupType::SHIELD:
      player.has_shield = true;
      player.shield_timer = powerup.duration;
      add_score_(100);
      break;

    case PowerupType::MAGNET:
      player.has_magnet = true;
      player.magnet_timer = powerup.duration;
      add_score_(50);
      break;

    // Powerdowns
    case PowerupType::SLOW:
      player.speed_modifier = 0.5f;
      player.speed_boost_timer = powerup.duration;
      break;

    case PowerupType::REVERSE_CONTROLS:
      player.controls_reversed = true;
      player.reverse_timer = powerup.duration;
      break;

    case PowerupType::LOW_JUMP:
      player.jump_modifier = 0.6f;
      player.jump_boost_timer = powerup.duration;
      break;

    default:
      break;
  }
}

void GamePlatform::damage_player_(Player &player) {
  if (player.invincibility_timer > 0) return;

  if (player.has_shield) {
    player.has_shield = false;
    player.invincibility_timer = 1.0f;
    spawn_particles_(player.x, player.y, 10, lv_color_hex(0x00FFFF));
    return;
  }

  if (shared_lives_) {
    shared_lives_count_--;
    if (shared_lives_count_ <= 0) {
      player.active = false;
    }
  } else {
    player.lives--;
    if (player.lives <= 0) {
      player.active = false;
    }
  }

  spawn_particles_(player.x, player.y, 15, player.color);

  if (player.active) {
    respawn_player_(player);
  }
}

void GamePlatform::respawn_player_(Player &player) {
  player.x = player.checkpoint_x * tile_size_;
  player.y = player.checkpoint_y * tile_size_;
  player.vx = 0;
  player.vy = 0;
  player.invincibility_timer = 2.0f;
  player.speed_modifier = 1.0f;
  player.jump_modifier = 1.0f;
  player.controls_reversed = false;
  player.jumps_remaining = double_jump_enabled_ ? 2 : 1;
}

// =============================================================================
// Enemy AI
// =============================================================================

void GamePlatform::update_enemies_(float dt) {
  for (auto &enemy : enemies_) {
    if (!enemy.active) continue;

    enemy.last_x = enemy.x;
    enemy.last_y = enemy.y;

    update_enemy_ai_(enemy, dt);

    // Timers
    enemy.state_timer += dt;
    enemy.shoot_timer += dt;
  }
}

void GamePlatform::update_enemy_ai_(Enemy &enemy, float dt) {
  switch (enemy.type) {
    case EnemyType::WALKER: {
      // Walk back and forth
      float speed = tile_size_ * 2.0f;
      if (enemy.moving_right) {
        enemy.x += speed * dt;
        if (enemy.x >= enemy.patrol_end_x) {
          enemy.moving_right = false;
        }
      } else {
        enemy.x -= speed * dt;
        if (enemy.x <= enemy.patrol_start_x) {
          enemy.moving_right = true;
        }
      }
      break;
    }

    case EnemyType::FLYER: {
      // Sine wave pattern
      float speed = tile_size_ * 1.5f;
      enemy.x += speed * dt * (enemy.moving_right ? 1 : -1);
      enemy.y = enemy.patrol_start_y + std::sin(enemy.state_timer * 3) * tile_size_ * 2;

      if (enemy.x >= enemy.patrol_end_x) enemy.moving_right = false;
      if (enemy.x <= enemy.patrol_start_x) enemy.moving_right = true;
      break;
    }

    case EnemyType::JUMPER: {
      // Hop periodically
      if (enemy.state_timer >= 1.0f) {
        enemy.vy = -tile_size_ * 6;
        enemy.state_timer = 0;
      }
      enemy.vy += gravity_ * dt;
      enemy.y += enemy.vy * dt;

      // Ground check
      if (enemy.y >= enemy.patrol_start_y) {
        enemy.y = enemy.patrol_start_y;
        enemy.vy = 0;
      }
      break;
    }

    case EnemyType::SHOOTER: {
      // Stand and shoot
      if (enemy.shoot_timer >= 2.0f) {
        enemy_shoot_(enemy);
        enemy.shoot_timer = 0;
      }
      break;
    }

    case EnemyType::CHASER: {
      // Chase nearest player
      float nearest_dist = 10000.0f;
      float target_x = enemy.x;

      for (const auto &player : players_) {
        if (!player.active) continue;
        float dx = player.x - enemy.x;
        float dy = player.y - enemy.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < nearest_dist) {
          nearest_dist = dist;
          target_x = player.x;
        }
      }

      if (nearest_dist < tile_size_ * 10) {
        float speed = tile_size_ * 3.0f;
        if (target_x > enemy.x + 5) {
          enemy.x += speed * dt;
        } else if (target_x < enemy.x - 5) {
          enemy.x -= speed * dt;
        }
      }
      break;
    }

    case EnemyType::BOSS_GIANT:
    case EnemyType::BOSS_FLYING:
    case EnemyType::BOSS_SPAWNER: {
      // Boss patterns
      float speed = tile_size_ * 1.5f;

      // Phase-based behavior
      if (enemy.health <= enemy.max_health / 2 && enemy.phase == 0) {
        enemy.phase = 1;  // Enraged phase
      }

      // Movement
      if (enemy.moving_right) {
        enemy.x += speed * dt;
        if (enemy.x >= level_pixel_width_ - enemy.width - tile_size_ * 2) {
          enemy.moving_right = false;
        }
      } else {
        enemy.x -= speed * dt;
        if (enemy.x <= tile_size_ * 2) {
          enemy.moving_right = true;
        }
      }

      // Flying boss vertical movement
      if (enemy.type == EnemyType::BOSS_FLYING) {
        enemy.y = (level_height_tiles_ - 6) * tile_size_ +
                  std::sin(enemy.state_timer * 2) * tile_size_ * 2;
      }

      // Shooting
      float shoot_interval = enemy.phase == 0 ? 1.5f : 0.8f;
      if (enemy.shoot_timer >= shoot_interval) {
        enemy_shoot_(enemy);
        enemy.shoot_timer = 0;
      }

      // Spawner spawns minions
      if (enemy.type == EnemyType::BOSS_SPAWNER && enemy.state_timer >= 5.0f) {
        spawn_boss_minions_(enemy);
        enemy.state_timer = 0;
      }
      break;
    }

    default:
      break;
  }
}

void GamePlatform::enemy_shoot_(Enemy &enemy) {
  if (projectiles_.size() >= MAX_PROJECTILES) return;

  // Find nearest player to target
  float nearest_dist = 10000.0f;
  float target_x = enemy.x;
  float target_y = enemy.y;

  for (const auto &player : players_) {
    if (!player.active) continue;
    float dx = player.x - enemy.x;
    float dy = player.y - enemy.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < nearest_dist) {
      nearest_dist = dist;
      target_x = player.x;
      target_y = player.y;
    }
  }

  Projectile proj;
  proj.active = true;
  proj.type = (enemy.type >= EnemyType::BOSS_GIANT) ?
              ProjectileType::BOSS_SHOT : ProjectileType::ENEMY_SHOT;
  proj.x = enemy.x + enemy.width / 2;
  proj.y = enemy.y + enemy.height / 2;

  // Calculate direction
  float dx = target_x - proj.x;
  float dy = target_y - proj.y;
  float dist = std::sqrt(dx * dx + dy * dy);
  if (dist > 0) {
    float speed = tile_size_ * 5;
    proj.vx = (dx / dist) * speed;
    proj.vy = (dy / dist) * speed;
  }

  proj.owner_player = 255;
  projectiles_.push_back(proj);
}

void GamePlatform::damage_enemy_(Enemy &enemy, int damage) {
  enemy.health -= damage;
  if (enemy.health <= 0) {
    enemy.active = false;

    // Boss defeated
    if (enemy.type >= EnemyType::BOSS_GIANT) {
      // Reveal goal
      set_tile_at_(level_width_tiles_ - 4, goal_y_, TileType::GOAL);
      add_score_(1000);
      spawn_particles_(enemy.x, enemy.y, 30, color_boss_);
    }
  }
}

void GamePlatform::spawn_boss_minions_(Enemy &boss) {
  if (enemies_.size() >= MAX_ENEMIES) return;

  Enemy minion;
  minion.reset();
  minion.type = EnemyType::WALKER;
  minion.x = boss.x + (rand01_() < 0.5f ? -tile_size_ * 2 : boss.width + tile_size_);
  minion.y = boss.y;
  minion.width = tile_size_;
  minion.height = tile_size_;
  minion.health = 1;
  minion.max_health = 1;
  minion.patrol_start_x = minion.x - tile_size_ * 3;
  minion.patrol_end_x = minion.x + tile_size_ * 3;

  enemies_.push_back(minion);
}

// =============================================================================
// AI Players
// =============================================================================

void GamePlatform::update_ai_players_(float dt) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (i < num_human_players_) continue;  // Skip human players
    if (!players_[i].active) continue;
    if (!ai_controllers_[i]) continue;

    lvgl_game_runner::GameState ai_state;
    ai_state.score = players_[i].score;
    ai_state.lives = players_[i].lives;
    ai_state.level = current_level_;
    ai_state.game_over = game_over_;

    auto event = ai_controllers_[i]->update(dt, ai_state, this);
    if (event.type != lvgl_game_runner::InputType::NONE) {
      on_input(event);
    }
  }
}

// =============================================================================
// Projectiles and Moving Platforms
// =============================================================================

void GamePlatform::update_projectiles_(float dt) {
  for (auto &proj : projectiles_) {
    if (!proj.active) continue;

    proj.x += proj.vx * dt;
    proj.y += proj.vy * dt;

    // Check bounds
    if (proj.x < 0 || proj.x >= level_pixel_width_ ||
        proj.y < 0 || proj.y >= level_pixel_height_) {
      proj.active = false;
      continue;
    }

    // Check tile collision
    if (check_tile_collision_(proj.x, proj.y, tile_size_ / 4, tile_size_ / 4, false)) {
      proj.active = false;
    }
  }

  // Remove inactive projectiles
  projectiles_.erase(
    std::remove_if(projectiles_.begin(), projectiles_.end(),
                   [](const Projectile &p) { return !p.active; }),
    projectiles_.end()
  );
}

void GamePlatform::update_moving_platforms_(float dt) {
  for (auto &platform : moving_platforms_) {
    if (!platform.active) continue;

    platform.last_x = platform.x;
    platform.last_y = platform.y;

    // Move towards end or start
    float target_x = platform.moving_forward ? platform.end_x : platform.start_x;
    float target_y = platform.moving_forward ? platform.end_y : platform.start_y;

    float dx = target_x - platform.x;
    float dy = target_y - platform.y;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < 1.0f) {
      platform.moving_forward = !platform.moving_forward;
    } else {
      platform.x += (dx / dist) * platform.speed * dt;
      platform.y += (dy / dist) * platform.speed * dt;
    }

    // Move players standing on platform
    float delta_x = platform.x - platform.last_x;
    float delta_y = platform.y - platform.last_y;

    for (auto &player : players_) {
      if (!player.active) continue;

      // Check if player is on this platform
      if (player.on_ground &&
          player.x + player.width > platform.last_x &&
          player.x < platform.last_x + platform.width &&
          std::abs(player.y + player.height - platform.last_y) < 3) {
        player.x += delta_x;
        player.y += delta_y;
      }
    }
  }
}

void GamePlatform::update_particles_(float dt) {
  for (auto &particle : particles_) {
    if (!particle.active) continue;

    particle.x += particle.vx * dt;
    particle.y += particle.vy * dt;
    particle.vy += gravity_ * 0.5f * dt;
    particle.life -= dt;

    if (particle.life <= 0) {
      particle.active = false;
    }
  }

  // Remove inactive particles
  particles_.erase(
    std::remove_if(particles_.begin(), particles_.end(),
                   [](const Particle &p) { return !p.active; }),
    particles_.end()
  );
}

// =============================================================================
// Game State Management
// =============================================================================

void GamePlatform::check_level_complete_() {
  if (level_complete_) return;

  // Check if any player reached the goal
  for (const auto &player : players_) {
    if (!player.active) continue;

    int player_tile_x = (player.x + player.width / 2) / tile_size_;
    int player_tile_y = (player.y + player.height) / tile_size_;

    if (get_tile_at_(player_tile_x * tile_size_, player_tile_y * tile_size_) == TileType::GOAL ||
        get_tile_at_(player_tile_x * tile_size_, (player_tile_y - 1) * tile_size_) == TileType::GOAL) {
      level_complete_ = true;
      add_score_(500 + current_level_ * 100);
      return;
    }
  }
}

void GamePlatform::advance_to_next_level_() {
  current_level_++;
  reset();
}

void GamePlatform::check_game_over_() {
  if (game_over_) return;

  if (shared_lives_) {
    if (shared_lives_count_ <= 0) {
      game_over_ = true;
    }
  } else {
    bool any_alive = false;
    for (const auto &player : players_) {
      if (player.active && player.lives > 0) {
        any_alive = true;
        break;
      }
    }
    if (!any_alive) {
      game_over_ = true;
    }
  }
}

void GamePlatform::add_score_(uint32_t points) {
  total_score_ += points;

  // Also add to individual player scores
  for (auto &player : players_) {
    if (player.active) {
      player.score += points / std::max(1, (int)num_human_players_);
    }
  }
}

// =============================================================================
// Camera
// =============================================================================

void GamePlatform::update_camera_() {
  // Find active players' centroid (or follow player 1)
  float target_x = 0;
  float target_y = 0;
  int count = 0;

  for (const auto &player : players_) {
    if (!player.active) continue;
    target_x += player.x;
    target_y += player.y;
    count++;
  }

  if (count == 0) return;

  target_x /= count;
  target_y /= count;

  // Camera leads slightly ahead
  target_camera_x_ = target_x - area_.w / 3;
  target_camera_y_ = target_y - area_.h / 2;

  // Smooth camera follow
  float lerp = 0.1f;
  camera_x_ += (target_camera_x_ - camera_x_) * lerp;
  camera_y_ += (target_camera_y_ - camera_y_) * lerp;

  // Clamp to level bounds
  camera_x_ = std::max(0.0f, std::min(camera_x_, (float)(level_pixel_width_ - area_.w)));
  camera_y_ = std::max(0.0f, std::min(camera_y_, (float)(level_pixel_height_ - area_.h)));
}

int GamePlatform::world_to_screen_x_(float world_x) {
  return static_cast<int>(world_x - camera_x_);
}

int GamePlatform::world_to_screen_y_(float world_y) {
  return static_cast<int>(world_y - camera_y_);
}

bool GamePlatform::is_on_screen_(float x, float y, int w, int h) {
  return !(x + w < camera_x_ || x > camera_x_ + area_.w ||
           y + h < camera_y_ || y > camera_y_ + area_.h);
}

// =============================================================================
// Rendering
// =============================================================================

void GamePlatform::render_() {
  if (initial_render_ || needs_full_redraw_) {
    // Full redraw
    fill_rect_fast(0, 0, area_.w, area_.h, color_bg_);
    render_tiles_();
    initial_render_ = false;
    needs_full_redraw_ = false;
  }

  render_background_();
  render_tiles_();
  render_moving_platforms_();
  render_powerups_();
  render_enemies_();
  render_projectiles_();
  render_players_();
  render_particles_();
  render_hud_();

  if (paused_) {
    render_pause_screen_();
  }

  if (game_over_) {
    render_game_over_();
  }

  if (level_complete_) {
    // Flash "Level Complete"
    int text_x = area_.w / 2 - 40;
    int text_y = area_.h / 2;
    draw_text(text_x, text_y, "LEVEL COMPLETE!", &lv_font_montserrat_10, color_tile_goal_, LV_TEXT_ALIGN_CENTER);
  }

  // Invalidate entire canvas for LVGL
  invalidate_area_rect(0, 0, area_.w, area_.h);
}

void GamePlatform::render_background_() {
  // Simple parallax background could go here
  // For now, just solid color (already drawn in full redraw)
}

void GamePlatform::render_tiles_() {
  // Only render visible tiles
  int start_tile_x = std::max(0, (int)(camera_x_ / tile_size_));
  int end_tile_x = std::min(level_width_tiles_, (int)((camera_x_ + area_.w) / tile_size_) + 1);
  int start_tile_y = std::max(0, (int)(camera_y_ / tile_size_));
  int end_tile_y = std::min(level_height_tiles_, (int)((camera_y_ + area_.h) / tile_size_) + 1);

  for (int ty = start_tile_y; ty < end_tile_y; ty++) {
    for (int tx = start_tile_x; tx < end_tile_x; tx++) {
      draw_tile_(tx, ty);
    }
  }
}

void GamePlatform::draw_tile_(int tile_x, int tile_y) {
  TileType type = level_tiles_[tile_y * level_width_tiles_ + tile_x];
  if (type == TileType::EMPTY || type == TileType::HIDDEN_BLOCK) return;

  int screen_x = world_to_screen_x_(tile_x * tile_size_);
  int screen_y = world_to_screen_y_(tile_y * tile_size_);

  if (screen_x + tile_size_ < 0 || screen_x >= area_.w ||
      screen_y + tile_size_ < 0 || screen_y >= area_.h) {
    return;
  }

  lv_color_t color;
  switch (type) {
    case TileType::SOLID:
      color = color_tile_solid_;
      break;
    case TileType::PLATFORM:
      color = color_tile_platform_;
      break;
    case TileType::SPIKE:
      color = color_tile_spike_;
      break;
    case TileType::LADDER:
      color = color_tile_ladder_;
      break;
    case TileType::GOAL:
      color = color_tile_goal_;
      break;
    case TileType::CHECKPOINT:
      color = color_tile_checkpoint_;
      break;
    case TileType::WATER:
      color = color_tile_water_;
      break;
    case TileType::ICE:
      color = color_tile_ice_;
      break;
    case TileType::BOUNCY:
      color = color_tile_bouncy_;
      break;
    default:
      color = color_tile_solid_;
      break;
  }

  fill_rect_fast(screen_x, screen_y, tile_size_, tile_size_, color);
}

void GamePlatform::render_moving_platforms_() {
  for (const auto &platform : moving_platforms_) {
    if (!platform.active) continue;
    if (!is_on_screen_(platform.x, platform.y, platform.width, platform.height)) continue;

    int screen_x = world_to_screen_x_(platform.x);
    int screen_y = world_to_screen_y_(platform.y);

    fill_rect_fast(screen_x, screen_y, platform.width, platform.height, color_tile_platform_);
  }
}

void GamePlatform::render_enemies_() {
  for (const auto &enemy : enemies_) {
    if (!enemy.active) continue;
    if (!is_on_screen_(enemy.x, enemy.y, enemy.width, enemy.height)) continue;

    draw_enemy_(enemy);
  }
}

void GamePlatform::draw_enemy_(const Enemy &enemy) {
  int screen_x = world_to_screen_x_(enemy.x);
  int screen_y = world_to_screen_y_(enemy.y);

  lv_color_t color = (enemy.type >= EnemyType::BOSS_GIANT) ? color_boss_ : color_enemy_;

  fill_rect_fast(screen_x, screen_y, enemy.width, enemy.height, color);

  // Health bar for bosses
  if (enemy.type >= EnemyType::BOSS_GIANT) {
    int bar_width = enemy.width;
    int bar_height = 3;
    int health_width = (bar_width * enemy.health) / enemy.max_health;

    fill_rect_fast(screen_x, screen_y - 5, bar_width, bar_height, lv_color_hex(0x800000));
    fill_rect_fast(screen_x, screen_y - 5, health_width, bar_height, lv_color_hex(0x00FF00));
  }
}

void GamePlatform::render_powerups_() {
  for (const auto &powerup : powerups_) {
    if (!powerup.active) continue;
    if (!is_on_screen_(powerup.x, powerup.y, powerup.width, powerup.height)) continue;

    draw_powerup_(powerup);
  }
}

void GamePlatform::draw_powerup_(const Powerup &powerup) {
  int screen_x = world_to_screen_x_(powerup.x);
  int screen_y = world_to_screen_y_(powerup.y);

  lv_color_t color = powerup.is_negative() ? color_powerdown_ : color_powerup_;

  // Different shapes for different types
  int size = powerup.width - 2;
  fill_rect_fast(screen_x + 1, screen_y + 1, size, size, color);
}

void GamePlatform::render_players_() {
  for (const auto &player : players_) {
    if (!player.active) continue;

    draw_player_(player);
  }
}

void GamePlatform::draw_player_(const Player &player) {
  int screen_x = world_to_screen_x_(player.x);
  int screen_y = world_to_screen_y_(player.y);

  // Blink when invincible
  if (player.invincibility_timer > 0) {
    int blink = (int)(player.invincibility_timer * 10) % 2;
    if (blink) return;
  }

  fill_rect_fast(screen_x, screen_y, player.width, player.height, player.color);

  // Shield indicator
  if (player.has_shield) {
    draw_rect(screen_x - 1, screen_y - 1, player.width + 2, player.height + 2, lv_color_hex(0x00FFFF));
  }
}

void GamePlatform::render_projectiles_() {
  for (const auto &proj : projectiles_) {
    if (!proj.active) continue;

    int screen_x = world_to_screen_x_(proj.x);
    int screen_y = world_to_screen_y_(proj.y);

    if (screen_x < 0 || screen_x >= area_.w || screen_y < 0 || screen_y >= area_.h) continue;

    lv_color_t color = (proj.type == ProjectileType::PLAYER_SHOT) ?
                       color_projectile_ : color_enemy_;
    int size = tile_size_ / 4;
    fill_rect_fast(screen_x, screen_y, size, size, color);
  }
}

void GamePlatform::render_particles_() {
  for (const auto &particle : particles_) {
    if (!particle.active) continue;

    int screen_x = world_to_screen_x_(particle.x);
    int screen_y = world_to_screen_y_(particle.y);

    if (screen_x < 0 || screen_x >= area_.w || screen_y < 0 || screen_y >= area_.h) continue;

    fill_rect_fast(screen_x, screen_y, 2, 2, particle.color);
  }
}

void GamePlatform::render_hud_() {
  // Score
  char score_text[32];
  snprintf(score_text, sizeof(score_text), "SCORE:%d", total_score_);
  draw_text(2, 2, score_text, &lv_font_montserrat_10, color_text_, LV_TEXT_ALIGN_LEFT);

  // Level
  char level_text[32];
  const char *type_str = "";
  if (current_level_type_ == LevelType::BOSS) type_str = "BOSS ";
  else if (current_level_type_ == LevelType::HIDDEN) type_str = "?";
  snprintf(level_text, sizeof(level_text), "%sLV:%d", type_str, current_level_);
  draw_text(area_.w - 50, 2, level_text, &lv_font_montserrat_10, color_text_, LV_TEXT_ALIGN_LEFT);

  // Lives
  if (shared_lives_) {
    char lives_text[16];
    snprintf(lives_text, sizeof(lives_text), "LIVES:%d", shared_lives_count_);
    draw_text(2, 12, lives_text, &lv_font_montserrat_10, color_text_, LV_TEXT_ALIGN_LEFT);
  } else {
    // Show each player's lives
    int y_offset = 12;
    for (int i = 0; i < MAX_PLAYERS; i++) {
      if (!players_[i].active && players_[i].lives == 0) continue;
      char lives_text[16];
      snprintf(lives_text, sizeof(lives_text), "P%d:%d", i + 1, players_[i].lives);
      draw_text(2, y_offset, lives_text, &lv_font_montserrat_10, players_[i].color, LV_TEXT_ALIGN_LEFT);
      y_offset += 10;
    }
  }
}

void GamePlatform::render_pause_screen_() {
  // Semi-transparent overlay
  fill_rect(0, 0, area_.w, area_.h, lv_color_hex(0x000000));

  // Pause text
  draw_text(area_.w / 2 - 20, area_.h / 2 - 5, "PAUSED",
            &lv_font_montserrat_10, color_text_, LV_TEXT_ALIGN_CENTER);
  draw_text(area_.w / 2 - 40, area_.h / 2 + 8, "Press START",
            &lv_font_montserrat_10, color_text_, LV_TEXT_ALIGN_CENTER);
}

void GamePlatform::render_game_over_() {
  // Overlay
  fill_rect(0, 0, area_.w, area_.h, lv_color_hex(0x200000));

  // Game over text
  draw_text(area_.w / 2 - 30, area_.h / 2 - 10, "GAME OVER",
            &lv_font_montserrat_10, color_tile_spike_, LV_TEXT_ALIGN_CENTER);

  char score_text[32];
  snprintf(score_text, sizeof(score_text), "Final Score: %d", total_score_);
  draw_text(area_.w / 2 - 40, area_.h / 2 + 5, score_text,
            &lv_font_montserrat_10, color_text_, LV_TEXT_ALIGN_CENTER);

  draw_text(area_.w / 2 - 40, area_.h / 2 + 18, "Press START",
            &lv_font_montserrat_10, color_text_, LV_TEXT_ALIGN_CENTER);
}

void GamePlatform::erase_rect_(int x, int y, int w, int h) {
  fill_rect_fast(x, y, w, h, color_bg_);
}

void GamePlatform::spawn_particles_(float x, float y, int count, lv_color_t color) {
  for (int i = 0; i < count && particles_.size() < MAX_PARTICLES; i++) {
    Particle p;
    p.active = true;
    p.x = x;
    p.y = y;
    p.vx = (rand01_() - 0.5f) * tile_size_ * 4;
    p.vy = (rand01_() - 0.5f) * tile_size_ * 4 - tile_size_ * 2;
    p.life = 0.5f + rand01_() * 0.5f;
    p.color = color;
    particles_.push_back(p);
  }
}

void GamePlatform::play_sound_(int sound_id) {
  // Sound implementation would go here
  // For now, just a placeholder
}

}  // namespace game_platform
}  // namespace esphome
