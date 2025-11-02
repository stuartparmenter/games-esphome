// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#include "input_handler.h"
#include "esphome/core/log.h"

namespace esphome::lvgl_game_runner {

static const char *const TAG = "lvgl_game_runner.input";

InputHandler::InputHandler() {
  this->mutex_ = xSemaphoreCreateMutex();
  if (!this->mutex_) {
    ESP_LOGE(TAG, "Failed to create input mutex");
  }
}

InputHandler::~InputHandler() {
  if (this->mutex_) {
    vSemaphoreDelete(this->mutex_);
    this->mutex_ = nullptr;
  }
}

void InputHandler::push_event(const InputEvent &event) {
  if (!this->mutex_)
    return;

  if (xSemaphoreTake(this->mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
    // Prevent queue overflow
    if (this->queue_.size() < MAX_QUEUE_SIZE) {
      this->queue_.push(event);
    } else {
      ESP_LOGW(TAG, "Input queue full, dropping event");
    }
    xSemaphoreGive(this->mutex_);
  } else {
    ESP_LOGW(TAG, "Failed to acquire mutex for push_event");
  }
}

bool InputHandler::pop_event(InputEvent &event) {
  if (!this->mutex_)
    return false;

  bool has_event = false;
  if (xSemaphoreTake(this->mutex_, 0) == pdTRUE) {
    if (!this->queue_.empty()) {
      event = this->queue_.front();
      this->queue_.pop();
      has_event = true;
    }
    xSemaphoreGive(this->mutex_);
  }
  return has_event;
}

bool InputHandler::has_events() {
  if (!this->mutex_)
    return false;

  bool has = false;
  if (xSemaphoreTake(this->mutex_, 0) == pdTRUE) {
    has = !this->queue_.empty();
    xSemaphoreGive(this->mutex_);
  }
  return has;
}

void InputHandler::clear() {
  if (!this->mutex_)
    return;

  if (xSemaphoreTake(this->mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
    while (!this->queue_.empty()) {
      this->queue_.pop();
    }
    xSemaphoreGive(this->mutex_);
  }
}

}  // namespace esphome::lvgl_game_runner
