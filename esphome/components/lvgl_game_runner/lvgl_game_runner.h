// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

#include <memory>
#include <string>

#include "game_base.h"
#include "game_registry.h"
#include "input_handler.h"

extern "C" {
#include <lvgl.h>
}

#ifndef LVGL_GAME_RUNNER_METRICS
#define LVGL_GAME_RUNNER_METRICS 1
#endif

namespace esphome::lvgl_game_runner {

class LvglGameRunner : public Component {
 public:
  // Lifecycle
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }

  // Codegen wiring
  void setup_binding(lv_obj_t *canvas_obj, GameBase *initial_game, int x, int y, int w, int h, bool start_paused);

  // Runtime control
  void start();  // Start/restart current game
  void pause();
  void resume();
  void toggle() { running_ ? pause() : resume(); }
  bool is_running() const { return running_; }

  // Input handling
  void send_input(InputType type, bool pressed = true, int16_t value = 0);
  void send_input(const char *input_str, bool pressed = true, int16_t value = 0);  // String overload for YAML
  void send_input_event(const InputEvent &event);

  // Per-instance timing
  void set_initial_period(uint32_t ms) { period_ms_ = ms; }
  void set_fps(float fps);
  void set_game(GameBase *game);

 protected:
  struct Area {
    int x{0}, y{0}, w{0}, h{0};
  };

  bool ensure_bound_();
  bool read_canvas_size_(uint16_t &w, uint16_t &h);
  void on_canvas_size_change_();
  void tick_(float dt);   // Execute one frame update
  void process_input_();  // Process queued input events

  // Bound canvas & game
  lv_obj_t *canvas_{nullptr};
  std::string game_key_;
  GameBase *game_{nullptr};  // Raw pointer - game lifetime managed by components
  InputHandler input_handler_;

  // Sub-rectangle (w/h == 0 => follow canvas size)
  Area area_{};

  // State
  bool running_{true};
  bool rebind_{false};
  uint16_t last_w_{0}, last_h_{0};

  // Timing
  uint32_t period_ms_{33};  // ~30 FPS default
  uint64_t last_us_{0};

#if LVGL_GAME_RUNNER_METRICS
  // ---- Metrics window (printed every ~5s) ----
  struct {
    uint64_t window_start_us{0};
    uint64_t last_tick_us{0};
    uint32_t frames{0};
    uint64_t step_us_sum{0};
    uint32_t step_us_max{0};
    uint64_t loop_us_sum{0};
    uint32_t loop_us_max{0};
    uint32_t overruns{0};
  } m_{};

  static constexpr uint32_t METRICS_PERIOD_MS = 5000;
  void metrics_log_and_roll_(uint64_t now_us);
#endif
};

// -------- Automation actions (per-instance) --------
class StartAction : public Action<>, public Parented<LvglGameRunner> {
 public:
  void play() override { this->parent_->start(); }
};

class PauseAction : public Action<>, public Parented<LvglGameRunner> {
 public:
  void play() override { this->parent_->pause(); }
};

class ResumeAction : public Action<>, public Parented<LvglGameRunner> {
 public:
  void play() override { this->parent_->resume(); }
  LvglGameRunner *t_{nullptr};
};

class ToggleAction : public Action<>, public Parented<LvglGameRunner> {
 public:
  void play() override { this->parent_->toggle(); }
};

template<typename... Ts> class SetFpsAction : public Action<Ts...>, public Parented<LvglGameRunner> {
 public:
  TEMPLATABLE_VALUE(float, fps);
  void play(const Ts &...x) override { this->parent_->set_fps(this->fps_.value(x...)); }
};

template<typename... Ts> class SetGameAction : public Action<Ts...>, public Parented<LvglGameRunner> {
 public:
  TEMPLATABLE_VALUE(GameBase *, game);

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 11, 0)
  void play(const Ts &...x) override {
#else
  void play(Ts... x) override {
#endif
    GameBase *game = this->game_.value(x...);
    if (game != nullptr)
      this->parent_->set_game(game);
  }
};

template<typename... Ts> class SendInputAction : public Action<Ts...>, public Parented<LvglGameRunner> {
 public:
  TEMPLATABLE_VALUE(std::string, input_type);

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 11, 0)
  void play(const Ts &...x) override {
#else
  void play(Ts... x) override {
#endif
    const std::string input_str = this->input_type_.value(x...);
    if (input_str.empty())
      return;

    // Map string to InputType
    InputType type = InputType::UP;
    if (input_str == "UP")
      type = InputType::UP;
    else if (input_str == "DOWN")
      type = InputType::DOWN;
    else if (input_str == "LEFT")
      type = InputType::LEFT;
    else if (input_str == "RIGHT")
      type = InputType::RIGHT;
    else if (input_str == "A")
      type = InputType::A;
    else if (input_str == "B")
      type = InputType::B;
    else if (input_str == "SELECT")
      type = InputType::SELECT;
    else if (input_str == "START")
      type = InputType::START;
    else if (input_str == "L_TRIGGER")
      type = InputType::L_TRIGGER;
    else if (input_str == "R_TRIGGER")
      type = InputType::R_TRIGGER;
    else if (input_str == "ROTATE_CW")
      type = InputType::ROTATE_CW;
    else if (input_str == "ROTATE_CCW")
      type = InputType::ROTATE_CCW;
    else if (input_str == "TOUCH")
      type = InputType::TOUCH;
    else {
      ESP_LOGW("lvgl_game_runner", "Unknown input type: %s", input_str.c_str());
      return;
    }

    this->parent_->send_input(type);
  }
};

}  // namespace esphome::lvgl_game_runner
