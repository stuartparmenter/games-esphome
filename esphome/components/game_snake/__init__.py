# © Copyright 2025 Stuart Parmenter
# SPDX-License-Identifier: MIT

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import lvgl_game_runner

DEPENDENCIES = ["lvgl_game_runner"]

game_snake_ns = cg.esphome_ns.namespace("game_snake")
GameSnake = game_snake_ns.class_("GameSnake", lvgl_game_runner.GameBase)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GameSnake),
    }
)


@lvgl_game_runner.register_game("snake")
async def to_code(config, var):
    """Snake game initialization - currently no custom config needed."""
    pass
