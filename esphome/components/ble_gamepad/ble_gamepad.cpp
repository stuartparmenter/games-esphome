// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#include "ble_gamepad.h"
#include "xbox_controller.h"
#include "esphome/core/log.h"
#include "esphome/components/esp32_ble/ble.h"
#include <inttypes.h>  // For PRIu32 format specifier

#ifdef USE_ESP_IDF

namespace esphome::ble_gamepad {

static const char *const TAG = "ble_gamepad";

// Known Xbox controller identifiers
static constexpr uint16_t BLE_APPEARANCE_GAMEPAD = 0x03C4;  // Generic Gamepad

// GATT app ID for registration (arbitrary value, must be unique within application)
// This ID is used to identify this component's GATT client instance
static constexpr uint16_t GATTC_APP_ID = 0x1234;

void BLEGamepad::setup() {
  ESP_LOGI(TAG, "Setting up BLE gamepad component");

  // GATT client registration will happen in loop() when BLE is active
  // Event handler registration is done via Python __init__.py
}

void BLEGamepad::loop() {
  // Check if BLE stack is active (same pattern as BLEClientBase)
  if (!esp32_ble::global_ble->is_active()) {
    return;  // BLE not ready yet
  }

  // Register GATT client app once BLE is active
  if (this->gattc_if_ == ESP_GATT_IF_NONE && !this->gatt_registered_) {
    this->gatt_registered_ = true;
    esp_err_t ret = esp_ble_gattc_app_register(GATTC_APP_ID);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "GATTC app registration failed: %s", esp_err_to_name(ret));
      this->mark_failed();
      return;
    }
    ESP_LOGI(TAG, "GATT client app registration initiated");
  }

  // Main loop - check for state changes and fire triggers
  if (active_controller_ == nullptr) {
    return;
  }

  const ControllerState &current = active_controller_->get_state();

  // Check for button changes
  bool button_changed = false;
  if (memcmp(&current.buttons, &prev_state_.buttons, sizeof(current.buttons)) != 0) {
    button_changed = true;
  }

  // Check for analog stick changes (with small deadzone to avoid noise)
  bool stick_changed = false;
  constexpr int8_t STICK_DEADZONE = 5;
  if (abs(current.left_stick_x - prev_state_.left_stick_x) > STICK_DEADZONE ||
      abs(current.left_stick_y - prev_state_.left_stick_y) > STICK_DEADZONE ||
      abs(current.right_stick_x - prev_state_.right_stick_x) > STICK_DEADZONE ||
      abs(current.right_stick_y - prev_state_.right_stick_y) > STICK_DEADZONE) {
    stick_changed = true;
  }

  // Fire triggers and log changes
  if (button_changed) {
    ESP_LOGD(TAG, "Button state changed:");

    // Macro to check a button change, log it, and fire callback
#define CHECK_BUTTON(field, name, log_name) \
  if (current.buttons.field != prev_state_.buttons.field) { \
    ESP_LOGD(TAG, "  %s: %s", log_name, current.buttons.field ? "PRESSED" : "released"); \
    on_button_callbacks_.call(name, current.buttons.field); \
  }

    // D-pad (map to UP/DOWN/LEFT/RIGHT for game runner compatibility)
    CHECK_BUTTON(dpad_up, "UP", "D-Up")
    CHECK_BUTTON(dpad_down, "DOWN", "D-Down")
    CHECK_BUTTON(dpad_left, "LEFT", "D-Left")
    CHECK_BUTTON(dpad_right, "RIGHT", "D-Right")

    // Face buttons
    CHECK_BUTTON(button_south, "A", "A")  // Xbox A / PS Cross
    CHECK_BUTTON(button_east, "B", "B")   // Xbox B / PS Circle

    // System buttons
    CHECK_BUTTON(button_select, "SELECT", "View")  // Xbox View / PS Share
    CHECK_BUTTON(button_start, "START", "Menu")    // Xbox Menu / PS Options

    // Additional buttons (for logging, games can choose to use or ignore)
    CHECK_BUTTON(button_west, "X", "X")         // Xbox X / PS Square
    CHECK_BUTTON(button_north, "Y", "Y")        // Xbox Y / PS Triangle
    CHECK_BUTTON(button_l1, "L1", "LB")         // Xbox LB / PS L1
    CHECK_BUTTON(button_r1, "R1", "RB")         // Xbox RB / PS R1
    CHECK_BUTTON(button_l3, "L3", "L3")         // Left stick press
    CHECK_BUTTON(button_r3, "R3", "R3")         // Right stick press
    CHECK_BUTTON(button_home, "HOME", "Xbox")   // Xbox button / PS button
    CHECK_BUTTON(button_misc, "MISC", "Share")  // Xbox Share button

#undef CHECK_BUTTON
  }
  if (stick_changed) {
    ESP_LOGD(TAG, "Stick changed: LX=%d LY=%d RX=%d RY=%d", current.left_stick_x, current.left_stick_y,
             current.right_stick_x, current.right_stick_y);
    on_stick_callbacks_.call();
  }

  // Log trigger changes (with threshold to avoid noise)
  constexpr uint8_t TRIGGER_THRESHOLD = 10;
  if (abs(static_cast<int>(current.left_trigger) - static_cast<int>(prev_state_.left_trigger)) > TRIGGER_THRESHOLD ||
      abs(static_cast<int>(current.right_trigger) - static_cast<int>(prev_state_.right_trigger)) > TRIGGER_THRESHOLD) {
    ESP_LOGD(TAG, "Triggers: LT=%d RT=%d", current.left_trigger, current.right_trigger);
  }

  // Update previous state
  prev_state_ = current;
}

