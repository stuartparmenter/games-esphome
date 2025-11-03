// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#ifdef USE_BLUEPAD32

#include "bluepad32_input.h"
#include "esphome/core/log.h"

namespace esphome::lvgl_game_runner {

static const char *TAG = "lvgl_game_runner.bluepad32";

// Analog stick deadzone threshold (~50% deflection, range is -512 to 511)
static constexpr int32_t STICK_THRESHOLD = 256;

Bluepad32Input *Bluepad32Input::instance_ = nullptr;

/**
 * @brief FreeRTOS task that runs BTstack event loop.
 *
 * This task is pinned to CPU0 and runs the blocking BTstack event loop.
 * It initializes BTstack, registers the custom platform, and starts
 * Bluepad32. Once btstack_run_loop_execute() is called, it never returns.
 */
static void bluepad32_task(void *arg) {
  auto *bp32 = static_cast<Bluepad32Input *>(arg);

  ESP_LOGI(TAG, "Initializing Bluepad32 on CPU%d", xPortGetCoreID());

  // Critical initialization sequence
  // 1. Initialize BTstack for ESP32 VHCI controller
  btstack_init();

  // 2. Register custom platform (MUST be before uni_init)
  uni_platform_set_custom(get_bluepad32_platform());

  // 3. Initialize Bluepad32
  uni_init(0, nullptr);

  ESP_LOGI(TAG, "Starting BTstack event loop (this call never returns)");

  // 4. Run event loop - blocking call that processes Bluetooth events
  btstack_run_loop_execute();

  // Should never reach here
  ESP_LOGE(TAG, "BTstack event loop exited unexpectedly!");
  vTaskDelete(nullptr);
}

void Bluepad32Input::start(InputHandler *handler) {
  if (!handler) {
    ESP_LOGE(TAG, "InputHandler is null, cannot start Bluepad32");
    return;
  }

  input_handler_ = handler;
  instance_ = this;

  // Create task pinned to CPU0
  BaseType_t result = xTaskCreatePinnedToCore(bluepad32_task,
                                              "bluepad32",  // Task name
                                              8192,         // Stack size (8KB)
                                              this,         // Task parameter
                                              5,            // Priority (medium)
                                              &task_handle_,
                                              0  // Pin to CPU0
  );

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create Bluepad32 task!");
    instance_ = nullptr;
  } else {
    ESP_LOGI(TAG, "Bluepad32 task created successfully on CPU0");
  }
}

// ===== Platform Callback Implementations =====

void Bluepad32Input::platform_init() {
  ESP_LOGI(TAG, "Platform initialized");
  // Could set custom gamepad mappings here if needed
}

void Bluepad32Input::platform_on_init_complete() {
  ESP_LOGI(TAG, "Bluetooth ready, enabling new connections");
  // Start accepting Bluetooth connections
  uni_bt_enable_new_connections_safe(true);
}

void Bluepad32Input::platform_on_device_discovered(uni_hid_device_t *d) {
  // Accept all devices by default
  // Could filter by Class of Device (COD) here if needed
  // Example: Reject keyboards
  // if ((d->cod & UNI_BT_COD_MINOR_MASK) == UNI_BT_COD_MINOR_KEYBOARD) {
  //     ESP_LOGI(TAG, "Rejecting keyboard device");
  //     return;
  // }
  ESP_LOGD(TAG, "Device discovered");
}

void Bluepad32Input::platform_on_device_connected(uni_hid_device_t *d) {
  ESP_LOGI(TAG, "Device connected (not yet ready)");
}

void Bluepad32Input::platform_on_device_disconnected(uni_hid_device_t *d) { ESP_LOGI(TAG, "Device disconnected"); }

void Bluepad32Input::platform_on_device_ready(uni_hid_device_t *d) {
  const char *name = uni_hid_device_get_name(d);
  ESP_LOGI(TAG, "Device ready: %s", name ? name : "Unknown");

  // Assign device to gamepad seat A (single player)
  uni_hid_device_set_gamepad_seat(d, GAMEPAD_SEAT_A);

  // Could trigger rumble/LED feedback here
  // uni_hid_device_set_rumble(d, 0, 250, 128, 0);  // Welcome rumble
}

void Bluepad32Input::platform_on_controller_data(uni_hid_device_t *d, uni_controller_t *ctl) {
  if (instance_) {
    instance_->on_controller_data(d, ctl);
  }
}

