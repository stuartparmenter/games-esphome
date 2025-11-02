// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#pragma once

#include "input_types.h"
#include "esphome/core/component.h"
#include <queue>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace esphome::lvgl_game_runner {

/**
 * Thread-safe input event queue.
 * Similar to the data ingress pattern in FxAudioSpectrum.
 *
 * Input events can come from multiple sources (button ISRs, encoder callbacks, etc.)
 * and need to be safely queued for the game loop to process.
 */
class InputHandler {
 public:
  InputHandler();
  ~InputHandler();

  /**
   * Push an input event to the queue.
   * Thread-safe, can be called from ISRs or other threads.
   */
  void push_event(const InputEvent &event);

  /**
   * Pop the next input event from the queue.
   * Returns false if queue is empty.
   * Should be called from the main game loop thread.
   */
  bool pop_event(InputEvent &event);

  /**
   * Check if there are events in the queue.
   */
  bool has_events();

  /**
   * Clear all events from the queue.
   */
  void clear();

 private:
  std::queue<InputEvent> queue_;
  SemaphoreHandle_t mutex_{nullptr};
  static constexpr size_t MAX_QUEUE_SIZE = 32;  // Prevent overflow
};

}  // namespace esphome::lvgl_game_runner
