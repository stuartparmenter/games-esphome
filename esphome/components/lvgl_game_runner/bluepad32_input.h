// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#pragma once

#ifdef USE_BLUEPAD32

#include "input_handler.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C" {
#include "uni.h"
#include "uni_hid_device.h"
#include "uni_platform.h"
}

namespace esphome::lvgl_game_runner {

/**
 * @brief Bridges Bluepad32 Bluetooth gamepad library to InputHandler.
 *
 * Spawns a dedicated FreeRTOS task on CPU0 to run the BTstack event loop.
 * Implements custom Bluepad32 platform callbacks to map gamepad events
 * to the game runner's InputHandler queue.
 *
 * Thread Safety:
 * - Bluepad32 callbacks execute on CPU0 (BTstack task)
 * - InputHandler::queue_input() is thread-safe (uses semaphore)
 * - ESPHome main loop runs on CPU1
 */
class Bluepad32Input {
 public:
  /**
   * @brief Start Bluepad32 task and link to InputHandler.
   * @param handler Pointer to InputHandler for queueing input events
   */
  void start(InputHandler *handler);

  /**
   * @brief Process controller data from Bluepad32 callback.
   * @param d HID device structure
   * @param ctl Controller data (gamepad, mouse, keyboard, etc.)
   *
   * Called from BTstack thread (CPU0). Maps gamepad inputs to InputHandler.
   */
  void on_controller_data(uni_hid_device_t *d, uni_controller_t *ctl);

 private:
  InputHandler *input_handler_{nullptr};
  TaskHandle_t task_handle_{nullptr};

  // Platform callbacks (must be static for C API)
  static void platform_init();
  static void platform_on_init_complete();
  static void platform_on_device_discovered(uni_hid_device_t *d);
  static void platform_on_device_connected(uni_hid_device_t *d);
  static void platform_on_device_disconnected(uni_hid_device_t *d);
  static void platform_on_device_ready(uni_hid_device_t *d);
  static void platform_on_controller_data(uni_hid_device_t *d, uni_controller_t *ctl);
  static void platform_on_oob_event(uni_platform_oob_event_t event, void *data);
  static uni_property_t *platform_get_property(uni_property_idx_t key);

  // Singleton instance for C callback bridge
  static Bluepad32Input *instance_;
};

/**
 * @brief Get platform struct for Bluepad32 registration.
 * @return Pointer to uni_platform structure with all callbacks
 *
 * Called from bluepad32_task during initialization.
 */
extern "C" struct uni_platform *get_bluepad32_platform();

}  // namespace esphome::lvgl_game_runner

#endif  // USE_BLUEPAD32