void Bluepad32Input::on_controller_data(uni_hid_device_t *d, uni_controller_t *ctl) {
  if (!input_handler_) {
    return;
  }

  // Only handle gamepad input (not mouse, keyboard, or balance board)
  if (ctl->klass != UNI_CONTROLLER_CLASS_GAMEPAD) {
    return;
  }

  const uni_gamepad_t *gp = &ctl->gamepad;

  // Map D-pad to directional inputs
  // D-pad uses bit flags: DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT
  bool dpad_up = (gp->dpad & DPAD_UP) != 0;
  bool dpad_down = (gp->dpad & DPAD_DOWN) != 0;
  bool dpad_left = (gp->dpad & DPAD_LEFT) != 0;
  bool dpad_right = (gp->dpad & DPAD_RIGHT) != 0;

  if (dpad_up) {
    input_handler_->queue_input(InputType::UP, true);
  }
  if (dpad_down) {
    input_handler_->queue_input(InputType::DOWN, true);
  }
  if (dpad_left) {
    input_handler_->queue_input(InputType::LEFT, true);
  }
  if (dpad_right) {
    input_handler_->queue_input(InputType::RIGHT, true);
  }

  // Map face buttons
  // buttons is a uint16_t with bit flags
  bool button_a = (gp->buttons & BUTTON_A) != 0;
  bool button_b = (gp->buttons & BUTTON_B) != 0;

  if (button_a) {
    input_handler_->queue_input(InputType::BUTTON_A, true);
  }
  if (button_b) {
    input_handler_->queue_input(InputType::BUTTON_B, true);
  }

  // Map left analog stick to directional input (with deadzone)
  // axis_x/axis_y range: -512 to 511
  // Negative Y = up, positive Y = down
  // Negative X = left, positive X = right
  bool stick_up = gp->axis_y < -STICK_THRESHOLD;
  bool stick_down = gp->axis_y > STICK_THRESHOLD;
  bool stick_left = gp->axis_x < -STICK_THRESHOLD;
  bool stick_right = gp->axis_x > STICK_THRESHOLD;

  if (stick_up && !dpad_up) {  // Don't double-trigger with d-pad
    input_handler_->queue_input(InputType::UP, true);
  }
  if (stick_down && !dpad_down) {
    input_handler_->queue_input(InputType::DOWN, true);
  }
  if (stick_left && !dpad_left) {
    input_handler_->queue_input(InputType::LEFT, true);
  }
  if (stick_right && !dpad_right) {
    input_handler_->queue_input(InputType::RIGHT, true);
  }

  // Optional: Map shoulder buttons or triggers
  // bool button_l1 = (gp->buttons & BUTTON_SHOULDER_L) != 0;
  // bool button_r1 = (gp->buttons & BUTTON_SHOULDER_R) != 0;
  // bool trigger_l2 = gp->brake > 512;  // L2 trigger (range 0-1023)
  // bool trigger_r2 = gp->throttle > 512;  // R2 trigger (range 0-1023)

  // Optional: Map misc buttons (Start, Select, Home)
  // bool button_start = (gp->misc_buttons & MISC_BUTTON_START) != 0;
  // bool button_select = (gp->misc_buttons & MISC_BUTTON_SELECT) != 0;
  // bool button_home = (gp->misc_buttons & MISC_BUTTON_SYSTEM) != 0;

  // Note: Motion sensors available if supported by controller
  // gp->gyro[3] - gyroscope data (degrees/second)
  // gp->accel[3] - accelerometer data (G's)

  // Note: Battery level available
  // ctl->battery - 0=empty, 254=full, 255=unavailable
}

void Bluepad32Input::platform_on_oob_event(uni_platform_oob_event_t event, void *data) {
  // Handle out-of-band events
  switch (event) {
    case UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON:
      ESP_LOGI(TAG, "System button pressed");
      // Could pause game or trigger menu
      break;

    case UNI_PLATFORM_OOB_BLUETOOTH_ENABLED:
      ESP_LOGI(TAG, "Bluetooth enabled");
      break;

    default:
      ESP_LOGD(TAG, "OOB event: %d", event);
      break;
  }
}

uni_property_t *Bluepad32Input::platform_get_property(uni_property_idx_t key) {
  // Return NULL for all properties to use Bluepad32 defaults
  // Could customize behavior here:
  // - UNI_PROPERTY_IDX_GAMEPAD_MAPPINGS - Custom button mappings
  // - UNI_PROPERTY_IDX_DELETE_STORED_KEYS - Auto-delete paired devices
  // - etc.
  return nullptr;
}

// ===== Platform Struct Registration =====

/**
 * @brief Platform structure with all callback pointers.
 *
 * This struct is registered with Bluepad32 during initialization.
 * All callbacks are static methods that delegate to the singleton instance.
 */
static struct uni_platform bluepad32_platform = {
    .name = "ESPHome Game Runner",
    .init = Bluepad32Input::platform_init,
    .on_init_complete = Bluepad32Input::platform_on_init_complete,
    .on_device_discovered = Bluepad32Input::platform_on_device_discovered,
    .on_device_connected = Bluepad32Input::platform_on_device_connected,
    .on_device_disconnected = Bluepad32Input::platform_on_device_disconnected,
    .on_device_ready = Bluepad32Input::platform_on_device_ready,
    .on_controller_data = Bluepad32Input::platform_on_controller_data,
    .on_oob_event = Bluepad32Input::platform_on_oob_event,
    .get_property = Bluepad32Input::platform_get_property,
    .on_gamepad_data = nullptr,        // Deprecated, use on_controller_data
    .device_dump = nullptr,            // Optional debug function
    .register_console_cmds = nullptr,  // Optional console commands
};

extern "C" struct uni_platform *get_bluepad32_platform() { return &bluepad32_platform; }

}  // namespace esphome::lvgl_game_runner

#endif  // USE_BLUEPAD32
