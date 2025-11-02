# © Copyright 2025 Stuart Parmenter
# SPDX-License-Identifier: MIT

# esphome/components/lvgl_game_runner/__init__.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_WIDTH,
    CONF_HEIGHT,
    CONF_X,
    CONF_Y,
)
from esphome.components import lvgl
from esphome import automation

DEPENDENCIES = ["lvgl"]

ns = cg.esphome_ns.namespace("lvgl_game_runner")

LvglGameRunner = ns.class_("LvglGameRunner", cg.Component)
GameBase = ns.class_("GameBase")
GameRegistry = ns.class_("GameRegistry")
StartAction = ns.class_("StartAction", automation.Action)
PauseAction = ns.class_("PauseAction", automation.Action)
ResumeAction = ns.class_("ResumeAction", automation.Action)
ToggleAction = ns.class_("ToggleAction", automation.Action)
SetFpsAction = ns.class_("SetFpsAction", automation.Action)
SetGameAction = ns.class_("SetGameAction", automation.Action)
SendInputAction = ns.class_("SendInputAction", automation.Action)

# Custom config keys (not in esphome.const)
CONF_GAME_RUNNER = "lvgl_game_runner"
CONF_GAME = "game"
CONF_FPS = "fps"
CONF_CANVAS = "canvas"
CONF_START_PAUSED = "start_paused"


def register_game(name):
    """
    Helper function for game components to register with the framework.
    Similar to @register_addressable_effect() pattern in light component.

    Args:
        name: Game key for registration (e.g., "snake")

    Returns:
        Decorator that wraps the game's to_code function

    Note:
        CONFIG_SCHEMA is discovered automatically by ESPHome from the game component module.
    """

    def decorator(to_code_func):
        async def wrapper(config):
            # Create the game instance
            var = cg.new_Pvariable(config[CONF_ID])

            # Call the game's custom to_code logic (if any)
            await to_code_func(config, var)

            # Register with C++ GameRegistry (static method call)
            registry_call = cg.MockObj(f"{GameRegistry}::register_instance")
            cg.add(registry_call(name, var))

            return var

        return wrapper

    return decorator


ITEM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LvglGameRunner),
        cv.Required(CONF_GAME): cv.string,
        cv.Required(CONF_CANVAS): cv.use_id(lvgl.Widget),
        cv.Optional(CONF_FPS, default=30.0): cv.float_range(min=1.0, max=240.0),
        cv.Optional(CONF_X, default=0): cv.int_range(min=0),
        cv.Optional(CONF_Y, default=0): cv.int_range(min=0),
        cv.Optional(CONF_WIDTH, default=0): cv.int_range(min=0),
        cv.Optional(CONF_HEIGHT, default=0): cv.int_range(min=0),
        cv.Optional(CONF_START_PAUSED, default=False): cv.boolean,
    }
)

CONFIG_SCHEMA = cv.All(cv.ensure_list(ITEM_SCHEMA))


@automation.register_action(
    "lvgl_game_runner.start",
    StartAction,
    cv.Schema({cv.Required(CONF_ID): cv.use_id(LvglGameRunner)}),
)
async def start_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id)
    tgt = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_target(tgt))
    return var


@automation.register_action(
    "lvgl_game_runner.pause",
    PauseAction,
    cv.Schema({cv.Required(CONF_ID): cv.use_id(LvglGameRunner)}),
)
async def pause_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id)
    tgt = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_target(tgt))
    return var


@automation.register_action(
    "lvgl_game_runner.resume",
    ResumeAction,
    cv.Schema({cv.Required(CONF_ID): cv.use_id(LvglGameRunner)}),
)
async def resume_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id)
    tgt = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_target(tgt))
    return var


@automation.register_action(
    "lvgl_game_runner.toggle",
    ToggleAction,
    cv.Schema({cv.Required(CONF_ID): cv.use_id(LvglGameRunner)}),
)
async def toggle_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id)
    tgt = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_target(tgt))
    return var


@automation.register_action(
    "lvgl_game_runner.set_fps",
    SetFpsAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(LvglGameRunner),
            cv.Required(CONF_FPS): cv.float_range(min=1.0, max=240.0),
        }
    ),
)
async def setfps_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id)
    tgt = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_target(tgt))
    cg.add(var.set_fps(config[CONF_FPS]))
    return var


@automation.register_action(
    "lvgl_game_runner.set_game",
    SetGameAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(LvglGameRunner),
            cv.Required(CONF_GAME): cv.templatable(cv.string),
        }
    ),
)
async def setgame_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    tgt = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_target(tgt))
    tmpl = await cg.templatable(config[CONF_GAME], args, cg.std_string)
    cg.add(var.set_game(tmpl))
    return var


@automation.register_action(
    "lvgl_game_runner.send_input",
    SendInputAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(LvglGameRunner),
            cv.Required(CONF_INPUT): cv.templatable(cv.string),
        }
    ),
)
async def send_input_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    tgt = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_target(tgt))
    tmpl = await cg.templatable(config[CONF_INPUT], args, cg.std_string)
    cg.add(var.set_input_type(tmpl))
    return var


async def to_code(config):
    # Generate the component instances
    for item in config:
        var = cg.new_Pvariable(item[CONF_ID])
        await cg.register_component(var, {})

        period_ms = int(round(1000.0 / item[CONF_FPS]))
        cg.add(var.set_initial_period(period_ms))

        canvas_widget = await cg.get_variable(item[CONF_CANVAS])

        cg.add(
            var.setup_binding(
                canvas_widget,
                item[CONF_GAME],
                item[CONF_X],
                item[CONF_Y],
                item[CONF_WIDTH],
                item[CONF_HEIGHT],
                item[CONF_START_PAUSED],
            )
        )