void BLEGamepad::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Gamepad:");
  if (active_controller_) {
    ESP_LOGCONFIG(TAG, "  Controller: %s", active_controller_->get_controller_type());
    ESP_LOGCONFIG(TAG, "  Connected: Yes");
  } else {
    ESP_LOGCONFIG(TAG, "  Connected: No");
    ESP_LOGCONFIG(TAG, "  Scanning: %s", scanning_ ? "Yes" : "No");
  }
}

const ControllerState *BLEGamepad::get_state() const {
  if (active_controller_ == nullptr) {
    return nullptr;
  }
  return &active_controller_->get_state();
}

void BLEGamepad::start_scan_() {
  // Configure scan parameters
  esp_ble_scan_params_t scan_params = {
      .scan_type = BLE_SCAN_TYPE_ACTIVE,
      .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
      .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
      .scan_interval = 0x50,  // 50ms
      .scan_window = 0x30,    // 30ms
      .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
  };

  esp_err_t ret = esp_ble_gap_set_scan_params(&scan_params);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Set scan params failed: %s", esp_err_to_name(ret));
    return;
  }

  ESP_LOGI(TAG, "Starting BLE scan for HID devices");
  scanning_ = true;
}

void BLEGamepad::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
      ESP_LOGI(TAG, "Scan parameters set, starting scan");
      esp_ble_gap_start_scanning(0);  // 0 = scan indefinitely
      break;
    }

    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: {
      if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Scan start failed");
        this->scanning_ = false;
      } else {
        ESP_LOGI(TAG, "Scan started successfully");
      }
      break;
    }

      // Note: ESP_GAP_BLE_SCAN_RESULT_EVT is handled by gap_scan_event_handler()
      // (dispatched separately by ESP32BLE to GAPScanEventHandler)

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
      ESP_LOGI(TAG, "Scan stopped");
      this->scanning_ = false;
      break;

      // ========== Security/Pairing Events (required for Xbox controller bonding) ==========

    case ESP_GAP_BLE_SEC_REQ_EVT:
      // Peer device (controller) requests security - accept it
      ESP_LOGI(TAG, "Security request from " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(param->ble_security.ble_req.bd_addr));
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      break;

    case ESP_GAP_BLE_NC_REQ_EVT:
      // Numeric Comparison request - auto-accept for Just Works pairing (Xbox uses this)
      ESP_LOGI(TAG, "Numeric comparison request, passkey: %06" PRIu32, param->ble_security.key_notif.passkey);
      esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
      break;

    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
      // Passkey notification (shouldn't happen with IO_CAP_NONE, but log just in case)
      ESP_LOGI(TAG, "Passkey notification: %06" PRIu32, param->ble_security.key_notif.passkey);
      break;

    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
      // Authentication/pairing complete - now safe to do GATT operations
      if (param->ble_security.auth_cmpl.success) {
        ESP_LOGI(TAG, "Authentication success with " ESP_BD_ADDR_STR,
                 ESP_BD_ADDR_HEX(param->ble_security.auth_cmpl.bd_addr));
        ESP_LOGI(TAG, "  Bonded: %s, Encryption: %s",
                 param->ble_security.auth_cmpl.auth_mode & ESP_LE_AUTH_BOND ? "Yes" : "No",
                 param->ble_security.auth_cmpl.auth_mode & ESP_LE_AUTH_REQ_MITM ? "MITM" : "Legacy");

        // Following bluepad32's sequence: Query Device Information Service FIRST
        // This reads PnP ID (VID/PID) which may be required for controller to activate
        if (this->connected_ && this->gattc_if_ != ESP_GATT_IF_NONE) {
          ESP_LOGI(TAG, "Searching for Device Information Service (DIS)");
          // Search for DIS service (0x180A) specifically
          esp_bt_uuid_t dis_uuid;
          dis_uuid.len = ESP_UUID_LEN_16;
          dis_uuid.uuid.uuid16 = DIS_SERVICE_UUID;
          esp_ble_gattc_search_service(this->gattc_if_, this->conn_id_, &dis_uuid);
        }
      } else {
        ESP_LOGE(TAG, "Authentication failed with " ESP_BD_ADDR_STR " (reason: %d)",
                 ESP_BD_ADDR_HEX(param->ble_security.auth_cmpl.bd_addr), param->ble_security.auth_cmpl.fail_reason);
        // Disconnect and retry
        this->disconnect_();
      }
      break;
    }

    case ESP_GAP_BLE_KEY_EVT:
      // Key exchange event - handled automatically by stack, just log
      ESP_LOGD(TAG, "Key event: type=%d", param->ble_security.ble_key.key_type);
      break;

    case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT:
      ESP_LOGI(TAG, "Bond device removed, status: %d", param->remove_bond_dev_cmpl.status);
      break;

    default:
      ESP_LOGD(TAG, "Unhandled GAP event: %d", event);
      break;
  }
}

