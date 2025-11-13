# © Copyright 2025 Stuart Parmenter
# SPDX-License-Identifier: MIT

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

from esphome.components import lvgl_game_runner

DEPENDENCIES = ["lvgl_game_runner"]

game_breakout_ns = cg.esphome_ns.namespace("game_breakout")
GameBreakout = game_breakout_ns.class_("GameBreakout", lvgl_game_runner.GameBase)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GameBreakout),
    }
)


async def to_code(config):
    """Breakout game initialization - currently no custom config needed."""
    cg.new_Pvariable(config[CONF_ID])
