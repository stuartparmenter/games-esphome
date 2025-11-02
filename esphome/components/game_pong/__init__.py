# © Copyright 2025 Stuart Parmenter
# SPDX-License-Identifier: MIT

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import lvgl_game_runner

DEPENDENCIES = ["lvgl_game_runner"]

game_pong_ns = cg.esphome_ns.namespace("game_pong")
GamePong = game_pong_ns.class_("GamePong", lvgl_game_runner.GameBase)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GamePong),
    }
)


@lvgl_game_runner.register_game("pong")
async def to_code(config, var):
    """Pong game initialization - currently no custom config needed."""
    pass