void BLEGamepad::gap_scan_event_handler(const esp32_ble::BLEScanResult &scan_result) {
  // Only process inquiry results when not connected
  if (this->connected_)
    return;

  // Check if device has HID service UUID or gamepad indicators
  uint8_t *adv_data = const_cast<uint8_t *>(scan_result.ble_adv);
  uint8_t adv_data_len = scan_result.adv_data_len;

  bool is_hid_device = false;
  bool is_gamepad = false;

  // Parse advertising data for HID service UUID (0x1812) and other indicators
  for (uint8_t i = 0; i < adv_data_len;) {
    uint8_t length = adv_data[i];
    if (length == 0)
      break;  // Invalid length, stop parsing

    // Validate that entire AD structure is within buffer bounds
    if (i + 1 + length > adv_data_len) {
      ESP_LOGW(TAG, "Malformed advertisement data: structure exceeds buffer (offset %d, length %d, buffer size %d)", i,
               length, adv_data_len);
      break;
    }

    uint8_t type = adv_data[i + 1];

    // Check for complete local name
    if (type == ESP_BLE_AD_TYPE_NAME_CMPL && length > 1) {
      char name[32] = {0};
      memcpy(name, &adv_data[i + 2], std::min((int) (length - 1), 31));

      // Check if name contains "Xbox" or "Controller"
      if (strstr(name, "Xbox") || strstr(name, "Controller") || strstr(name, "Gamepad")) {
        is_gamepad = true;
      }
    }

    // Check for appearance (gamepad = 0x03C4)
    if (type == ESP_BLE_AD_TYPE_APPEARANCE && length == 3) {
      uint16_t appearance = (adv_data[i + 3] << 8) | adv_data[i + 2];
      if (appearance == BLE_APPEARANCE_GAMEPAD) {
        is_gamepad = true;
      }
    }

    // Check for HID service UUID
    if (type == ESP_BLE_AD_TYPE_16SRV_CMPL || type == ESP_BLE_AD_TYPE_16SRV_PART) {
      for (uint8_t j = 0; j < length - 1; j += 2) {
        uint16_t uuid = (adv_data[i + 3 + j] << 8) | adv_data[i + 2 + j];
        if (uuid == HID_SERVICE_UUID) {
          is_hid_device = true;
        }
      }
    }

    i += (length + 1);
  }

  // Accept device if it's either a HID device OR has gamepad appearance/name
  is_hid_device = is_hid_device || is_gamepad;

  if (is_hid_device) {
    // Check if already connecting (prevents multiple connection attempts from repeated advertising packets)
    if (!this->scanning_) {
      return;  // Already initiated connection
    }

    ESP_LOGI(TAG, "Found HID/gamepad device: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(scan_result.bda));

    // Stop scanning and connect
    esp_ble_gap_stop_scanning();
    this->scanning_ = false;

    // Save address and connect
    memcpy(this->remote_bda_, scan_result.bda, sizeof(esp_bd_addr_t));
    this->connect_to_device_(this->remote_bda_);
  }
}

void BLEGamepad::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                     esp_ble_gattc_cb_param_t *param) {
  // Log events at VERBOSE level to reduce noise
  ESP_LOGV(TAG, "GATT event: %d", event);

  // Filter events - only handle events for our GATT client interface
  // Exception: ESP_GATTC_REG_EVT must be processed to capture our gattc_if
  if (event != ESP_GATTC_REG_EVT && gattc_if != ESP_GATT_IF_NONE && gattc_if != this->gattc_if_) {
    ESP_LOGD(TAG, "Ignoring event %d for different gattc_if: %d (ours: %d)", event, gattc_if, this->gattc_if_);
    return;
  }

  switch (event) {
    case ESP_GATTC_REG_EVT: {
      if (param->reg.status == ESP_GATT_OK) {
        this->gattc_if_ = gattc_if;
        ESP_LOGI(TAG, "GATT client registered, app_id: %04x", param->reg.app_id);

        // Configure BLE security parameters for Xbox controller pairing
        // Based on bluepad32 implementation - bonding is required for Xbox controllers
        esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;  // No input/output (Just Works pairing)
        esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));

        uint8_t auth_req = ESP_LE_AUTH_BOND;  // Bonding required (critical for Xbox)
        esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));

        uint8_t key_size = 16;  // Max encryption key size
        esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));

        uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));

        uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

        ESP_LOGI(TAG, "BLE security configured (bonding enabled, IO cap: none)");

        // Start scanning for HID devices now that GATT client is registered
        this->start_scan_();
      } else {
        ESP_LOGE(TAG, "GATT client registration failed, status: %d", param->reg.status);
      }
      break;
    }

    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        this->conn_id_ = param->open.conn_id;
        this->connected_ = true;
        ESP_LOGI(TAG, "Connected to device: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(param->open.remote_bda));

        // Save the remote address for pairing
        memcpy(this->remote_bda_, param->open.remote_bda, sizeof(esp_bd_addr_t));

        // Update MTU
        esp_ble_gattc_send_mtu_req(gattc_if, param->open.conn_id);

        // Initiate security/pairing (required for Xbox controllers)
        // Service discovery will happen in ESP_GAP_BLE_AUTH_CMPL_EVT after pairing succeeds
        ESP_LOGI(TAG, "Initiating BLE encryption/pairing");
        esp_err_t ret = esp_ble_set_encryption(param->open.remote_bda, ESP_BLE_SEC_ENCRYPT);
        if (ret != ESP_OK) {
          ESP_LOGE(TAG, "Failed to initiate encryption: %s", esp_err_to_name(ret));
          this->disconnect_();
        }
      } else {
        ESP_LOGE(TAG, "Connection failed, status: %d", param->open.status);
        this->connected_ = false;
        // Restart scanning
        this->start_scan_();
      }
      break;
    }

    case ESP_GATTC_CLOSE_EVT: {
      ESP_LOGI(TAG, "Disconnected from device");
      this->connected_ = false;
      // Note: gattc_if_ remains valid for the app ID registration, don't reset

      if (this->active_controller_) {
        this->active_controller_->on_disconnect();
        this->active_controller_.reset();
        this->on_disconnect_callbacks_.call();
      }

      // Restart scanning
      this->start_scan_();
      break;
    }

    case ESP_GATTC_SEARCH_RES_EVT: {
      // Service discovered
      esp_gatt_id_t *srvc_id = &param->search_res.srvc_id;
      if (srvc_id->uuid.len == ESP_UUID_LEN_16) {
        uint16_t uuid = srvc_id->uuid.uuid.uuid16;

        // Check for Device Information Service (0x180A)
        if (uuid == DIS_SERVICE_UUID) {
          ESP_LOGI(TAG, "Found Device Information Service");
          this->dis_service_start_handle_ = param->search_res.start_handle;
          this->dis_service_end_handle_ = param->search_res.end_handle;
        }
        // Check for HID Service (0x1812)
        else if (uuid == HID_SERVICE_UUID) {
          ESP_LOGI(TAG, "Found HID service");
          this->hid_service_start_handle_ = param->search_res.start_handle;
          this->hid_service_end_handle_ = param->search_res.end_handle;
        }
      }
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (param->search_cmpl.status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "Service discovery failed");
        this->disconnect_();
        break;
      }

      ESP_LOGI(TAG, "Service discovery complete");

      // Check if DIS was searched for but not found (both handles still 0)
      // This happens when Xbox controller doesn't have DIS service
      if (this->dis_service_start_handle_ == 0 && this->hid_service_start_handle_ == 0) {
        if (this->service_discovery_retries_ >= MAX_DISCOVERY_RETRIES) {
          ESP_LOGE(TAG, "Service discovery failed after %d retries - no DIS or HID services found",
                   MAX_DISCOVERY_RETRIES);
          this->disconnect_();
          break;
        }
        this->service_discovery_retries_++;
        ESP_LOGI(TAG, "DIS service not found, searching for HID service (attempt %d/%d)",
                 this->service_discovery_retries_, MAX_DISCOVERY_RETRIES);
        esp_bt_uuid_t hid_uuid;
        hid_uuid.len = ESP_UUID_LEN_16;
        hid_uuid.uuid.uuid16 = HID_SERVICE_UUID;
        esp_ble_gattc_search_service(this->gattc_if_, this->conn_id_, &hid_uuid);
        break;
      }

      // Check if we just discovered DIS service
      if (this->dis_service_start_handle_ != 0 && this->hid_service_start_handle_ == 0) {
        // DIS service discovery complete - now read PnP ID
        ESP_LOGI(TAG, "DIS service discovered, searching for PnP ID characteristic");

        uint16_t char_count = 0;
        esp_gatt_status_t status = esp_ble_gattc_get_attr_count(
            gattc_if, this->conn_id_, ESP_GATT_DB_CHARACTERISTIC, this->dis_service_start_handle_,
            this->dis_service_end_handle_, ESP_GATT_INVALID_HANDLE, &char_count);

        if (status != ESP_GATT_OK || char_count == 0) {
          ESP_LOGW(TAG, "No characteristics found in DIS service, proceeding to HID discovery");
          // Skip DIS and go straight to HID
          esp_bt_uuid_t hid_uuid;
          hid_uuid.len = ESP_UUID_LEN_16;
          hid_uuid.uuid.uuid16 = HID_SERVICE_UUID;
          esp_ble_gattc_search_service(this->gattc_if_, this->conn_id_, &hid_uuid);
          break;
        }

        // Get all characteristics in DIS service
        auto char_elems = std::make_unique<esp_gattc_char_elem_t[]>(char_count);
        uint16_t actual_count = char_count;

        status = esp_ble_gattc_get_all_char(gattc_if, this->conn_id_, this->dis_service_start_handle_,
                                            this->dis_service_end_handle_, char_elems.get(), &actual_count, 0);

        if (status != ESP_GATT_OK) {
          ESP_LOGW(TAG, "Failed to get DIS characteristics, proceeding to HID discovery");
          // char_elems automatically cleaned up by unique_ptr
          // Skip DIS and go straight to HID
          esp_bt_uuid_t hid_uuid;
          hid_uuid.len = ESP_UUID_LEN_16;
          hid_uuid.uuid.uuid16 = HID_SERVICE_UUID;
          esp_ble_gattc_search_service(this->gattc_if_, this->conn_id_, &hid_uuid);
          break;
        }

        // Find PnP ID characteristic (0x2A50)
        for (uint16_t i = 0; i < actual_count; i++) {
          if (char_elems[i].uuid.len == ESP_UUID_LEN_16 && char_elems[i].uuid.uuid.uuid16 == DIS_PNP_ID_UUID) {
            ESP_LOGI(TAG, "Found PnP ID characteristic, handle: %04x", char_elems[i].char_handle);
            this->dis_pnp_id_handle_ = char_elems[i].char_handle;
            break;
          }
        }
        // char_elems automatically cleaned up by unique_ptr at end of scope

        if (this->dis_pnp_id_handle_ != 0) {
          // Read PnP ID to get VID/PID
          this->init_state_ = InitState::READING_DIS_PNPID;
          ESP_LOGI(TAG, "Reading PnP ID");
          esp_ble_gattc_read_char(gattc_if, this->conn_id_, this->dis_pnp_id_handle_, ESP_GATT_AUTH_REQ_NONE);
        } else {
          ESP_LOGW(TAG, "PnP ID characteristic not found, proceeding to HID discovery");
          // Proceed to HID service discovery
          esp_bt_uuid_t hid_uuid;
          hid_uuid.len = ESP_UUID_LEN_16;
          hid_uuid.uuid.uuid16 = HID_SERVICE_UUID;
          esp_ble_gattc_search_service(this->gattc_if_, this->conn_id_, &hid_uuid);
        }
        break;
      }

      // HID service discovery complete
      if (this->hid_service_start_handle_ == 0) {
        ESP_LOGW(TAG, "HID service not found");
        this->disconnect_();
        break;
      }

      // Get all characteristics in HID service (synchronous call)
      uint16_t char_count = 0;
      esp_gatt_status_t status = esp_ble_gattc_get_attr_count(
          gattc_if, this->conn_id_, ESP_GATT_DB_CHARACTERISTIC, this->hid_service_start_handle_,
          this->hid_service_end_handle_, ESP_GATT_INVALID_HANDLE, &char_count);

      if (status != ESP_GATT_OK || char_count == 0) {
        ESP_LOGE(TAG, "Failed to get characteristic count");
        this->disconnect_();
        break;
      }

      ESP_LOGI(TAG, "Found %d characteristics", char_count);

      // Allocate buffer for characteristics
      auto char_elems = std::make_unique<esp_gattc_char_elem_t[]>(char_count);
      uint16_t actual_count = char_count;

      status = esp_ble_gattc_get_all_char(gattc_if, this->conn_id_, this->hid_service_start_handle_,
                                          this->hid_service_end_handle_, char_elems.get(), &actual_count, 0);

      if (status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "Failed to get characteristics");
        // char_elems automatically cleaned up by unique_ptr
        this->disconnect_();
        break;
      }

      // Find HID characteristics (Info, Report Map, Protocol Mode, Reports)
      for (uint16_t i = 0; i < actual_count; i++) {
        if (char_elems[i].uuid.len == ESP_UUID_LEN_16) {
          uint16_t uuid = char_elems[i].uuid.uuid.uuid16;
          ESP_LOGD(TAG, "Characteristic %d: UUID=0x%04x, handle=0x%04x", i, uuid, char_elems[i].char_handle);

          // Find HID Information (0x2A4A) - required for HOGP
          if (uuid == HID_INFO_UUID) {
            ESP_LOGI(TAG, "Found HID Information characteristic, handle: %04x", char_elems[i].char_handle);
            this->hid_info_handle_ = char_elems[i].char_handle;
          }
          // Find HID Report Map (0x2A4B) - CRITICAL for Xbox pairing
          else if (uuid == HID_REPORT_MAP_UUID) {
            ESP_LOGI(TAG, "Found HID Report Map characteristic, handle: %04x", char_elems[i].char_handle);
            this->hid_report_map_handle_ = char_elems[i].char_handle;
          }
          // Find Protocol Mode (0x2A4E) - optional for Report-only devices
          else if (uuid == PROTOCOL_MODE_UUID) {
            ESP_LOGI(TAG, "Found Protocol Mode characteristic, handle: %04x", char_elems[i].char_handle);
            this->protocol_mode_handle_ = char_elems[i].char_handle;
          }
          // Find HID Report (0x2A4D) - input/output/feature reports
          else if (uuid == HID_REPORT_UUID) {
            ESP_LOGI(TAG, "Found HID Report characteristic, handle: %04x", char_elems[i].char_handle);

            // Store ALL HID Report characteristics (Xbox has multiple: input, output, feature)
            HIDReportCharacteristic report_char = {char_elems[i].char_handle, 0};

            // Get descriptors for this characteristic (CCC for notifications)
            uint16_t descr_count = 0;
            status = esp_ble_gattc_get_attr_count(gattc_if, this->conn_id_, ESP_GATT_DB_DESCRIPTOR,
                                                  this->hid_service_start_handle_, this->hid_service_end_handle_,
                                                  char_elems[i].char_handle, &descr_count);

            if (status == ESP_GATT_OK && descr_count > 0) {
              auto descr_elems = std::make_unique<esp_gattc_descr_elem_t[]>(descr_count);
              uint16_t actual_descr_count = descr_count;

              status = esp_ble_gattc_get_all_descr(gattc_if, this->conn_id_, char_elems[i].char_handle,
                                                   descr_elems.get(), &actual_descr_count, 0);

              if (status == ESP_GATT_OK) {
                // Find CCC descriptor
                for (uint16_t j = 0; j < actual_descr_count; j++) {
                  if (descr_elems[j].uuid.len == ESP_UUID_LEN_16 &&
                      descr_elems[j].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
                    ESP_LOGI(TAG, "Found CCC descriptor for HID Report, handle: %04x", descr_elems[j].handle);
                    report_char.ccc_handle = descr_elems[j].handle;
                    break;
                  }
                }
              }
              // descr_elems automatically cleaned up by unique_ptr at end of scope
            }

            // Store this HID Report characteristic
            this->hid_report_chars_.push_back(report_char);
            ESP_LOGI(TAG, "Stored HID Report char_handle=%04x, ccc_handle=%04x", report_char.char_handle,
                     report_char.ccc_handle);
          }
        }
      }
      // char_elems automatically cleaned up by unique_ptr at end of scope

      // Validate required characteristics were found
      if (this->hid_report_chars_.empty()) {
        ESP_LOGE(TAG, "No HID Report characteristics found");
        this->disconnect_();
        break;
      }
      ESP_LOGI(TAG, "Found %zu HID Report characteristic(s)", this->hid_report_chars_.size());

      // Check if at least one has CCC descriptor
      bool has_ccc = false;
      for (const auto &report : this->hid_report_chars_) {
        if (report.ccc_handle != 0) {
          has_ccc = true;
          break;
        }
      }
      if (!has_ccc) {
        ESP_LOGW(TAG, "No CCC descriptors found for HID Reports - notifications may not work");
      }

      // Start HOGP initialization sequence (required for Xbox controllers)
      // Order: HID Info → Report Map → Protocol Mode → Enable Notifications
      ESP_LOGI(TAG, "Starting HOGP initialization sequence");

      if (this->hid_info_handle_ != 0) {
        // Step 1: Read HID Information
        this->init_state_ = InitState::READING_HID_INFO;
        ESP_LOGI(TAG, "Reading HID Information");
        esp_ble_gattc_read_char(gattc_if, this->conn_id_, this->hid_info_handle_, ESP_GATT_AUTH_REQ_NONE);
      } else if (this->hid_report_map_handle_ != 0) {
        // Step 2: Skip to Report Map if HID Info not found
        this->init_state_ = InitState::READING_REPORT_MAP;
        ESP_LOGI(TAG, "Reading HID Report Map (HID Info not found, skipping)");
        esp_ble_gattc_read_char(gattc_if, this->conn_id_, this->hid_report_map_handle_, ESP_GATT_AUTH_REQ_NONE);
      } else {
        // Both missing - log warning and try to continue
        ESP_LOGW(TAG, "HID Info and Report Map characteristics not found - controller may not pair properly");
        this->init_state_ = InitState::SETTING_PROTOCOL_MODE;
        // Try setting protocol mode or enabling notifications
        if (this->protocol_mode_handle_ != 0) {
          ESP_LOGI(TAG, "Setting Protocol Mode to Report Mode");
          uint8_t report_mode = 0x01;
          esp_ble_gattc_write_char(gattc_if, this->conn_id_, this->protocol_mode_handle_, sizeof(report_mode),
                                   &report_mode, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
        } else {
          // Enable notifications on all HID Report characteristics
          this->enable_all_notifications_(gattc_if);
        }
      }
      break;
    }

    case ESP_GATTC_READ_CHAR_EVT: {
      // Handle characteristic read completion (DIS and HOGP initialization)
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "Read characteristic failed, handle: %04x, status: %d", param->read.handle, param->read.status);
        this->disconnect_();
        break;
      }

      // PnP ID read complete (from Device Information Service)
      if (param->read.handle == this->dis_pnp_id_handle_) {
        ESP_LOGI(TAG, "PnP ID read complete, length: %d", param->read.value_len);

        // PnP ID structure: Vendor ID Source (1 byte), Vendor ID (2 bytes LE), Product ID (2 bytes LE), Product Version
        // (2 bytes LE)
        if (param->read.value_len >= 7) {
          uint8_t vendor_id_source = param->read.value[0];
          this->vendor_id_ = (param->read.value[2] << 8) | param->read.value[1];
          this->product_id_ = (param->read.value[4] << 8) | param->read.value[3];
          uint16_t product_version = (param->read.value[6] << 8) | param->read.value[5];

          ESP_LOGI(TAG, "  Vendor ID: 0x%04x, Product ID: 0x%04x, Version: 0x%04x", this->vendor_id_, this->product_id_,
                   product_version);

          // Identify controller type
          if (this->vendor_id_ == 0x045e) {  // Microsoft
            if (this->product_id_ == 0x02e0) {
              ESP_LOGI(TAG, "  Detected: Xbox One BLE controller");
            } else if (this->product_id_ == 0x0b20) {
              ESP_LOGI(TAG, "  Detected: Xbox Series X/S controller");
            } else {
              ESP_LOGI(TAG, "  Detected: Microsoft controller (unknown model)");
            }
          }
        }

        // After reading PnP ID, proceed to HID service discovery
        ESP_LOGI(TAG, "DIS query complete, searching for HID service");
        esp_bt_uuid_t hid_uuid;
        hid_uuid.len = ESP_UUID_LEN_16;
        hid_uuid.uuid.uuid16 = HID_SERVICE_UUID;
        esp_ble_gattc_search_service(this->gattc_if_, this->conn_id_, &hid_uuid);
        break;
      }

      // HID Information read complete
      if (param->read.handle == this->hid_info_handle_) {
        ESP_LOGI(TAG, "HID Information read complete, length: %d", param->read.value_len);
        if (param->read.value_len >= 4) {
          uint16_t bcd_hid = (param->read.value[1] << 8) | param->read.value[0];
          uint8_t country_code = param->read.value[2];
          uint8_t flags = param->read.value[3];
          ESP_LOGI(TAG, "  HID version: %04x, Country: %02x, Flags: %02x", bcd_hid, country_code, flags);
        }

        // Step 2: Read HID Report Map
        if (this->hid_report_map_handle_ != 0) {
          this->init_state_ = InitState::READING_REPORT_MAP;
          ESP_LOGI(TAG, "Reading HID Report Map");
          esp_ble_gattc_read_char(gattc_if, this->conn_id_, this->hid_report_map_handle_, ESP_GATT_AUTH_REQ_NONE);
        } else {
          // Skip to Protocol Mode if Report Map not found
          this->init_state_ = InitState::SETTING_PROTOCOL_MODE;
          ESP_LOGW(TAG, "HID Report Map not found - skipping to Protocol Mode");
          if (this->protocol_mode_handle_ != 0) {
            ESP_LOGI(TAG, "Setting Protocol Mode to Report Mode");
            uint8_t report_mode = 0x01;
            esp_ble_gattc_write_char(gattc_if, this->conn_id_, this->protocol_mode_handle_, sizeof(report_mode),
                                     &report_mode, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
          } else {
            // Skip to enabling notifications
            this->enable_all_notifications_(gattc_if);
          }
        }
      }
      // HID Report Map read complete (CRITICAL for Xbox pairing)
      else if (param->read.handle == this->hid_report_map_handle_) {
        ESP_LOGI(TAG, "HID Report Map read complete, length: %d bytes", param->read.value_len);

        // Store report map for future parsing (Xbox requires this read to complete pairing)
        this->hid_report_map_.assign(param->read.value, param->read.value + param->read.value_len);
        ESP_LOGI(TAG, "Stored HID Report Map (%zu bytes)", this->hid_report_map_.size());

        // Step 3: Set Protocol Mode (if available)
        if (this->protocol_mode_handle_ != 0) {
          this->init_state_ = InitState::SETTING_PROTOCOL_MODE;
          ESP_LOGI(TAG, "Setting Protocol Mode to Report Mode");
          uint8_t report_mode = 0x01;  // Report Protocol Mode
          esp_ble_gattc_write_char(gattc_if, this->conn_id_, this->protocol_mode_handle_, sizeof(report_mode),
                                   &report_mode, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
        } else {
          // Step 4: Enable notifications (Protocol Mode not present)
          this->enable_all_notifications_(gattc_if);
        }
      }
      // Initial HID Report read complete (to "prime" controller)
      else if (this->init_state_ == InitState::READING_INITIAL_REPORT) {
        ESP_LOGI(TAG, "Initial HID Report read complete, length: %d bytes", param->read.value_len);

        // Log the report data for debugging
        if (param->read.value_len > 0) {
          ESP_LOGI(TAG, "Initial report data: %02X %02X %02X %02X...", param->read.value[0],
                   param->read.value_len > 1 ? param->read.value[1] : 0,
                   param->read.value_len > 2 ? param->read.value[2] : 0,
                   param->read.value_len > 3 ? param->read.value[3] : 0);
        }

        // HOGP initialization complete - create controller instance
        this->init_state_ = InitState::COMPLETE;
        this->service_discovery_retries_ = 0;  // Reset retry counter on successful connection

        // Xbox-only limitation: Among common console controllers, only Xbox supports standard BLE HID
        // - Xbox One S/X/Series (Model 1708+, revision 1914) use BLE HID Profile
        // - PlayStation controllers use proprietary Bluetooth protocols (not standard HID)
        // - Switch Pro controllers require reverse-engineered HID report descriptors
        // Future: Could detect controller type via VID/PID from DIS PnP ID characteristic
        this->active_controller_ = std::make_unique<XboxController>();
        if (this->active_controller_) {
          this->active_controller_->on_connect();
          this->on_connect_callbacks_.call();
          ESP_LOGI(TAG, "HOGP initialization complete - Controller ready: %s",
                   this->active_controller_->get_controller_type());
        }
      }
      break;
    }

    case ESP_GATTC_WRITE_CHAR_EVT: {
      // Protocol Mode write complete
      if (param->write.status == ESP_GATT_OK && param->write.handle == this->protocol_mode_handle_) {
        ESP_LOGI(TAG, "Protocol Mode set successfully");

        // Step 4: Enable notifications on all HID Report characteristics
        this->enable_all_notifications_(gattc_if);
      } else if (param->write.status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "Failed to set Protocol Mode, status: %d", param->write.status);
      }
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      // Notification registration complete (Bluedroid requirement)
      if (param->reg_for_notify.status == ESP_GATT_OK) {
        // Verify this is for our current characteristic
        if (this->current_notify_index_ < this->hid_report_chars_.size() &&
            param->reg_for_notify.handle == this->hid_report_chars_[this->current_notify_index_].char_handle) {
          ESP_LOGI(TAG, "Notification registration successful for handle=%04x",
                   this->hid_report_chars_[this->current_notify_index_].char_handle);

          // Now write CCC descriptor to enable notifications for this characteristic
          this->init_state_ = InitState::ENABLING_NOTIFICATIONS;
          uint16_t notify_enable = 1;
          uint16_t ccc_handle = this->hid_report_chars_[this->current_notify_index_].ccc_handle;

          ESP_LOGI(TAG, "Writing CCC descriptor (handle=%04x) to enable notifications", ccc_handle);
          esp_ble_gattc_write_char_descr(gattc_if, this->conn_id_, ccc_handle, sizeof(notify_enable),
                                         (uint8_t *) &notify_enable, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
        } else {
          ESP_LOGW(TAG, "REG_FOR_NOTIFY event for unexpected handle=%04x (current index=%zu)",
                   param->reg_for_notify.handle, this->current_notify_index_);
        }
      } else {
        ESP_LOGE(TAG, "Failed to register for notifications on handle=%04x, status: %d", param->reg_for_notify.handle,
                 param->reg_for_notify.status);
        this->disconnect_();
      }
      break;
    }

    case ESP_GATTC_WRITE_DESCR_EVT: {
      // CCC descriptor write complete for one characteristic
      if (param->write.status == ESP_GATT_OK) {
        // Check if this is one of our HID Report CCC descriptors
        bool is_hid_report_ccc = false;
        for (const auto &report : this->hid_report_chars_) {
          if (report.ccc_handle == param->write.handle) {
            is_hid_report_ccc = true;
            ESP_LOGI(TAG, "CCC write complete for HID Report handle=%04x (CCC=%04x)", report.char_handle,
                     report.ccc_handle);
            break;
          }
        }

        if (is_hid_report_ccc && this->init_state_ == InitState::ENABLING_NOTIFICATIONS) {
          // Move to next characteristic with CCC descriptor
          bool found_next = false;
          for (size_t i = this->current_notify_index_ + 1; i < this->hid_report_chars_.size(); i++) {
            if (this->hid_report_chars_[i].ccc_handle != 0) {
              // Found next characteristic with CCC - register for notifications
              this->current_notify_index_ = i;
              this->init_state_ = InitState::REGISTERING_NOTIFICATIONS;
              found_next = true;

              ESP_LOGI(TAG, "Registering for notifications: HID Report handle=%04x (next characteristic)",
                       this->hid_report_chars_[i].char_handle);

              esp_err_t err = esp_ble_gattc_register_for_notify(gattc_if, this->remote_bda_,
                                                                this->hid_report_chars_[i].char_handle);
              if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to register for notify: %s", esp_err_to_name(err));
                this->disconnect_();
              }
              break;
            }
          }

          if (!found_next) {
            // All CCC descriptors have been enabled - proceed to reading initial report
            ESP_LOGI(TAG, "All notifications enabled on %zu HID Report characteristic(s)",
                     this->hid_report_chars_.size());

            // Xbox controllers may need an initial read to "prime" them before they start sending notifications
            // Read the first input HID Report characteristic (the one with CCC descriptor)
            for (const auto &report : this->hid_report_chars_) {
              if (report.ccc_handle != 0) {
                this->init_state_ = InitState::READING_INITIAL_REPORT;
                ESP_LOGI(TAG, "Reading initial HID Report to activate controller");
                esp_ble_gattc_read_char(gattc_if, this->conn_id_, report.char_handle, ESP_GATT_AUTH_REQ_NONE);
                break;  // Only read the first input report
              }
            }
          }
        }
      } else {
        ESP_LOGE(TAG, "Failed to write CCC descriptor handle=%04x, status: %d", param->write.handle,
                 param->write.status);
        this->disconnect_();
      }
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      // Verify this notification is for our connection
      if (param->notify.conn_id != this->conn_id_) {
        ESP_LOGD(TAG, "Notification from different connection (conn_id: %d, ours: %d), ignoring", param->notify.conn_id,
                 this->conn_id_);
        break;
      }

      // HID report notification received
      ESP_LOGV(TAG, "Notification received: handle=%04x, len=%d", param->notify.handle, param->notify.value_len);

      // Check if this is from any of our HID Report characteristics
      bool is_hid_report = false;
      for (const auto &report : this->hid_report_chars_) {
        if (param->notify.handle == report.char_handle) {
          is_hid_report = true;
          ESP_LOGV(TAG, "HID Report notification (input report): handle=%04x, len=%d", report.char_handle,
                   param->notify.value_len);
          this->handle_notification_(param->notify.value, param->notify.value_len);
          break;
        }
      }

      if (!is_hid_report) {
        ESP_LOGD(TAG, "Notification from unknown handle: %04x", param->notify.handle);
      }
      break;
    }

    default:
      ESP_LOGD(TAG, "Unhandled GATT event: %d", event);
      break;
  }
}

