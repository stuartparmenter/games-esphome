// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#include "xbox_controller.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::ble_gamepad {

static const char *const TAG = "xbox_controller";

// Xbox BLE HID Report structure (empirically verified with Xbox One Model 1914 controller)
// 16-byte input report format (Report ID byte 0x01 stripped by BLE stack):
// Bytes 0-1: Left Stick X (16-bit LE, center=32768)
// Bytes 2-3: Left Stick Y (16-bit LE, center=32768)
// Bytes 4-5: Right Stick X (16-bit LE, center=32768)
// Bytes 6-7: Right Stick Y (16-bit LE, center=32768)
// Bytes 8-9: Left Trigger (10-bit LE, 0-1023, bits 0-9)
// Bytes 10-11: Right Trigger (10-bit LE, 0-1023, bits 0-9)
// Byte 12: D-Pad/Hat (0=center, 1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW)
// Byte 13: Face buttons (A=bit0, B=bit1, X=bit3, Y=bit4, LB=bit6, RB=bit7)
// Byte 14: System buttons (View, Menu, Xbox, L3, R3 - bit positions TBD)
// Byte 15: Share button (bit0)

static constexpr uint16_t STICK_CENTER_16 = 32768;

XboxController::XboxController() {
  state_.reset();
  ESP_LOGD(TAG, "Xbox BLE controller created");
}

void XboxController::on_connect() {
  ESP_LOGI(TAG, "Xbox controller connected");
  state_.connected = true;
}

void XboxController::on_disconnect() {
  ESP_LOGI(TAG, "Xbox controller disconnected");
  state_.reset();
}

bool XboxController::parse_input_report(const uint8_t *report, uint16_t len) {
  if (report == nullptr || len < 2) {
    ESP_LOGW(TAG, "Invalid report: null or too short");
    return false;
  }

  // Parse Xbox BLE HID input report
  return parse_ble_report_(report, len);
}

