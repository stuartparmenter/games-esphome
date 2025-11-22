# © Copyright 2025 Stuart Parmenter
# SPDX-License-Identifier: MIT

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import lvgl_game_runner

DEPENDENCIES = ["lvgl_game_runner"]

game_zaxxon_ns = cg.esphome_ns.namespace("game_zaxxon")
GameZaxxon = game_zaxxon_ns.class_("GameZaxxon", lvgl_game_runner.GameBase)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GameZaxxon),
    }
)


@lvgl_game_runner.register_game("zaxxon")
async def to_code(config, var):
    """Zaxxon game initialization - currently no custom config needed."""
    pass
