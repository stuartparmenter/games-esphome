// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#include "lvgl_game_runner.h"

#include "esp_timer.h"
#include <algorithm>
#include <cmath>

namespace esphome::lvgl_game_runner {

static const char *const TAG = "lvgl_game_runner";

void LvglGameRunner::set_fps(float fps) {
  if (fps < 1.0f)
    fps = 1.0f;
  if (fps > 240.0f)
    fps = 240.0f;
  period_ms_ = (uint32_t) lroundf(1000.0f / fps);
}

void LvglGameRunner::pause() {
  if (game_)
    game_->pause();
  running_ = false;
  this->disable_loop();
}

void LvglGameRunner::resume() {
  if (game_)
    game_->resume();
  running_ = true;
  rebind_ = true;
  last_us_ = esp_timer_get_time();  // Resync timing
  this->enable_loop();
}

void LvglGameRunner::start() {
  if (game_)
    game_->reset();
  if (!running_)
    resume();
}

void LvglGameRunner::send_input(InputType type, bool pressed, int16_t value) {
  input_handler_.push_event(InputEvent(type, pressed, value));
}

void LvglGameRunner::send_input(const char *input_str, bool pressed, int16_t value) {
  // Map string to InputType (same logic as SendInputAction)
  InputType type = InputType::UP;
  if (strcmp(input_str, "UP") == 0)
    type = InputType::UP;
  else if (strcmp(input_str, "DOWN") == 0)
    type = InputType::DOWN;
  else if (strcmp(input_str, "LEFT") == 0)
    type = InputType::LEFT;
  else if (strcmp(input_str, "RIGHT") == 0)
    type = InputType::RIGHT;
  else if (strcmp(input_str, "A") == 0)
    type = InputType::A;
  else if (strcmp(input_str, "B") == 0)
    type = InputType::B;
  else if (strcmp(input_str, "SELECT") == 0)
    type = InputType::SELECT;
  else if (strcmp(input_str, "START") == 0)
    type = InputType::START;
  else if (strcmp(input_str, "L_TRIGGER") == 0)
    type = InputType::L_TRIGGER;
  else if (strcmp(input_str, "R_TRIGGER") == 0)
    type = InputType::R_TRIGGER;
  else if (strcmp(input_str, "ROTATE_CW") == 0)
    type = InputType::ROTATE_CW;
  else if (strcmp(input_str, "ROTATE_CCW") == 0)
    type = InputType::ROTATE_CCW;
  else if (strcmp(input_str, "TOUCH") == 0)
    type = InputType::TOUCH;
  else {
    ESP_LOGW(TAG, "Unknown input type: %s", input_str);
    return;
  }

  send_input(type, pressed, value);
}

void LvglGameRunner::send_input_event(const InputEvent &event) { input_handler_.push_event(event); }

void LvglGameRunner::setup_binding(lv_obj_t *canvas_obj, const std::string &game_key, int x, int y, int w, int h,
                                   bool start_paused) {
  canvas_ = canvas_obj;
  game_key_ = game_key;
  area_.x = x;
  area_.y = y;
  area_.w = w;
  area_.h = h;
  running_ = !start_paused;
  rebind_ = running_;
}

void LvglGameRunner::setup() {
  last_us_ = esp_timer_get_time();
#if LVGL_GAME_RUNNER_METRICS
  m_.window_start_us = last_us_;
  m_.last_tick_us = last_us_;
#endif

  // If starting paused, disable loop to save power
  if (!running_) {
    this->disable_loop();
  }
}

void LvglGameRunner::loop() {
  if (!running_)
    return;

  const uint64_t now = esp_timer_get_time();
  const uint64_t elapsed_us = now - last_us_;
  const uint64_t target_us = (uint64_t) period_ms_ * 1000;

  if (elapsed_us >= target_us) {
    // Calculate dt from MEASURED elapsed time (for graceful degradation)
    const float dt = std::min((float) elapsed_us / 1e6f, 0.1f);  // cap at 100ms

    // Update last_us_ BEFORE tick
    last_us_ = now;

    // Pass measured dt to tick
    this->tick_(dt);
  }
}

void LvglGameRunner::set_game(const std::string &key) {
  if (key == game_key_)
    return;
  game_key_ = key;
  game_ = nullptr;         // clear old instance reference
  rebind_ = true;          // ensure ensure_bound_() runs next update
  input_handler_.clear();  // clear any pending input
  ESP_LOGI(TAG, "Game changed to '%s'; will rebind", game_key_.c_str());
}

bool LvglGameRunner::read_canvas_size_(uint16_t &w, uint16_t &h) {
  if (!canvas_ || !lv_obj_is_valid(canvas_))
    return false;
  lv_obj_update_layout(canvas_);
  w = (uint16_t) lv_obj_get_width(canvas_);
  h = (uint16_t) lv_obj_get_height(canvas_);
  return (w > 0 && h > 0);
}

void LvglGameRunner::on_canvas_size_change_() {
  if (!game_)
    return;
  uint16_t cw{0}, ch{0};
  if (!this->read_canvas_size_(cw, ch))
    return;
  const int w = (area_.w > 0) ? area_.w : (int) cw;
  const int h = (area_.h > 0) ? area_.h : (int) ch;
  game_->on_resize(GameBase::Rect{area_.x, area_.y, w, h});
}

bool LvglGameRunner::ensure_bound_() {
  if (!canvas_ || !lv_obj_is_valid(canvas_))
    return false;

  const lv_img_dsc_t *img = (const lv_img_dsc_t *) lv_canvas_get_img(canvas_);
  if (!img || !img->data) {
    ESP_LOGW(TAG, "Canvas image not ready yet; will retry");
    return false;
  }

  if (!game_) {
    game_ = GameRegistry::make(game_key_);
    if (!game_) {
      ESP_LOGE(TAG, "Game '%s' not found", game_key_.c_str());
      return false;
    }
    game_->on_bind(canvas_);
    game_->reset();  // Initialize game state
  }
  this->on_canvas_size_change_();
  return true;
}

void LvglGameRunner::process_input_() {
  if (!game_)
    return;

  InputEvent event;
  while (input_handler_.pop_event(event)) {
    game_->on_input(event);
  }
}

void LvglGameRunner::tick_(float dt) {
  uint16_t cw{0}, ch{0};
  const bool have_size = this->read_canvas_size_(cw, ch);

  if (rebind_) {
    if (!this->ensure_bound_())
      return;
    rebind_ = false;
    last_w_ = cw;
    last_h_ = ch;
  } else if (have_size && (cw != last_w_ || ch != last_h_)) {
    last_w_ = cw;
    last_h_ = ch;
    this->on_canvas_size_change_();
  }

  if (!game_)
    return;

  // Process input events first
  this->process_input_();

  // Measure the game step() duration
#if LVGL_GAME_RUNNER_METRICS
  const uint64_t t0 = esp_timer_get_time();
#endif

  game_->step(dt);

#if LVGL_GAME_RUNNER_METRICS
  const uint64_t t1 = esp_timer_get_time();
  const uint32_t step_us = static_cast<uint32_t>(t1 - t0);

  // Loop interval (tick-to-tick), using end-of-previous update
  const uint32_t loop_us = static_cast<uint32_t>(t1 - m_.last_tick_us);
  m_.last_tick_us = t1;

  // Aggregate window stats
  m_.frames++;
  m_.step_us_sum += step_us;
  m_.loop_us_sum += loop_us;
  m_.step_us_max = std::max(m_.step_us_max, step_us);
  m_.loop_us_max = std::max(m_.loop_us_max, loop_us);
  if (step_us / 1000.0f > static_cast<float>(period_ms_))
    m_.overruns++;

  // Periodic log/roll
  if (t1 - m_.window_start_us >= (uint64_t) METRICS_PERIOD_MS * 1000ULL) {
    metrics_log_and_roll_(t1);
  }
#endif
}

void LvglGameRunner::dump_config() {
  uint16_t cw = 0, ch = 0;
  read_canvas_size_(cw, ch);
  ESP_LOGCONFIG(TAG, "LvglGameRunner(%p): game='%s' area=[%d,%d %dx%d] canvas=%ux%u period=%ums running=%s", this,
                game_key_.c_str(), area_.x, area_.y, area_.w, area_.h, cw, ch, period_ms_, running_ ? "true" : "false");
#if LVGL_GAME_RUNNER_METRICS
  ESP_LOGCONFIG(TAG, "Metrics: enabled (period=%ums)", METRICS_PERIOD_MS);
#else
  ESP_LOGCONFIG(TAG, "Metrics: disabled (LVGL_GAME_RUNNER_METRICS=0)");
#endif
}

#if LVGL_GAME_RUNNER_METRICS
void LvglGameRunner::metrics_log_and_roll_(uint64_t now_us) {
  if (m_.frames == 0) {
    m_.window_start_us = now_us;
    return;
  }
  const double win_s = (now_us - m_.window_start_us) / 1e6;
  const double target_fps = (period_ms_ > 0) ? (1000.0 / period_ms_) : 0.0;
  const double effective_fps = m_.frames / win_s;
  const double avg_step_ms = (m_.step_us_sum / 1000.0) / m_.frames;
  const double avg_loop_ms = (m_.loop_us_sum / 1000.0) / m_.frames;

  ESP_LOGD(TAG,
           "[metrics] eff=%.2ffps tgt=%.2ffps frames=%u "
           "step(avg/max)=%.3f/%.3f ms loop(avg/max)=%.3f/%.3f ms overruns=%u",
           effective_fps, target_fps, m_.frames, avg_step_ms, m_.step_us_max / 1000.0, avg_loop_ms,
           m_.loop_us_max / 1000.0, m_.overruns);

  // Roll the window
  m_.window_start_us = now_us;
  m_.frames = 0;
  m_.step_us_sum = 0;
  m_.step_us_max = 0;
  m_.loop_us_sum = 0;
  m_.loop_us_max = 0;
  m_.overruns = 0;
}
#endif

}  // namespace esphome::lvgl_game_runner
