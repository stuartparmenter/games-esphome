# © Copyright 2025 Stuart Parmenter
# SPDX-License-Identifier: MIT

"""
BLE gamepad component for ESPHome.

Enables ESP32-S3 to accept connections from BLE gamepads
(Xbox controllers with BLE support) using ESP-IDF's BLE stack.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_TRIGGER_ID
from esphome.components import esp32_ble
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.final_validate as fv

# Only works with ESP-IDF framework
DEPENDENCIES = ["esp32", "esp32_ble"]
AUTO_LOAD = ["esp32_ble"]
CODEBASE_COMPONENTS = ["esp32"]

# Component namespace
ble_gamepad_ns = cg.esphome_ns.namespace("ble_gamepad")

# Classes - C++ inherits from GAPEventHandler, GAPScanEventHandler, and GATTcEventHandler
# (GAPScanEventHandler not listed here - not exported from esp32_ble, but C++ has it)
BLEGamepad = ble_gamepad_ns.class_(
    "BLEGamepad", cg.Component, esp32_ble.GAPEventHandler, esp32_ble.GATTcEventHandler
)

# Triggers
BLEGamepadConnectTrigger = ble_gamepad_ns.class_(
    "BLEGamepadConnectTrigger", automation.Trigger.template()
)
BLEGamepadDisconnectTrigger = ble_gamepad_ns.class_(
    "BLEGamepadDisconnectTrigger", automation.Trigger.template()
)
BLEGamepadButtonTrigger = ble_gamepad_ns.class_(
    "BLEGamepadButtonTrigger", automation.Trigger.template()
)
BLEGamepadStickTrigger = ble_gamepad_ns.class_(
    "BLEGamepadStickTrigger", automation.Trigger.template()
)

# Config keys
CONF_ON_CONNECT = "on_connect"
CONF_ON_DISCONNECT = "on_disconnect"
CONF_ON_BUTTON = "on_button"
CONF_ON_STICK = "on_stick"

# Configuration schema
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEGamepad),
        cv.GenerateID(esp32_ble.CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
        cv.Optional(CONF_ON_CONNECT): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEGamepadConnectTrigger),
            }
        ),
        cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    BLEGamepadDisconnectTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_BUTTON): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEGamepadButtonTrigger),
            }
        ),
        cv.Optional(CONF_ON_STICK): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEGamepadStickTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_psram(config):
    """Validate that PSRAM is configured for BLE gamepad component.

    The BLE stack (Bluedroid) requires significant internal RAM. By using
    CONFIG_BT_ALLOCATION_FROM_SPIRAM_FIRST, we move BLE allocations to PSRAM.
    Without PSRAM, the component will cause memory exhaustion and crashes.
    """
    full_config = fv.full_config.get()

    # Check if PSRAM component is present (it's a top-level component)
    if "psram" in full_config:
        # PSRAM is configured - validation passes
        return config

    # PSRAM not found
    raise cv.Invalid(
        "ble_gamepad requires PSRAM to be enabled due to BLE stack memory requirements.\n"
        "The Bluedroid BLE stack uses significant internal RAM (~20-30KB), which causes\n"
        "memory exhaustion on ESP32-S3 without PSRAM.\n\n"
        "Please add PSRAM configuration as a top-level component:\n\n"
        "psram:\n"
        "  mode: octal  # or quad, depending on your ESP32-S3 board\n"
        "  speed: 80MHz\n\n"
        "If your board doesn't support PSRAM, this component cannot be used."
    )


FINAL_VALIDATE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): fv.id_declaration_match_schema(_validate_psram),
    },
    extra=cv.ALLOW_EXTRA,
)


async def to_code(config):
    """Generate C++ code for ble_gamepad component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Get the esp32_ble parent component
    parent = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])

    # Register as event handlers (this increments handler counts for proper sizing)
    esp32_ble.register_gap_event_handler(parent, var)
    esp32_ble.register_gap_scan_event_handler(parent, var)
    esp32_ble.register_gattc_event_handler(parent, var)

    # Enable GATT client interface (required for GATTcEventHandler to exist)
    cg.add_define("USE_ESP32_BLE_CLIENT")

    # Enable GATT client in ESP-IDF (esp32_ble's final_validation only sets this
    # when esp32_ble_tracker/client are loaded, but we're a direct GATT client)
    add_idf_sdkconfig_option("CONFIG_BT_GATTC_ENABLE", True)
    add_idf_sdkconfig_option(
        "CONFIG_BT_GATTS_ENABLE", True
    )  # Required when GATTC is enabled

    # Gamepad-specific BLE configuration - minimize memory usage
    add_idf_sdkconfig_option(
        "CONFIG_BT_GATT_MAX_SR_PROFILES", 2
    )  # Reduced from 8 - only need HID service
    add_idf_sdkconfig_option(
        "CONFIG_BTDM_CTRL_MODE_BLE_ONLY", True
    )  # ESP32-S3 BLE-only mode

    # Reduce Bluedroid memory footprint
    add_idf_sdkconfig_option(
        "CONFIG_BT_BLE_DYNAMIC_ENV_MEMORY", True
    )  # Use dynamic memory allocation

    # Allocate BLE memory from PSRAM first (requires PSRAM-enabled ESP32-S3)
    # This moves BLE stack allocations out of precious internal RAM
    add_idf_sdkconfig_option("CONFIG_BT_ALLOCATION_FROM_SPIRAM_FIRST", True)

    # Register automation triggers
    for conf in config.get(CONF_ON_CONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_DISCONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_BUTTON, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_string, "input"), (cg.bool_, "pressed")], conf
        )

    for conf in config.get(CONF_ON_STICK, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
