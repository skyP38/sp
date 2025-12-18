#ifndef PIXEL_H
#define PIXEL_H

#include <libgimp/gimp.h>
#include "ui.h"

// Функция наложения слоев с альфа-смешением
void blend_layers(guchar *base_pixels, guchar *overlay_pixels, 
                  gint width, gint height, gint bpp,
                  gdouble opacity, gint overlay_x, gint overlay_y,
                  gint overlay_width, gint overlay_height);

// Функция загрузки пикселей водяного знака
gboolean load_watermark_pixels(const gchar *filename, guchar **pixels, 
                                     gint *width, gint *height, gint *bpp);

// Функция масштабирования изображения
guchar* scale_image_pixels(const guchar *src_pixels, gint src_width, gint src_height, 
                                 gint bpp, gint new_width, gint new_height);
                    
// Функция поворота изображения
guchar* rotate_image_pixels(const guchar *src_pixels, gint src_width, gint src_height,
                                  gint bpp, gdouble angle_degrees, gint *new_width, gint *new_height);

// Функция применения единичного водяного знака на уровне пикселей
gboolean apply_single_image_watermark_pixel_level(GimpImage *image, GimpDrawable *drawable, 
                                                  WatermarkParams *params);

// Функция вычисления позиции
void calculate_single_position(gint img_width, gint img_height,
                                     gint wm_width, gint wm_height,
                                     gint position_type, gint custom_x, gint custom_y,
                                     gint *pos_x, gint *pos_y);

// Функция получения пикселей из текстового слоя
guchar* get_text_layer_pixels(GimpImage *image, WatermarkParams *params, 
                              gint *width, gint *height, gint *bpp);


#endif