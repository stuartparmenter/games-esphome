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

  // Rotary encoder
  ROTATE_CW,   // Clockwise rotation
  ROTATE_CCW,  // Counter-clockwise rotation

  // Future: touchscreen
  TOUCH,
};

/**
 * Input event structure.
 */
struct InputEvent {
  InputType type;
  bool pressed;   // true = press/trigger, false = release
  int16_t value;  // Optional: analog value, touch coordinates, rotation steps, etc.

  InputEvent() : type(InputType::UP), pressed(false), value(0) {}
  InputEvent(InputType t, bool p = true, int16_t v = 0) : type(t), pressed(p), value(v) {}
};

}  // namespace esphome::lvgl_game_runner
