import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import lvgl_game_runner

DEPENDENCIES = ["lvgl_game_runner"]

game_platform_ns = cg.esphome_ns.namespace("game_platform")
GamePlatform = game_platform_ns.class_("GamePlatform", lvgl_game_runner.GameBase)

# Configuration keys
CONF_NUM_HUMAN_PLAYERS = "num_human_players"
CONF_SEED = "seed"
CONF_INITIAL_LIVES = "initial_lives"
CONF_DIFFICULTY = "difficulty"
CONF_ENABLE_HIDDEN_LEVELS = "enable_hidden_levels"
CONF_BOSS_EVERY_N_LEVELS = "boss_every_n_levels"
CONF_PLAYER_SPEED = "player_speed"
CONF_JUMP_STRENGTH = "jump_strength"
CONF_GRAVITY = "gravity"
CONF_ENABLE_DOUBLE_JUMP = "enable_double_jump"
CONF_ENABLE_WALL_JUMP = "enable_wall_jump"
CONF_FRIENDLY_FIRE = "friendly_fire"
CONF_SHARED_LIVES = "shared_lives"
CONF_AI_DIFFICULTY = "ai_difficulty"
CONF_LEVEL_WIDTH_TILES = "level_width_tiles"
CONF_LEVEL_HEIGHT_TILES = "level_height_tiles"
CONF_MAX_ENEMIES_PER_LEVEL = "max_enemies_per_level"
CONF_POWERUP_FREQUENCY = "powerup_frequency"

# Difficulty enum
DIFFICULTY = {
    "easy": 0,
    "normal": 1,
    "hard": 2,
    "extreme": 3,
}

# AI difficulty enum
AI_DIFFICULTY = {
    "beginner": 0,
    "intermediate": 1,
    "advanced": 2,
    "expert": 3,
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(GamePlatform),

    # Multiplayer settings
    cv.Optional(CONF_NUM_HUMAN_PLAYERS, default=1): cv.int_range(min=0, max=4),

    # Level generation
    cv.Optional(CONF_SEED, default=12345): cv.uint32_t,
    cv.Optional(CONF_LEVEL_WIDTH_TILES, default=64): cv.int_range(min=20, max=256),
    cv.Optional(CONF_LEVEL_HEIGHT_TILES, default=16): cv.int_range(min=8, max=32),

    # Difficulty settings
    cv.Optional(CONF_DIFFICULTY, default="normal"): cv.enum(DIFFICULTY),
    cv.Optional(CONF_AI_DIFFICULTY, default="intermediate"): cv.enum(AI_DIFFICULTY),
    cv.Optional(CONF_INITIAL_LIVES, default=3): cv.int_range(min=1, max=9),

    # Level types
    cv.Optional(CONF_ENABLE_HIDDEN_LEVELS, default=True): cv.boolean,
    cv.Optional(CONF_BOSS_EVERY_N_LEVELS, default=5): cv.int_range(min=2, max=20),

    # Physics settings
    cv.Optional(CONF_PLAYER_SPEED, default=1.0): cv.float_range(min=0.5, max=3.0),
    cv.Optional(CONF_JUMP_STRENGTH, default=1.0): cv.float_range(min=0.5, max=2.0),
    cv.Optional(CONF_GRAVITY, default=1.0): cv.float_range(min=0.5, max=2.0),

    # Player abilities
    cv.Optional(CONF_ENABLE_DOUBLE_JUMP, default=False): cv.boolean,
    cv.Optional(CONF_ENABLE_WALL_JUMP, default=False): cv.boolean,

    # Multiplayer mechanics
    cv.Optional(CONF_FRIENDLY_FIRE, default=False): cv.boolean,
    cv.Optional(CONF_SHARED_LIVES, default=False): cv.boolean,

    # Content density
    cv.Optional(CONF_MAX_ENEMIES_PER_LEVEL, default=10): cv.int_range(min=1, max=50),
    cv.Optional(CONF_POWERUP_FREQUENCY, default=1.0): cv.float_range(min=0.0, max=5.0),
})


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    # Multiplayer
    cg.add(var.set_num_human_players(config[CONF_NUM_HUMAN_PLAYERS]))

    # Level generation
    cg.add(var.set_seed(config[CONF_SEED]))
    cg.add(var.set_level_dimensions(
        config[CONF_LEVEL_WIDTH_TILES],
        config[CONF_LEVEL_HEIGHT_TILES]
    ))

    # Difficulty
    cg.add(var.set_difficulty(config[CONF_DIFFICULTY]))
    cg.add(var.set_ai_difficulty(config[CONF_AI_DIFFICULTY]))
    cg.add(var.set_initial_lives(config[CONF_INITIAL_LIVES]))

    # Level types
    cg.add(var.set_hidden_levels_enabled(config[CONF_ENABLE_HIDDEN_LEVELS]))
    cg.add(var.set_boss_level_frequency(config[CONF_BOSS_EVERY_N_LEVELS]))

    # Physics
    cg.add(var.set_player_speed_multiplier(config[CONF_PLAYER_SPEED]))
    cg.add(var.set_jump_strength_multiplier(config[CONF_JUMP_STRENGTH]))
    cg.add(var.set_gravity_multiplier(config[CONF_GRAVITY]))

    # Abilities
    cg.add(var.set_double_jump_enabled(config[CONF_ENABLE_DOUBLE_JUMP]))
    cg.add(var.set_wall_jump_enabled(config[CONF_ENABLE_WALL_JUMP]))

    # Multiplayer mechanics
    cg.add(var.set_friendly_fire(config[CONF_FRIENDLY_FIRE]))
    cg.add(var.set_shared_lives(config[CONF_SHARED_LIVES]))

    # Content
    cg.add(var.set_max_enemies(config[CONF_MAX_ENEMIES_PER_LEVEL]))
    cg.add(var.set_powerup_frequency(config[CONF_POWERUP_FREQUENCY]))
