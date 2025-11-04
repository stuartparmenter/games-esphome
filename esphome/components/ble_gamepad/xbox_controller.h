// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#pragma once

#include "controller_base.h"

namespace esphome::ble_gamepad {

/**
 * @brief Xbox controller parser for BLE mode.
 *
 * Parses HID over GATT input reports from Xbox controllers (model 1914+, firmware v5.15+).
 * Supports BLE mode only (Bluetooth Classic not supported on ESP32-S3).
 *
 * Reference: Xbox Wireless Controller BLE HID reports
 */
class XboxController : public ControllerBase {
 public:
  XboxController();
  ~XboxController() override = default;

  // ControllerBase interface
  bool parse_input_report(const uint8_t *report, uint16_t len) override;
  void on_connect() override;
  void on_disconnect() override;
  const char *get_controller_type() const override { return "Xbox Controller"; }

  bool supports_rumble() const override { return true; }
  bool set_rumble(uint8_t weak_magnitude, uint8_t strong_magnitude, uint16_t duration_ms) override;

  bool supports_led() const override { return true; }
  bool set_led_color(uint8_t r, uint8_t g, uint8_t b) override;

 private:
  /**
   * @brief Parse BLE HID report from Xbox controller.
   *
   * @param data Report data (including report ID)
   * @param len Report length
   * @return true on success
   */
  bool parse_ble_report_(const uint8_t *data, uint16_t len);

  /**
   * @brief Normalize analog stick value from raw 16-bit (0-65535, center=32768) to signed (-127 to 127).
   *
   * @param raw Raw 16-bit stick value from report
   * @return Normalized signed value
   */
  static int8_t normalize_stick_16_(uint16_t raw);

  /**
   * @brief Cached rumble state for output reports.
   */
  struct {
    uint8_t weak{0};
    uint8_t strong{0};
  } rumble_;
};

}  // namespace esphome::ble_gamepad
