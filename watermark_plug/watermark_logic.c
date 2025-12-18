#include "watermark_logic.h"
#include "ui.h"
#include "text.h"
#include "image.h"
#include "mosaic.h"
#include "pixel.h"

// Функция применения трансформаций
void apply_transformations(GimpLayer *layer, WatermarkParams *params)
{
    // Масштабирование
    if (params->scale_factor != 1.0) {
        gint layer_width = gimp_drawable_get_width(GIMP_DRAWABLE(layer));
        gint layer_height = gimp_drawable_get_height(GIMP_DRAWABLE(layer));
        gint new_width = layer_width * params->scale_factor;
        gint new_height = layer_height * params->scale_factor;
        
        gimp_layer_scale(layer, new_width, new_height, FALSE);
    }
    
    // Поворот
    if (params->rotation_angle != 0.0) {
        gimp_item_transform_rotate(GIMP_ITEM(layer), 
                                 params->rotation_angle * G_PI / 180.0, 
                                 TRUE, 0, 0);
    }
}


// Функция позиционирования водяного знака
void position_watermark_layer(GimpImage *image, GimpLayer *layer, WatermarkParams *params)
{
    gint image_width = gimp_image_get_width(image);
    gint image_height = gimp_image_get_height(image);
    gint layer_width = gimp_drawable_get_width(GIMP_DRAWABLE(layer));
    gint layer_height = gimp_drawable_get_height(GIMP_DRAWABLE(layer));
    
    gint x = 0, y = 0;
    gint margin = 20; // Отступ от краев
    
    switch (params->position_type) {
        case 0: // Центр
            x = (image_width - layer_width) / 2;
            y = (image_height - layer_height) / 2;
            break;
        case 1: // Левый верх
            x = margin;
            y = margin;
            break;
        case 2: // Правый верх
            x = image_width - layer_width - margin;
            y = margin;
            break;
        case 3: // Левый низ
            x = margin;
            y = image_height - layer_height - margin;
            break;
        case 4: // Правый низ
            x = image_width - layer_width - margin;
            y = image_height - layer_height - margin;
            break;
        case 5: // Случайно
            x = g_random_int_range(margin, image_width - layer_width - margin);
            y = g_random_int_range(margin, image_height - layer_height - margin);
            break;
        case 6: // Точные координаты
            x = params->custom_x;
            y = params->custom_y;
            break;
        default: // По умолчанию - центр
            x = (image_width - layer_width) / 2;
            y = (image_height - layer_height) / 2;
            break;
    }
    
    // Ограничиваем координаты размерами изображения
    x = CLAMP(x, 0, image_width - layer_width);
    y = CLAMP(y, 0, image_height - layer_height);
    
    gimp_layer_set_offsets(layer, x, y);
    g_print("Watermark positioned at: %d, %d\n", x, y);
}

// Основная функция применения водяного знака
gboolean apply_watermark_to_image(GimpImage *image, GimpDrawable *drawable, WatermarkParams *params)
{
    gboolean success = FALSE;

    if (params->placement_type == 0) { // Единичное размещение
        g_print("Applying SINGLE watermark at pixel level\n");
        if (params->watermark_type == 0) { // Текст
            success = apply_single_text_watermark(image, drawable, params);
        } else { // Изображение
            success = apply_single_image_watermark_pixel_level(image, drawable, params);
        }
    } else { // Мозаичное размещение
        g_print("Applying MOSAIC watermark at pixel level\n");
        success = apply_mosaic_watermark(image, drawable, params);
    }

    if (success) {
        gimp_message("Watermark applied successfully");
        gimp_displays_flush();
    } else {
        gimp_message("Failed to apply watermark");
    }
    
    return success;
}