bool XboxController::parse_ble_report_(const uint8_t *data, uint16_t len) {
  // Xbox BLE HID input report: 16 bytes (Report ID 0x01 already stripped by BLE stack)
  if (len < 16) {
    ESP_LOGW(TAG, "BLE report too short: %d (expected 16)", len);
    return false;
  }

  // Bytes 0-7: Analog sticks (4x 16-bit little-endian, centered at 32768)
  uint16_t lx_raw = data[0] | (data[1] << 8);
  uint16_t ly_raw = data[2] | (data[3] << 8);
  uint16_t rx_raw = data[4] | (data[5] << 8);
  uint16_t ry_raw = data[6] | (data[7] << 8);

  // Convert from 0-65535 (center=32768) to -127 to +127 (center=0)
  // Invert Y axes to match standard gamepad orientation (up = positive)
  state_.left_stick_x = normalize_stick_16_(lx_raw);
  state_.left_stick_y = -normalize_stick_16_(ly_raw);
  state_.right_stick_x = normalize_stick_16_(rx_raw);
  state_.right_stick_y = -normalize_stick_16_(ry_raw);

  // Bytes 8-9: Left Trigger (10-bit LE, 0-1023), scale to 0-255
  uint16_t lt_raw = data[8] | ((data[9] & 0x03) << 8);
  state_.left_trigger = (lt_raw * 255) / 1023;

  // Bytes 10-11: Right Trigger (10-bit LE, 0-1023), scale to 0-255
  uint16_t rt_raw = data[10] | ((data[11] & 0x03) << 8);
  state_.right_trigger = (rt_raw * 255) / 1023;

  // Byte 12: D-Pad (hat switch: 0=center, 1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW)
  uint8_t hat = data[12];
  state_.buttons.dpad_up = (hat == 1 || hat == 2 || hat == 8);
  state_.buttons.dpad_down = (hat == 4 || hat == 5 || hat == 6);
  state_.buttons.dpad_left = (hat == 6 || hat == 7 || hat == 8);
  state_.buttons.dpad_right = (hat == 2 || hat == 3 || hat == 4);

  // Byte 13: Face buttons (A, B, X, Y, LB, RB)
  uint8_t btn13 = data[13];
  state_.buttons.button_south = (btn13 & 0x01) != 0;  // A = bit 0
  state_.buttons.button_east = (btn13 & 0x02) != 0;   // B = bit 1
  state_.buttons.button_west = (btn13 & 0x08) != 0;   // X = bit 3
  state_.buttons.button_north = (btn13 & 0x10) != 0;  // Y = bit 4
  state_.buttons.button_l1 = (btn13 & 0x40) != 0;     // LB = bit 6 (assumed)
  state_.buttons.button_r1 = (btn13 & 0x80) != 0;     // RB = bit 7 (assumed)

  // Byte 14: System buttons (View, Menu, Xbox, L3, R3)
  uint8_t btn14 = data[14];
  state_.buttons.button_select = (btn14 & 0x04) != 0;  // View (bit 2, needs verification)
  state_.buttons.button_start = (btn14 & 0x08) != 0;   // Menu (bit 3, needs verification)
  state_.buttons.button_home = (btn14 & 0x10) != 0;    // Xbox (bit 4) ✓
  state_.buttons.button_l3 = (btn14 & 0x20) != 0;      // L3 (bit 5, needs verification)
  state_.buttons.button_r3 = (btn14 & 0x40) != 0;      // R3 (bit 6, needs verification)

  // Byte 15: Share button
  uint8_t btn15 = data[15];
  state_.buttons.button_misc = (btn15 & 0x01) != 0;  // Share button (bit 0)

  // Single comprehensive log line showing all 16 bytes + parsed values (throttled to reduce log spam)
  static uint32_t log_counter = 0;
  if (++log_counter % 30 == 0) {  // Log once per second at 30fps
    ESP_LOGD(TAG,
             "Report: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X | LT=%3d RT=%3d "
             "B13=0x%02X B14=0x%02X B15=0x%02X Hat=%d",
             data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9], data[10],
             data[11], data[12], data[13], data[14], data[15], state_.left_trigger, state_.right_trigger, btn13, btn14,
             btn15, hat);
  }

  return true;
}

int8_t XboxController::normalize_stick_16_(uint16_t raw) {
  // Convert 0-65535 (center=32768) to -127 to 127 (center=0)
  int32_t centered = static_cast<int32_t>(raw) - STICK_CENTER_16;

  // Scale to int8_t range
  int8_t result = static_cast<int8_t>((centered * 127) / 32768);

  return result;
}

bool XboxController::set_rumble(uint8_t weak_magnitude, uint8_t strong_magnitude, uint16_t duration_ms) {
  // Cache rumble state for output reports
  rumble_.weak = weak_magnitude;
  rumble_.strong = strong_magnitude;

  // TODO: Implement BLE HID output report for rumble
  // Xbox BLE controllers support rumble via HID output reports (report ID 0x03)
  // Requires writing to HID Report Output characteristic with format:
  // [Report ID, Enable Rumble, Strong Motor, Weak Motor, Duration Hi, Duration Lo, ...]
  // Reference: https://github.com/atar-axis/xpadneo/blob/master/docs/protocol.md

  ESP_LOGW(TAG, "Rumble not yet implemented for BLE Xbox controllers (weak=%d, strong=%d, duration=%dms)",
           weak_magnitude, strong_magnitude, duration_ms);
  return false;  // Not implemented yet
}

bool XboxController::set_led_color(uint8_t r, uint8_t g, uint8_t b) {
  // Xbox controllers don't have RGB LEDs (only white LEDs with fixed patterns)
  // LED control is not exposed via BLE HID - requires proprietary Xbox Wireless protocol
  ESP_LOGW(TAG, "LED color control not supported on Xbox BLE controllers");
  return false;
}

}  // namespace esphome::ble_gamepad
