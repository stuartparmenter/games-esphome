// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#pragma once

#include "controller_base.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/esp32_ble/ble.h"

#include <memory>
#include <vector>
#include <map>

// ESP-IDF BLE includes
#ifdef USE_ESP_IDF
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_defs.h"
#endif

namespace esphome::ble_gamepad {

// BLE Device Information Service UUID (0x180A)
static constexpr uint16_t DIS_SERVICE_UUID = 0x180A;

// BLE Device Information Service characteristic UUIDs
static constexpr uint16_t DIS_PNP_ID_UUID = 0x2A50;
static constexpr uint16_t DIS_MANUFACTURER_NAME_UUID = 0x2A29;
static constexpr uint16_t DIS_MODEL_NUMBER_UUID = 0x2A24;
static constexpr uint16_t DIS_SERIAL_NUMBER_UUID = 0x2A25;

// BLE HID service UUID (0x1812)
static constexpr uint16_t HID_SERVICE_UUID = 0x1812;

// BLE HID characteristic UUIDs
static constexpr uint16_t HID_REPORT_UUID = 0x2A4D;
static constexpr uint16_t HID_REPORT_MAP_UUID = 0x2A4B;
static constexpr uint16_t HID_INFO_UUID = 0x2A4A;
static constexpr uint16_t HID_CONTROL_POINT_UUID = 0x2A4C;
static constexpr uint16_t PROTOCOL_MODE_UUID = 0x2A4E;

/**
 * @brief Main BLE gamepad component.
 *
 * Integrates with ESPHome's esp32_ble component to connect to BLE HID gamepads.
 * Uses HID over GATT Profile (HOGP) to receive input from controllers.
 * Delegates controller-specific parsing to ControllerBase implementations.
 */
class BLEGamepad : public Component,
                   public esp32_ble::GAPEventHandler,
                   public esp32_ble::GAPScanEventHandler,
                   public esp32_ble::GATTcEventHandler {
 public:
  BLEGamepad() = default;

  // ESPHome lifecycle
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  /**
   * @brief Get current controller state (if connected).
   *
   * @return Pointer to state, or nullptr if no controller connected
   */
  const ControllerState *get_state() const;

  /**
   * @brief Check if any controller is currently connected.
   *
   * @return true if connected
   */
  bool is_connected() const { return active_controller_ != nullptr && gattc_if_ != ESP_GATT_IF_NONE; }

  /**
   * @brief Register automation triggers.
   */
  void add_on_connect_callback(std::function<void()> &&callback) { on_connect_callbacks_.add(std::move(callback)); }
  void add_on_disconnect_callback(std::function<void()> &&callback) {
    on_disconnect_callbacks_.add(std::move(callback));
  }
  void add_on_button_callback(std::function<void(std::string, bool)> &&callback) {
    on_button_callbacks_.add(std::move(callback));
  }
  void add_on_stick_callback(std::function<void()> &&callback) { on_stick_callbacks_.add(std::move(callback)); }

 protected:
#ifdef USE_ESP_IDF
  /**
   * @brief Start BLE scanning for HID devices.
   */
  void start_scan_();

  /**
   * @brief GAP event handler from esp32_ble.
   *
   * Handles BLE scanning, connection, and disconnection events.
   */
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

  /**
   * @brief GAP scan event handler from esp32_ble.
   *
   * Receives BLE scan results and filters for HID/gamepad devices.
   */
  void gap_scan_event_handler(const esp32_ble::BLEScanResult &scan_result) override;

  /**
   * @brief GATT client event handler from esp32_ble.
   *
   * Handles GATT service discovery, characteristic subscription, and notifications.
   */
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  /**
   * @brief Handle GATT notification (HID report).
   *
   * Delegates parsing to active controller implementation.
   *
   * @param value Report data
   * @param value_len Report length
   */
  void handle_notification_(uint8_t *value, uint16_t value_len);

  /**
   * @brief Connect to discovered HID device.
   *
   * @param bda Device Bluetooth address
   */
  void connect_to_device_(esp_bd_addr_t bda);

  /**
   * @brief Disconnect from current device.
   */
  void disconnect_();

  /**
   * @brief Enable notifications on all HID Report characteristics.
   *
   * @param gattc_if GATT client interface
   */
  void enable_all_notifications_(esp_gatt_if_t gattc_if);

  // BLE connection state
  esp_gatt_if_t gattc_if_{ESP_GATT_IF_NONE};
  uint16_t conn_id_{0};
  esp_bd_addr_t remote_bda_{};
  bool connected_{false};
  bool scanning_{false};
  bool gatt_registered_{false};

  // Device Information Service handles
  uint16_t dis_service_start_handle_{0};
  uint16_t dis_service_end_handle_{0};
  uint16_t dis_pnp_id_handle_{0};  // PnP ID characteristic (0x2A50) - contains VID/PID

  // Controller identification (from PnP ID)
  uint16_t vendor_id_{0};   // e.g., 0x045e for Microsoft
  uint16_t product_id_{0};  // e.g., 0x02e0 for Xbox One BLE

  // GATT service/characteristic handles
  uint16_t hid_service_start_handle_{0};
  uint16_t hid_service_end_handle_{0};
  uint16_t hid_info_handle_{0};        // HID Information (0x2A4A) - required for HOGP
  uint16_t hid_report_map_handle_{0};  // HID Report Map (0x2A4B) - CRITICAL for Xbox pairing
  uint16_t protocol_mode_handle_{0};   // Protocol Mode (0x2A4E) - optional for Report-only devices

  // HID Report characteristics (multiple reports: input, output, feature)
  struct HIDReportCharacteristic {
    uint16_t char_handle;
    uint16_t ccc_handle;  // Client Characteristic Configuration descriptor
  };
  std::vector<HIDReportCharacteristic> hid_report_chars_;  // All HID Report characteristics

  // HID Report Map storage (required for Xbox controllers)
  std::vector<uint8_t> hid_report_map_;

  // HOGP initialization state tracking
  enum class InitState {
    IDLE,
    READING_DIS_PNPID,  // Reading Device Information Service PnP ID
    READING_HID_INFO,
    READING_REPORT_MAP,
    SETTING_PROTOCOL_MODE,
    REGISTERING_NOTIFICATIONS,  // Calling esp_ble_gattc_register_for_notify() for each HID Report
    ENABLING_NOTIFICATIONS,     // Writing CCC descriptors to enable notifications
    READING_INITIAL_REPORT,     // Reading first input report to "prime" the controller
    COMPLETE
  };
  InitState init_state_{InitState::IDLE};

  // Service discovery retry counter (prevents infinite loops if services missing)
  static constexpr uint8_t MAX_DISCOVERY_RETRIES = 3;
  uint8_t service_discovery_retries_{0};

  // Index for sequential notification registration (Bluedroid requirement)
  size_t current_notify_index_{0};

  // Active controller (nullptr if disconnected)
  std::unique_ptr<ControllerBase> active_controller_{nullptr};

  // Previous state for change detection (triggers)
  ControllerState prev_state_{};

  // Automation trigger callbacks
  CallbackManager<void()> on_connect_callbacks_;
  CallbackManager<void()> on_disconnect_callbacks_;
  CallbackManager<void(std::string, bool)> on_button_callbacks_;
  CallbackManager<void()> on_stick_callbacks_;
#endif
};

// ===== Automation Triggers =====

/**
 * @brief Trigger fired when controller connects.
 */
class BLEGamepadConnectTrigger : public Trigger<> {
 public:
  explicit BLEGamepadConnectTrigger(BLEGamepad *parent) {
    parent->add_on_connect_callback([this]() { this->trigger(); });
  }
};

/**
 * @brief Trigger fired when controller disconnects.
 */
class BLEGamepadDisconnectTrigger : public Trigger<> {
 public:
  explicit BLEGamepadDisconnectTrigger(BLEGamepad *parent) {
    parent->add_on_disconnect_callback([this]() { this->trigger(); });
  }
};

/**
 * @brief Trigger fired when any button state changes.
 *
 * Passes the button name (e.g., "UP", "A") and pressed state (true/false).
 */
class BLEGamepadButtonTrigger : public Trigger<std::string, bool> {
 public:
  explicit BLEGamepadButtonTrigger(BLEGamepad *parent) {
    parent->add_on_button_callback([this](std::string input, bool pressed) { this->trigger(input, pressed); });
  }
};

/**
 * @brief Trigger fired when analog stick values change.
 */
class BLEGamepadStickTrigger : public Trigger<> {
 public:
  explicit BLEGamepadStickTrigger(BLEGamepad *parent) {
    parent->add_on_stick_callback([this]() { this->trigger(); });
  }
};

}  // namespace esphome::ble_gamepad
