# © Copyright 2025 Stuart Parmenter
# SPDX-License-Identifier: MIT

# esphome/components/lvgl_game_runner/__init__.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
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
CONF_PRESSED = "pressed"
CONF_PLAYER = "player"
CONF_NUM_HUMAN_PLAYERS = "num_human_players"

# Input type enum matching C++ InputType
InputTypeEnum = ns.enum("InputType", is_class=True)
INPUT_TYPES = {
    "UP": InputTypeEnum.UP,
    "DOWN": InputTypeEnum.DOWN,
    "LEFT": InputTypeEnum.LEFT,
    "RIGHT": InputTypeEnum.RIGHT,
    "A": InputTypeEnum.A,
    "B": InputTypeEnum.B,
    "SELECT": InputTypeEnum.SELECT,
    "START": InputTypeEnum.START,
    "L_TRIGGER": InputTypeEnum.L_TRIGGER,
    "R_TRIGGER": InputTypeEnum.R_TRIGGER,
    "ROTATE_CW": InputTypeEnum.ROTATE_CW,
    "ROTATE_CCW": InputTypeEnum.ROTATE_CCW,
    "TOUCH": InputTypeEnum.TOUCH,
}


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LvglGameRunner),
        cv.Required(CONF_CANVAS): cv.use_id(lvgl.Widget),
        cv.Optional(CONF_INITIAL_GAME): cv.use_id(GameBase),
        cv.Optional(CONF_FPS, default=30.0): cv.float_range(min=1.0, max=240.0),
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
    game_var = await cg.get_variable(config[CONF_GAME])
    cg.add(var.set_game(game_var))
    return var


@automation.register_action(
    "lvgl_game_runner.send_input",
    SendInputAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(LvglGameRunner),
            cv.Required(CONF_INPUT): cv.templatable(cv.enum(INPUT_TYPES)),
            cv.Optional(CONF_PLAYER, default=1): cv.templatable(cv.int_range(min=1, max=4)),
            cv.Required(CONF_PRESSED): cv.templatable(cv.boolean),
        }
    ),
)
@automation.register_action(
    "lvgl_game_runner.press_input",
    SendInputAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(LvglGameRunner),
            cv.Required(CONF_INPUT): cv.templatable(cv.enum(INPUT_TYPES)),
            cv.Optional(CONF_PLAYER, default=1): cv.templatable(cv.int_range(min=1, max=4)),
            cv.Optional(CONF_PRESSED, default=True): cv.templatable(cv.boolean),
        },
        key=CONF_INPUT,
    ),
)
@automation.register_action(
    "lvgl_game_runner.release_input",
    SendInputAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(LvglGameRunner),
            cv.Required(CONF_INPUT): cv.templatable(cv.enum(INPUT_TYPES)),
            cv.Optional(CONF_PLAYER, default=1): cv.templatable(cv.int_range(min=1, max=4)),
            cv.Optional(CONF_PRESSED, default=False): cv.templatable(cv.boolean),
        },
        key=CONF_INPUT,
    ),
)
async def input_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    tmpl = await cg.templatable(config[CONF_INPUT], args, InputTypeEnum)
    cg.add(var.set_input_type(tmpl))

    # Player number (defaults to 1)
    player_tmpl = await cg.templatable(config[CONF_PLAYER], args, cg.uint8)
    cg.add(var.set_player(player_tmpl))

    # All actions have CONF_PRESSED:
    # - send_input: required (user must specify)
    # - press_input: default=True
    # - release_input: default=False
    pressed_tmpl = await cg.templatable(config[CONF_PRESSED], args, bool)
    cg.add(var.set_pressed(pressed_tmpl))

    return var
