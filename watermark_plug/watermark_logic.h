#ifndef WATERMARK_LOGIC_H
#define WATERMARK_LOGIC_H

#include <libgimp/gimp.h>
#include "ui.h"

// Функции применения трансформации
void apply_transformations(GimpLayer *layer, WatermarkParams *params);

// Функции позиционирования
void position_watermark_layer(GimpImage *image, GimpLayer *layer, WatermarkParams *params);

// Основная функция применения водяного знака
gboolean apply_watermark_to_image(GimpImage *image, GimpDrawable *drawable, WatermarkParams *params);

#endif