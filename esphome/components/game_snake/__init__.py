# © Copyright 2025 Stuart Parmenter
# SPDX-License-Identifier: MIT

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

from esphome.components import lvgl_game_runner

DEPENDENCIES = ["lvgl_game_runner"]

CONF_NUM_HUMAN_PLAYERS = "num_human_players"

game_snake_ns = cg.esphome_ns.namespace("game_snake")
GameSnake = game_snake_ns.class_("GameSnake", lvgl_game_runner.GameBase)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GameSnake),
        cv.Optional(CONF_NUM_HUMAN_PLAYERS, default=1): cv.int_range(min=0, max=1),
    }
)


async def to_code(config):
    """Snake game initialization."""
    var = cg.new_Pvariable(config[CONF_ID])

    # Set number of human players (Snake supports max 1 player)
    cg.add(var.set_num_human_players(config[CONF_NUM_HUMAN_PLAYERS]))