void BLEGamepad::handle_notification_(uint8_t *value, uint16_t value_len) {
  if (active_controller_ == nullptr) {
    return;
  }

  // Delegate parsing to controller-specific implementation
  if (!active_controller_->parse_input_report(value, value_len)) {
    ESP_LOGW(TAG, "Failed to parse input report (length: %d)", value_len);
  }
}

void BLEGamepad::connect_to_device_(esp_bd_addr_t bda) {
  ESP_LOGI(TAG, "Connecting to device: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(bda));
  esp_ble_gattc_open(gattc_if_, bda, BLE_ADDR_TYPE_PUBLIC, true);
}

void BLEGamepad::disconnect_() {
  if (connected_) {
    esp_err_t ret = esp_ble_gattc_close(gattc_if_, conn_id_);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to close GATT connection: %s", esp_err_to_name(ret));
      // Force cleanup even if close fails to prevent stuck state
      this->connected_ = false;
      if (this->active_controller_) {
        this->active_controller_->on_disconnect();
        this->active_controller_.reset();
      }
      this->on_disconnect_trigger_->trigger();
    }
  }
}

void BLEGamepad::enable_all_notifications_(esp_gatt_if_t gattc_if) {
  // Bluedroid requires calling esp_ble_gattc_register_for_notify() BEFORE writing CCC
  // Without this, Bluedroid won't dispatch notification events to our handler
  this->init_state_ = InitState::REGISTERING_NOTIFICATIONS;
  this->current_notify_index_ = 0;
  ESP_LOGI(TAG, "Registering for notifications on %zu HID Report characteristic(s)", this->hid_report_chars_.size());

  // Find first characteristic with CCC descriptor and register for notifications
  for (size_t i = 0; i < this->hid_report_chars_.size(); i++) {
    if (this->hid_report_chars_[i].ccc_handle != 0) {
      this->current_notify_index_ = i;
      ESP_LOGI(TAG, "Registering for notifications: HID Report handle=%04x", this->hid_report_chars_[i].char_handle);

      esp_err_t err =
          esp_ble_gattc_register_for_notify(gattc_if, this->remote_bda_, this->hid_report_chars_[i].char_handle);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register for notify: %s", esp_err_to_name(err));
        this->disconnect_();
      }
      return;  // Wait for ESP_GATTC_REG_FOR_NOTIFY_EVT before proceeding
    }
  }

  // No characteristics with CCC found
  ESP_LOGE(TAG, "No HID Report characteristics with CCC descriptors found");
  this->disconnect_();
}

}  // namespace esphome::ble_gamepad

#endif  // USE_ESP_IDF
