# © Copyright 2025 Stuart Parmenter
# SPDX-License-Identifier: MIT

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

from esphome.components import lvgl_game_runner

DEPENDENCIES = ["lvgl_game_runner"]

CONF_NUM_HUMAN_PLAYERS = "num_human_players"

game_pong_ns = cg.esphome_ns.namespace("game_pong")
GamePong = game_pong_ns.class_("GamePong", lvgl_game_runner.GameBase)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GamePong),
        cv.Optional(CONF_NUM_HUMAN_PLAYERS, default=1): cv.int_range(min=0, max=2),
    }
)


async def to_code(config):
    """Pong game initialization."""
    var = cg.new_Pvariable(config[CONF_ID])

    # Set number of human players (Pong supports max 2 players)
    cg.add(var.set_num_human_players(config[CONF_NUM_HUMAN_PLAYERS]))
