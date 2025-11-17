// © Copyright 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

#include "game_base.h"

namespace esphome::lvgl_game_runner {

void GameBase::fill_rect(int x, int y, int w, int h, lv_color_t color) {
  if (!canvas_)
    return;

  lv_draw_rect_dsc_t rect_dsc;
  lv_draw_rect_dsc_init(&rect_dsc);
  rect_dsc.bg_color = color;
  rect_dsc.bg_opa = LV_OPA_COVER;
  rect_dsc.border_width = 0;

  lv_canvas_draw_rect(canvas_, x, y, w, h, &rect_dsc);
}

void GameBase::draw_rect(int x, int y, int w, int h, lv_color_t color) {
  if (!canvas_)
    return;

  lv_draw_rect_dsc_t rect_dsc;
  lv_draw_rect_dsc_init(&rect_dsc);
  rect_dsc.bg_opa = LV_OPA_TRANSP;
  rect_dsc.border_color = color;
  rect_dsc.border_width = 1;
  rect_dsc.border_opa = LV_OPA_COVER;

  lv_canvas_draw_rect(canvas_, x, y, w, h, &rect_dsc);
}

void GameBase::draw_line(int x1, int y1, int x2, int y2, lv_color_t color) {
  if (!canvas_)
    return;

  lv_draw_line_dsc_t line_dsc;
  lv_draw_line_dsc_init(&line_dsc);
  line_dsc.color = color;
  line_dsc.width = 1;
  line_dsc.opa = LV_OPA_COVER;

  lv_point_t points[2] = {{(lv_coord_t) x1, (lv_coord_t) y1}, {(lv_coord_t) x2, (lv_coord_t) y2}};
  lv_canvas_draw_line(canvas_, points, 2, &line_dsc);
}

void GameBase::draw_pixel(int x, int y, lv_color_t color) {
  if (!canvas_)
    return;

  // Bounds checking
  if (x < 0 || x >= area_.w || y < 0 || y >= area_.h)
    return;

  lv_canvas_set_px_color(canvas_, x, y, color);
}

void GameBase::draw_text(int x, int y, const char *text, lv_color_t color, lv_text_align_t align) {
  if (!canvas_ || !text)
    return;

  lv_draw_label_dsc_t label_dsc;
  lv_draw_label_dsc_init(&label_dsc);
  label_dsc.color = color;
  label_dsc.font = LV_FONT_DEFAULT;
  label_dsc.align = align;

  lv_canvas_draw_text(canvas_, x, y, area_.w, &label_dsc, text);
}

void GameBase::fill_rect_fast(int x, int y, int w, int h, lv_color_t color) {
  lv_color_t *buf = get_canvas_buffer();
  if (!buf)
    return;

  // Coordinates are relative to game area
  // Fill the rectangle directly in the buffer
  for (int dy = 0; dy < h; dy++) {
    const int py = y + dy;
    if (py < 0 || py >= area_.h)
      continue;

    // Calculate row offset using area width for stride
    lv_color_t *row = &buf[py * area_.w];

    for (int dx = 0; dx < w; dx++) {
      const int px = x + dx;
      if (px >= 0 && px < area_.w) {
        row[px] = color;
      }
    }
  }

  // Automatically invalidate the drawn area
  invalidate_area_rect(x, y, w, h);
}

void GameBase::invalidate_area_rect(int x, int y, int w, int h) {
  if (!canvas_)
    return;

  // Convert relative coordinates to absolute canvas coordinates
  lv_area_t area;
  area.x1 = area_.x + x;
  area.y1 = area_.y + y;
  area.x2 = area_.x + x + w;
  area.y2 = area_.y + y + h;

  lv_obj_invalidate_area(canvas_, &area);
}

}  // namespace esphome::lvgl_game_runner
