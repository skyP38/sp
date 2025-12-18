#include "pixel.h"
#include <math.h>
#include "watermark_logic.h"


// Наложение с учетом прозрачности (alpha blending)
void blend_layers(guchar *base_pixels, guchar *overlay_pixels, 
                  gint width, gint height, gint bpp,
                  gdouble opacity, gint overlay_x, gint overlay_y,
                  gint overlay_width, gint overlay_height)
{
    // Цикл по строкам накладываемого изображения
    for (gint y = 0; y < overlay_height; y++) {
        gint base_y = overlay_y + y;
        if (base_y < 0 || base_y >= height) continue;
        
        // Цикл по столбцам накладываемого изображения
        for (gint x = 0; x < overlay_width; x++) {
            gint base_x = overlay_x + x;
            if (base_x < 0 || base_x >= width) continue;
            
            // Расчет индексов для доступа к пикселям
            gint base_idx = (base_y * width + base_x) * 3;  // Базовое изображение RGB (3 канала)
            gint overlay_idx = (y * overlay_width + x) * 4; // Накладываемое изображение RGBA (4 канала)
            
            // Расчет альфа-канала с учетом общей прозрачности
            gdouble overlay_alpha = overlay_pixels[overlay_idx + 3] / 255.0;
            gdouble final_alpha = opacity * overlay_alpha;

            // Пропуск полностью прозрачных пикселей
            if (overlay_pixels[overlay_idx + 3] == 0) continue;
            
            // Наложение для каждого цветового канала (R, G, B)
            for (gint c = 0; c < 3; c++) {
                gdouble base_val = base_pixels[base_idx + c];
                gdouble overlay_val = overlay_pixels[overlay_idx + c];
                
                // Формула alpha blending
                gdouble blended = overlay_val * final_alpha + base_val * (1.0 - final_alpha);
                // Ограничение значения и запись результата
                base_pixels[base_idx + c] = (guchar)CLAMP(blended, 0, 255);
            }
        }
    }
}

// Функция для загрузки изображения в пиксельный буфер
gboolean load_watermark_pixels(const gchar *filename, guchar **pixels, 
                                     gint *width, gint *height, gint *bpp)
{
    if (!filename || !g_file_test(filename, G_FILE_TEST_EXISTS))
        return FALSE;

    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(filename, &error);
    
    if (!pixbuf) {
        g_print("Failed to load image: %s\n", error->message);
        g_error_free(error);
        return FALSE;
    }

    // Получение параметров изображения
    *width = gdk_pixbuf_get_width(pixbuf);
    *height = gdk_pixbuf_get_height(pixbuf);
    *bpp = gdk_pixbuf_get_n_channels(pixbuf);

     g_print("DEBUG: Loaded watermark: %dx%d, %d bpp, has_alpha: %d\n", 
           *width, *height, *bpp, gdk_pixbuf_get_has_alpha(pixbuf));
    
    // Конвертация в RGBA для единообразия
    GdkPixbuf *pixbuf_rgba;
    if (gdk_pixbuf_get_has_alpha(pixbuf)) {
        // Уже имеет альфа-канал - просто копируем
        pixbuf_rgba = pixbuf;
    } else {
        // Добавляем альфа-канал
        pixbuf_rgba = gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);
        g_object_unref(pixbuf);
    }
        
    *bpp = 4; // Теперь всегда RGBA
    gint rowstride = gdk_pixbuf_get_rowstride(pixbuf_rgba);
    gint data_size = *height * rowstride;
    
    *pixels = g_malloc(data_size);
    memcpy(*pixels, gdk_pixbuf_get_pixels(pixbuf), data_size);
    
    g_object_unref(pixbuf_rgba);
    return TRUE;
}

// Функция масштабирования изображения методом билинейной интерполяции
guchar* scale_image_pixels(const guchar *src_pixels, gint src_width, gint src_height, 
                                 gint bpp, gint new_width, gint new_height)
{
    // Расчет шагов строк
    gint src_rowstride = src_width * bpp;
    gint dst_rowstride = new_width * bpp;

    // Выделение памяти для масштабированного изображения
    guchar *dst_pixels = g_malloc0(new_height * dst_rowstride);
    
    // Цикл по строкам нового изображения
    for (gint y = 0; y < new_height; y++) {
        // Расчет координат в исходном изображении
        gdouble src_y = (gdouble)y / new_height * src_height;
        gint src_y0 = (gint)src_y;
        gint src_y1 = MIN(src_y0 + 1, src_height - 1);
        gdouble y_ratio = src_y - src_y0;
        
        // Цикл по столбцам нового изображения
        for (gint x = 0; x < new_width; x++) {
            // Расчет координат в исходном изображении
            gdouble src_x = (gdouble)x / new_width * src_width;
            gint src_x0 = (gint)src_x;
            gint src_x1 = MIN(src_x0 + 1, src_width - 1);
            gdouble x_ratio = src_x - src_x0;
            
            // Цикл по цветовым каналам
            for (gint c = 0; c < bpp; c++) {
                // Билинейная интерполяция по 4 точкам
                gdouble value = 
                    (1 - x_ratio) * (1 - y_ratio) * src_pixels[src_y0 * src_rowstride + src_x0 * bpp + c] +
                    x_ratio * (1 - y_ratio) * src_pixels[src_y0 * src_rowstride + src_x1 * bpp + c] +
                    (1 - x_ratio) * y_ratio * src_pixels[src_y1 * src_rowstride + src_x0 * bpp + c] +
                    x_ratio * y_ratio * src_pixels[src_y1 * src_rowstride + src_x1 * bpp + c];
                
                dst_pixels[y * dst_rowstride + x * bpp + c] = (guchar)CLAMP(value, 0, 255);
            }
        }
    }
    
    return dst_pixels;
}

