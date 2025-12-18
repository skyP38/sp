#include "image.h"

// Функция создания слоя водяного знака из изображения
GimpLayer* create_image_watermark_layer(GimpImage *image, WatermarkParams *params)
{
    if (!params->image_path || !g_file_test(params->image_path, G_FILE_TEST_EXISTS)) {
        gimp_message("Watermark image file not found");
        return NULL;
    }
    
    gimp_message("Loading watermark image");

    // Создаем GFile из пути загрузки
    GFile *file = g_file_new_for_path(params->image_path);
    
    // Загружаем слой напрямую в изображение
    GimpLayer *watermark_layer = gimp_file_load_layer(GIMP_RUN_NONINTERACTIVE, image, file);
    g_object_unref(file);
    
    if (!watermark_layer) {
        gimp_message("Failed to load watermark image as layer");
        return NULL;
    }
    
    // Устанавливаем имя слоя
    gimp_item_set_name(GIMP_ITEM(watermark_layer), "Watermark Image");
    
    // Установка режима наложения
    gimp_layer_set_mode(watermark_layer, GIMP_LAYER_MODE_NORMAL);
    return watermark_layer;
}


// Функция для единичного водяного знака-изображения
gboolean apply_single_image_watermark(GimpImage *image, GimpDrawable *drawable, 
                                           WatermarkParams *params)
{
    g_print("Applying single image watermark\n");
    g_print("Image path: '%s'\n", params->image_path ? params->image_path : "NULL");

    // Проверяем путь к изображению
    if (!params->image_path || strlen(params->image_path) == 0) {
        gimp_message("Image path is empty. Please select an image file.");
        return FALSE;
    }
    
    // Проверяем существование изображения
    if (!g_file_test(params->image_path, G_FILE_TEST_EXISTS)) {
        gimp_message("Image file does not exist. Please check the file path.");
        g_print("File does not exist: %s\n", params->image_path);
        return FALSE;
    }

    // Создание слоя водяного знака
    GimpLayer *image_layer = create_image_watermark_layer(image, params);
    if (!image_layer) {
        return FALSE;
    }

    // Вставка слоя в изображение
    gimp_image_insert_layer(image, image_layer, NULL, -1);

    // Прозрачность
    gimp_layer_set_opacity(image_layer, params->opacity * 100.0);
    
    // Режим наложения для лучшей устойчивости
    gimp_layer_set_mode(image_layer, GIMP_LAYER_MODE_NORMAL);
    
    // Применяем трансформации
    apply_transformations(image_layer, params);
    
    // Позиционирование
    position_watermark_layer(image, image_layer, params);
    return TRUE;
}
