#include <libgimp/gimp.h>
#include "ui.h"
#include "pixel.h"

// Функция применения мозаичного водяного знака
gboolean apply_mosaic_watermark(GimpImage *image, GimpDrawable *drawable, 
                                     WatermarkParams *params)
{
    gimp_message("Applying mosaic watermark at pixel level");
    
    // Получение размеров изображения
    gint image_width = gimp_drawable_get_width(drawable);
    gint image_height = gimp_drawable_get_height(drawable);
    
    // Получаем буфер основного изображения
    GeglBuffer *buffer = gimp_drawable_get_buffer(drawable);
    guchar *image_pixels = g_malloc(image_width * image_height * 3);
    
    // Чтение пикселей из буфера
    gegl_buffer_get(buffer, GEGL_RECTANGLE(0, 0, image_width, image_height), 1.0,
                   babl_format("R'G'B' u8"), image_pixels,
                   GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
    
    // Загружаем и подготавливаем водяной знак
    guchar *watermark_pixels = NULL;
    gint wm_width, wm_height, wm_bpp;
    
    // Выбор типа водяного знака
    if (params->watermark_type == 0) {
        // Текстовый водяной знак
        watermark_pixels = get_text_layer_pixels(image, params, &wm_width, &wm_height, &wm_bpp);
    } else {
        // Водяной знак-изображение
        if (!load_watermark_pixels(params->image_path, &watermark_pixels, 
                                  &wm_width, &wm_height, &wm_bpp)) {
            g_free(image_pixels);
            g_object_unref(buffer);
            return FALSE;
        }
    }

    if (!watermark_pixels) {
        g_free(image_pixels);
        g_object_unref(buffer);
        return FALSE;
    }
    
    // Масштабируем водяной знак
    if (params->scale_factor != 1.0) {
        gint new_width = wm_width * params->scale_factor;
        gint new_height = wm_height * params->scale_factor;
        
        guchar *scaled_pixels = scale_image_pixels(watermark_pixels, wm_width, wm_height, 
                                                  wm_bpp, new_width, new_height);
        g_free(watermark_pixels);
        watermark_pixels = scaled_pixels;
        wm_width = new_width;
        wm_height = new_height;
    }
    
    // Поворачиваем водяной знак
    if (params->rotation_angle != 0.0) {
        gint rotated_width, rotated_height;
        guchar *rotated_pixels = rotate_image_pixels(watermark_pixels, wm_width, wm_height,
                                                    wm_bpp, params->rotation_angle, 
                                                    &rotated_width, &rotated_height);
        g_free(watermark_pixels);
        watermark_pixels = rotated_pixels;
        wm_width = rotated_width;
        wm_height = rotated_height;
    }
    
    // Создаем мозаику
    gint tiles_x = params->mosaic_cols;
    gint tiles_y = params->mosaic_rows;
    gint spacing = params->mosaic_spacing;
    
    gint base_margin = 0;
    gint available_width = image_width - 2 * base_margin;
    gint available_height = image_height - 2 * base_margin;
    
    gint tile_width = available_width / tiles_x;
    gint tile_height = available_height / tiles_y;
    
    // Цикл по строкам и колонкам мозаики
    for (gint row = 0; row < tiles_y; row++) {
        for (gint col = 0; col < tiles_x; col++) {
            gint pos_x, pos_y;
            
            if (row % 2 == 0) {
                pos_x = base_margin + col * (tile_width + spacing);
                pos_y = base_margin + row * (tile_height + spacing);
            } else {
                pos_x = base_margin + (col * (tile_width + spacing)) + (tile_width / 2);
                pos_y = base_margin + row * (tile_height + spacing);
            }
            // Добавляем случайные смещения
            gint random_offset_x = g_random_int_range(-spacing, spacing);
            gint random_offset_y = g_random_int_range(-spacing, spacing);
            
            // Финальное местоположение
            gint final_x = pos_x + random_offset_x;
            gint final_y = pos_y + random_offset_y;
            
            // Накладываем водяной знак
            blend_layers(image_pixels, watermark_pixels, image_width, image_height, 3,
                        params->opacity, final_x, final_y, wm_width, wm_height);
        }
    }
    
    // Сохраняем результат обратно в буфер
    gegl_buffer_set(buffer, GEGL_RECTANGLE(0, 0, image_width, image_height), 0,
                   babl_format("R'G'B' u8"), image_pixels,
                   GEGL_AUTO_ROWSTRIDE);
    
    // Обновляем изображение
    gimp_drawable_update(drawable, 0, 0, image_width, image_height);
    
    // Освобождаем память
    g_free(image_pixels);
    g_free(watermark_pixels);
    g_object_unref(buffer);
    
    return TRUE;
}