// Функция поворота изображения
guchar* rotate_image_pixels(const guchar *src_pixels, gint src_width, gint src_height,
                                  gint bpp, gdouble angle_degrees, gint *new_width, gint *new_height)
{
    // Конвертация угла в радианы
    gdouble angle = angle_degrees * G_PI / 180.0;
    gdouble cos_angle = cos(angle);
    gdouble sin_angle = sin(angle);
    
    // Вычисляем размеры повернутого изображения
    gint half_src_w = src_width / 2;
    gint half_src_h = src_height / 2;
    
    // Координаты углов после поворота
    gint corners_x[4], corners_y[4];
    corners_x[0] = -half_src_w * cos_angle - (-half_src_h) * sin_angle;
    corners_y[0] = -half_src_w * sin_angle + (-half_src_h) * cos_angle;
    
    corners_x[1] = half_src_w * cos_angle - (-half_src_h) * sin_angle;
    corners_y[1] = half_src_w * sin_angle + (-half_src_h) * cos_angle;
    
    corners_x[2] = -half_src_w * cos_angle - half_src_h * sin_angle;
    corners_y[2] = -half_src_w * sin_angle + half_src_h * cos_angle;
    
    corners_x[3] = half_src_w * cos_angle - half_src_h * sin_angle;
    corners_y[3] = half_src_w * sin_angle + half_src_h * cos_angle;
    
    // Находим максимальные размеры
    gint max_x = 0, max_y = 0;
    for (gint i = 0; i < 4; i++) {
        max_x = MAX(max_x, abs(corners_x[i]));
        max_y = MAX(max_y, abs(corners_y[i]));
    }
    
    *new_width = max_x * 2;
    *new_height = max_y * 2;
    
    // Выделение памяти для повернутого изображения
    gint dst_rowstride = *new_width * bpp;
    guchar *dst_pixels = g_malloc0(*new_height * dst_rowstride);
    
    // Центр нового изображения
    gint center_x = *new_width / 2;
    gint center_y = *new_height / 2;
    
    // Цикл по всем пикселям нового изображения
    for (gint y = 0; y < *new_height; y++) {
        for (gint x = 0; x < *new_width; x++) {
            // Обратное преобразование координат
            gdouble src_x = cos_angle * (x - center_x) + sin_angle * (y - center_y) + half_src_w;
            gdouble src_y = -sin_angle * (x - center_x) + cos_angle * (y - center_y) + half_src_h;
            
            // Проверка попадания в границы исходного изображения
            if (src_x >= 0 && src_x < src_width - 1 && src_y >= 0 && src_y < src_height - 1) {
                gint src_x0 = (gint)src_x;
                gint src_y0 = (gint)src_y;
                gint src_x1 = src_x0 + 1;
                gint src_y1 = src_y0 + 1;
                
                gdouble x_ratio = src_x - src_x0;
                gdouble y_ratio = src_y - src_y0;
                
                gint src_rowstride = src_width * bpp;
                
                // Билинейная интерполяция для каждого канала
                for (gint c = 0; c < bpp; c++) {
                    gdouble value = 
                        (1 - x_ratio) * (1 - y_ratio) * src_pixels[src_y0 * src_rowstride + src_x0 * bpp + c] +
                        x_ratio * (1 - y_ratio) * src_pixels[src_y0 * src_rowstride + src_x1 * bpp + c] +
                        (1 - x_ratio) * y_ratio * src_pixels[src_y1 * src_rowstride + src_x0 * bpp + c] +
                        x_ratio * y_ratio * src_pixels[src_y1 * src_rowstride + src_x1 * bpp + c];
                    
                    dst_pixels[y * dst_rowstride + x * bpp + c] = (guchar)CLAMP(value, 0, 255);
                }
            }
        }
    }
    
    return dst_pixels;
}

