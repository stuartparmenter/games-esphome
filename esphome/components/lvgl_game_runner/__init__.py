# © Copyright 2025 Stuart Parmenter
# SPDX-License-Identifier: MIT

# esphome/components/lvgl_game_runner/__init__.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_HEIGHT,
    CONF_ID,
    CONF_INPUT,
    CONF_WIDTH,
    CONF_X,
    CONF_Y,
)

from esphome import automation
from esphome.components import lvgl

DEPENDENCIES = ["lvgl"]

ns = cg.esphome_ns.namespace("lvgl_game_runner")

LvglGameRunner = ns.class_("LvglGameRunner", cg.Component)
GameBase = ns.class_("GameBase")
StartAction = ns.class_(
    "StartAction", automation.Action, cg.Parented.template(LvglGameRunner)
)
PauseAction = ns.class_(
    "PauseAction", automation.Action, cg.Parented.template(LvglGameRunner)
)
ResumeAction = ns.class_(
    "ResumeAction", automation.Action, cg.Parented.template(LvglGameRunner)
)
ToggleAction = ns.class_(
    "ToggleAction", automation.Action, cg.Parented.template(LvglGameRunner)
)
SetFpsAction = ns.class_(
    "SetFpsAction", automation.Action, cg.Parented.template(LvglGameRunner)
)
SetGameAction = ns.class_(
    "SetGameAction", automation.Action, cg.Parented.template(LvglGameRunner)
)
SendInputAction = ns.class_(
    "SendInputAction", automation.Action, cg.Parented.template(LvglGameRunner)
)

# Custom config keys (not in esphome.const)
CONF_GAME = "game"
CONF_GAME_RUNNER = "lvgl_game_runner"
CONF_INITIAL_GAME = "initial_game"
CONF_FPS = "fps"
CONF_CANVAS = "canvas"
CONF_START_PAUSED = "start_paused"


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LvglGameRunner),
        cv.Required(CONF_CANVAS): cv.use_id(lvgl.Widget),
        cv.Optional(CONF_INITIAL_GAME): cv.use_id(GameBase),
        cv.Optional(CONF_FPS, default=30.0): cv.float_range(min=1.0, max=240.0),
        cv.Optional(CONF_X, default=0): cv.int_range(min=0),
        cv.Optional(CONF_Y, default=0): cv.int_range(min=0),
        cv.Optional(CONF_WIDTH, default=0): cv.int_range(min=0),
        cv.Optional(CONF_HEIGHT, default=0): cv.int_range(min=0),
        cv.Optional(CONF_START_PAUSED, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    period_ms = int(round(1000.0 / config[CONF_FPS]))
    cg.add(var.set_initial_period(period_ms))

    canvas_widget = await cg.get_variable(config[CONF_CANVAS])

    if initial_game := config.get(CONF_INITIAL_GAME):
        initial_game_var = await cg.get_variable(initial_game)

    cg.add(
        var.setup_binding(
            canvas_widget,
            initial_game_var if initial_game else None,
            config[CONF_X],
            config[CONF_Y],
            config[CONF_WIDTH],
            config[CONF_HEIGHT],
            config[CONF_START_PAUSED],
        )
    )


@automation.register_action(
    "lvgl_game_runner.start",
    StartAction,
    cv.Schema({cv.GenerateID(): cv.use_id(LvglGameRunner)}),
)
@automation.register_action(
    "lvgl_game_runner.pause",
    PauseAction,
    cv.Schema({cv.GenerateID(): cv.use_id(LvglGameRunner)}),
)
@automation.register_action(
    "lvgl_game_runner.resume",
    ResumeAction,
    cv.Schema({cv.GenerateID(): cv.use_id(LvglGameRunner)}),
)
@automation.register_action(
    "lvgl_game_runner.toggle",
    ToggleAction,
    cv.Schema({cv.GenerateID(): cv.use_id(LvglGameRunner)}),
)
async def toggle_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "lvgl_game_runner.set_fps",
    SetFpsAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(LvglGameRunner),
            cv.Required(CONF_FPS): cv.float_range(min=1.0, max=240.0),
        },
        key=CONF_FPS,
    ),
)
async def setfps_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_fps(config[CONF_FPS]))
    return var


@automation.register_action(
    "lvgl_game_runner.set_game",
    SetGameAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(LvglGameRunner),
            cv.Required(CONF_GAME): cv.use_id(GameBase),
        },
        key=CONF_GAME,
    ),
)
async def setgame_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    tmpl = await cg.templatable(config[CONF_INITIAL_GAME], args, cg.std_string)
    cg.add(var.set_game(tmpl))
    return var


@automation.register_action(
    "lvgl_game_runner.send_input",
    SendInputAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(LvglGameRunner),
            cv.Required(CONF_INPUT): cv.templatable(cv.string),
        },
        key=CONF_INPUT,
    ),
)
async def send_input_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    tmpl = await cg.templatable(config[CONF_INPUT], args, cg.std_string)
    cg.add(var.set_input_type(tmpl))
    return var
