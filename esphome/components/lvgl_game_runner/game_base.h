// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#pragma once

#include <lvgl.h>
#include "esphome/core/component.h"
#include "input_types.h"

namespace esphome::lvgl_game_runner {

/**
 * Base class for all games in the runner.
 * Similar to FxBase in lvgl-canvas-fx but with game-specific methods.
 */
class GameBase {
 public:
  virtual ~GameBase() = default;

  /**
   * Rectangular area for rendering (supports sub-regions like canvas-fx).
   */
  struct Rect {
    int x{0};
    int y{0};
    int w{0};
    int h{0};
  };

  /**
   * Called once when the canvas buffer is ready.
   * Use this for one-time initialization (palettes, lookup tables, etc.).
   */
  virtual void on_bind(lv_obj_t *canvas) { canvas_ = canvas; }

  /**
   * Called when canvas size changes or when sub-region is set.
   * Games should reallocate buffers and recompute parameters as needed.
   */
  virtual void on_resize(const Rect &r) { area_ = r; }

  /**
   * Called each frame with the elapsed time since last step.
   * dt = measured elapsed time in seconds (capped at 0.1s for stability).
   * This is where game logic and rendering happens.
   */
  virtual void step(float dt) = 0;

  /**
   * Called when input events are received.
   * Games should update their state based on input.
   */
  virtual void on_input(const InputEvent &event) {}

  /**
   * Reset game state to initial conditions.
   * Called when game is restarted or switched.
   */
  virtual void reset() {}

  /**
   * Pause the game (stop updating state but preserve it).
   */
  virtual void pause() { paused_ = true; }

  /**
   * Resume the game.
   */
  virtual void resume() { paused_ = false; }

  /**
   * Check if game is paused.
   */
  bool is_paused() const { return paused_; }

  /**
   * Optional: Called when a sound event should be triggered.
   * Games can emit sound events for external handling.
   * Not implemented yet, but framework is ready.
   */
  enum class SoundEvent {
    NONE,
    JUMP,
    COIN,
    HIT,
    GAME_OVER,
    LEVEL_UP,
  };
  virtual void on_sound_event(SoundEvent event) {
    // No-op by default, can be overridden for sound integration
    (void) event;
  }

 protected:
  lv_obj_t *canvas_{nullptr};  // LVGL canvas object
  Rect area_{};                // Rendering area
  bool paused_{false};         // Pause state

  /**
   * Helper to get canvas buffer for direct pixel manipulation.
   * Returns nullptr if canvas is not ready.
   */
  lv_color_t *get_canvas_buffer() {
    if (!canvas_)
      return nullptr;
    const lv_img_dsc_t *img = static_cast<const lv_img_dsc_t *>(lv_canvas_get_img(canvas_));
    if (!img || !img->data)
      return nullptr;
    return static_cast<lv_color_t *>(const_cast<void *>(static_cast<const void *>(img->data)));
  }

  /**
   * Helper to get canvas dimensions.
   */
  void get_canvas_size(int &width, int &height) {
    if (!canvas_) {
      width = height = 0;
      return;
    }
    width = lv_obj_get_width(canvas_);
    height = lv_obj_get_height(canvas_);
  }

  /**
   * LVGL drawing primitives - common wrappers for canvas operations.
   * These handle descriptor initialization and provide a consistent API.
   */

  /**
   * Draw a filled rectangle on the canvas.
   */
  void fill_rect(int x, int y, int w, int h, lv_color_t color);

  /**
   * Draw a rectangle outline on the canvas.
   */
  void draw_rect(int x, int y, int w, int h, lv_color_t color);

  /**
   * Draw a line on the canvas.
   */
  void draw_line(int x1, int y1, int x2, int y2, lv_color_t color);

  /**
   * Draw a single pixel on the canvas.
   * Includes bounds checking.
   */
  void draw_pixel(int x, int y, lv_color_t color);

  /**
   * Draw text on the canvas with alignment.
   */
  void draw_text(int x, int y, const char *text, lv_color_t color, lv_text_align_t align = LV_TEXT_ALIGN_LEFT);

  /**
   * Fast rectangle fill using direct buffer manipulation.
   * Coordinates are relative to the game area (0,0 = top-left of game area).
   * Much faster than fill_rect() for simple solid fills.
   * Automatically invalidates the drawn area for LVGL redraw.
   */
  void fill_rect_fast(int x, int y, int w, int h, lv_color_t color);

  /**
   * Invalidate a rectangular area for LVGL redraw.
   * Coordinates are relative to the game area - this function converts them
   * to absolute canvas coordinates automatically.
   */
  void invalidate_area_rect(int x, int y, int w, int h);
};

}  // namespace esphome::lvgl_game_runner
