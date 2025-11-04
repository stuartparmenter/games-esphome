// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>

namespace esphome::ble_gamepad {

/**
 * @brief Unified controller state structure.
 *
 * Normalizes input from different controller types (PS5, Xbox, Switch, etc.)
 * into a common format for game consumption.
 */
struct ControllerState {
  // Buttons (bitfield for efficiency)
  struct {
    bool dpad_up : 1;
    bool dpad_down : 1;
    bool dpad_left : 1;
    bool dpad_right : 1;

    bool button_south : 1;  // Cross/A/B
    bool button_east : 1;   // Circle/B/A
    bool button_west : 1;   // Square/X/Y
    bool button_north : 1;  // Triangle/Y/X

    bool button_l1 : 1;  // L1/LB/L
    bool button_r1 : 1;  // R1/RB/R
    bool button_l2 : 1;  // L2/LT/ZL (digital)
    bool button_r2 : 1;  // R2/RT/ZR (digital)

    bool button_l3 : 1;  // L3 stick press
    bool button_r3 : 1;  // R3 stick press

    bool button_select : 1;  // Share/View/Minus
    bool button_start : 1;   // Options/Menu/Plus
    bool button_home : 1;    // PS/Xbox/Home
    bool button_misc : 1;    // Touchpad/Capture/etc.
  } buttons{};

  // Analog sticks (normalized to -127 to 127, 0 = center)
  int8_t left_stick_x{0};
  int8_t left_stick_y{0};
  int8_t right_stick_x{0};
  int8_t right_stick_y{0};

  // Analog triggers (0-255, 0 = not pressed)
  uint8_t left_trigger{0};
  uint8_t right_trigger{0};

  // Battery level (0-100, 255 = unavailable)
  uint8_t battery_level{255};

  // Connection status
  bool connected{false};

  /**
   * @brief Reset all state to defaults.
   */
  void reset() {
    buttons = {};
    left_stick_x = 0;
    left_stick_y = 0;
    right_stick_x = 0;
    right_stick_y = 0;
    left_trigger = 0;
    right_trigger = 0;
    battery_level = 255;
    connected = false;
  }
};

/**
 * @brief Abstract base class for controller implementations.
 *
 * Each controller type (PS5, Xbox, Switch, etc.) implements this interface
 * to parse HID reports and provide normalized state.
 */
class ControllerBase {
 public:
  virtual ~ControllerBase() = default;

  /**
   * @brief Parse HID input report and update controller state.
   *
   * @param report Raw HID report data
   * @param len Length of report data
   * @return true if parsing succeeded, false on error
   */
  virtual bool parse_input_report(const uint8_t *report, uint16_t len) = 0;

  /**
   * @brief Handle controller connection event.
   *
   * Called when controller is paired and ready. Can be used to:
   * - Request initial state
   * - Enable special features (motion sensors, LED control, etc.)
   * - Set controller-specific modes
   */
  virtual void on_connect() = 0;

  /**
   * @brief Handle controller disconnection event.
   */
  virtual void on_disconnect() = 0;

  /**
   * @brief Get current controller state.
   *
   * @return Reference to unified controller state
   */
  const ControllerState &get_state() const { return state_; }

  /**
   * @brief Get controller type name for logging/diagnostics.
   *
   * @return Controller type string (e.g., "PS5 DualSense", "Xbox Series")
   */
  virtual const char *get_controller_type() const = 0;

  /**
   * @brief Check if controller supports rumble/haptics.
   *
   * @return true if rumble is supported
   */
  virtual bool supports_rumble() const { return false; }

  /**
   * @brief Set rumble/haptic feedback (if supported).
   *
   * @param weak_magnitude Weak motor intensity (0-255)
   * @param strong_magnitude Strong motor intensity (0-255)
   * @param duration_ms Duration in milliseconds (0 = until stopped)
   * @return true if rumble command was sent successfully
   */
  virtual bool set_rumble(uint8_t weak_magnitude, uint8_t strong_magnitude, uint16_t duration_ms) {
    return false;  // Default: not supported
  }

  /**
   * @brief Check if controller supports LED control.
   *
   * @return true if LED control is supported
   */
  virtual bool supports_led() const { return false; }

  /**
   * @brief Set LED color (if supported).
   *
   * @param r Red component (0-255)
   * @param g Green component (0-255)
   * @param b Blue component (0-255)
   * @return true if LED command was sent successfully
   */
  virtual bool set_led_color(uint8_t r, uint8_t g, uint8_t b) {
    return false;  // Default: not supported
  }

 protected:
  ControllerState state_;  ///< Unified controller state
};

}  // namespace esphome::ble_gamepad