// Функция для единичного наложения изображения водяного знака
gboolean apply_single_image_watermark_pixel_level(GimpImage *image, GimpDrawable *drawable, 
                                                  WatermarkParams *params)
{
    // Получение размеров основного изображения
    gint image_width = gimp_drawable_get_width(drawable);
    gint image_height = gimp_drawable_get_height(drawable);
    
    // Получаем буфер основного изображения
    GeglBuffer *buffer = gimp_drawable_get_buffer(drawable);
    guchar *image_pixels = g_malloc(image_width * image_height * 3);
    
    // Чтение пикселей из буфера в массив
    gegl_buffer_get(buffer, GEGL_RECTANGLE(0, 0, image_width, image_height), 1.0,
                   babl_format("R'G'B' u8"), image_pixels,
                   GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);    

    // Загружаем водяной знак
    guchar *watermark_pixels = NULL;
    gint wm_width, wm_height, wm_bpp;
    
    // Проверка успешности загрузки
    if (!load_watermark_pixels(params->image_path, &watermark_pixels, 
                              &wm_width, &wm_height, &wm_bpp)) {
        g_free(image_pixels);
        g_object_unref(buffer);
        return FALSE;
    }
    
    // Применяем трансформации: масштабирование
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
    
    // Применяем трансформации: поворот
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
    
    // Вычисляем позицию для единичного размещения
    gint pos_x, pos_y;
    calculate_single_position(image_width, image_height, wm_width, wm_height, 
                             params->position_type, params->custom_x, params->custom_y,
                             &pos_x, &pos_y);
    
    // Накладываем водяной знак
    blend_layers(image_pixels, watermark_pixels, image_width, image_height, 3,
                params->opacity, pos_x, pos_y, wm_width, wm_height);
    
    // Сохраняем результат обратно в буфер
    gegl_buffer_set(buffer, GEGL_RECTANGLE(0, 0, image_width, image_height), 0,
                   babl_format("RGB u8"), image_pixels,
                   GEGL_AUTO_ROWSTRIDE);
    
    // Обновляем изображение
    gimp_drawable_update(drawable, 0, 0, image_width, image_height);
    
    g_free(image_pixels);
    g_free(watermark_pixels);
    g_object_unref(buffer);
    
    return TRUE;
}

// Вспомогательная функция для вычисления позиции
void calculate_single_position(gint img_width, gint img_height,
                                     gint wm_width, gint wm_height,
                                     gint position_type, gint custom_x, gint custom_y,
                                     gint *pos_x, gint *pos_y)
{
    gint margin = 20;
    
    switch (position_type) {
        case 0: // Центр
            *pos_x = (img_width - wm_width) / 2;
            *pos_y = (img_height - wm_height) / 2;
            break;
        case 1: // Левый верх
            *pos_x = margin;
            *pos_y = margin;
            break;
        case 2: // Правый верх
            *pos_x = img_width - wm_width - margin;
            *pos_y = margin;
            break;
        case 3: // Левый низ
            *pos_x = margin;
            *pos_y = img_height - wm_height - margin;
            break;
        case 4: // Правый низ
            *pos_x = img_width - wm_width - margin;
            *pos_y = img_height - wm_height - margin;
            break;
        case 5: // Случайно
            *pos_x = g_random_int_range(margin, img_width - wm_width - margin);
            *pos_y = g_random_int_range(margin, img_height - wm_height - margin);
            break;
        case 6: // Точные координаты
            *pos_x = custom_x;
            *pos_y = custom_y;
            break;
        default:
            *pos_x = (img_width - wm_width) / 2;
            *pos_y = (img_height - wm_height) / 2;
            break;
    }
    
    // Ограничиваем координаты
    *pos_x = CLAMP(*pos_x, 0, img_width - wm_width);
    *pos_y = CLAMP(*pos_y, 0, img_height - wm_height);
}

// Функция получения пикселей из текстового слоя
guchar* get_text_layer_pixels(GimpImage *image, WatermarkParams *params, 
                              gint *width, gint *height, gint *bpp)
{
    // Получение шрифта из контекста GIMP
    GimpFont *font = gimp_context_get_font();
    if (!font) {
        gimp_message("Failed to get font from context");
    }

    // Создание текстового слоя
    GimpTextLayer *text_layer = gimp_text_layer_new(image, 
                                                   params->text_content, 
                                                   font,  
                                                   params->font_size, 
                                                   gimp_unit_pixel());
    if (!text_layer) {
        g_print("Failed to create text layer\n");
        return NULL;
    }
    
    // Получаем размеры текстового слоя
    *width = gimp_drawable_get_width(GIMP_DRAWABLE(text_layer));
    *height = gimp_drawable_get_height(GIMP_DRAWABLE(text_layer));
    *bpp = 4; 

    g_print("DEBUG: Text layer size: %dx%d\n", *width, *height);
    
    // Получаем буфер из слоя
    GeglBuffer *buffer = gimp_drawable_get_buffer(GIMP_DRAWABLE(text_layer));
    guchar *pixels = g_malloc(*width * *height * *bpp);
        memset(pixels, 0, *width * *height * *bpp);
    
    // Копируем данные из буфера
    gegl_buffer_get(buffer, GEGL_RECTANGLE(0, 0, *width, *height), 1.0,
                   babl_format("RGBA u8"), pixels,
                   GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);

    
    g_object_unref(buffer);
    
    return pixels;
}