#include "text.h"

// Функция создания текстового слоя водяного знака
GimpLayer* create_text_watermark_layer(GimpImage *image, WatermarkParams *params)
{
    GimpFont *font = gimp_context_get_font();
    if (!font) {
        gimp_message("Failed to get font from context");
    }
    
    // Создаем текстовый слой
    GimpTextLayer *text_layer = gimp_text_layer_new(image, 
                                                   params->text_content, 
                                                   font, 
                                                   params->font_size, 
                                                   gimp_unit_pixel());
    
    gimp_context_pop();
    
    if (!text_layer) {
        gimp_message("Failed to create text layer");
        return NULL;
    }
    
    // Прозрачность
    gimp_layer_set_opacity(GIMP_LAYER(text_layer), params->opacity * 100.0);
    
    // Режим наложения для лучшей устойчивости
    gimp_layer_set_mode(GIMP_LAYER(text_layer), GIMP_LAYER_MODE_NORMAL);
    
    // Применяем трансформации
    apply_transformations(GIMP_LAYER(text_layer), params);
    
    return GIMP_LAYER(text_layer);
}

// Функция для единичного текстового водяного знака
gboolean apply_single_text_watermark(GimpImage *image, GimpDrawable *drawable, 
                                          WatermarkParams *params)
{
    GimpLayer *text_layer = create_text_watermark_layer(image, params);
    if (!text_layer) {
        return FALSE;
    }

    gimp_image_insert_layer(image, text_layer, NULL, 0);
    
    // Позиционирование
    position_watermark_layer(image, text_layer, params);
    return TRUE;
}