// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace esphome::lvgl_game_runner {

/**
 * Generic input types supporting various hardware sources.
 * Games use these abstract input types, and ESPHome YAML maps
 * hardware (buttons, encoders, touchscreen) to these events.
 */
enum class InputType {
  // Directional inputs
  UP,
  DOWN,
  LEFT,
  RIGHT,

  // Action buttons
  A,
  B,
  SELECT,
  START,

  // Analog triggers (use event.value for analog data 0-255)
  L_TRIGGER,
  R_TRIGGER,

  // Rotary encoder
  ROTATE_CW,   // Clockwise rotation
  ROTATE_CCW,  // Counter-clockwise rotation

  // Future: touchscreen
  TOUCH,

  // No input (used by AI to indicate no action)
  NONE,
};

/**
 * Input event structure.
 */
struct InputEvent {
  InputType type;
  uint8_t player;  // Player number (1-4)
  bool pressed;    // true = press/trigger, false = release
  int16_t value;   // Optional: analog value, touch coordinates, rotation steps, etc.

  InputEvent() : type(InputType::NONE), player(1), pressed(false), value(0) {}
  InputEvent(InputType t, uint8_t pl = 1, bool p = true, int16_t v = 0) : type(t), player(pl), pressed(p), value(v) {}
};

}  // namespace esphome::lvgl_game_runner
