#include "stereobm.h"

// Основная функция обработки изображения
void stereobm_plugin(GimpProcedure *procedure, GimpDrawable *drawable, 
                    StereoBMParams *params) {
  gint width = gimp_drawable_get_width(drawable);
  gint height = gimp_drawable_get_height(drawable);
  
  // Проверка, что изображение side-by-side
  if (width % 2 != 0) {
    gimp_message("Изображение должно быть side-by-side (четная ширина)");
    return;
  }
  
  gint stereo_width = width / 2;
  
  // Выделение память для изображений
  guchar *original_image = g_new(guchar, width * height * 3); // RGB
  guchar *left_image = g_new(guchar, stereo_width * height);
  guchar *right_image = g_new(guchar, stereo_width * height);
  guchar *output_image = g_new0(guchar, stereo_width * height * 3);
  
  // Получение изображения (RGB)
  GeglBuffer *buffer = gimp_drawable_get_buffer(drawable);
  gegl_buffer_get(buffer, NULL, 1.0, babl_format("R'G'B' u8"), original_image,
                  GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  
  // Разделяем side-by-side изображение на левое и правое
  for (gint i = 0; i < height; i++) {
    for (gint j = 0; j < stereo_width; j++) {
      gint left_idx = (i * width + j) * 3;
      gint right_idx = (i * width + j + stereo_width) * 3;
      
      // Формула для конвертации RGB в grayscale: 0.299*R + 0.587*G + 0.114*B
      left_image[i * stereo_width + j] = (guchar)(
        0.299 * original_image[left_idx] + 
        0.587 * original_image[left_idx + 1] + 
        0.114 * original_image[left_idx + 2]);
      
      right_image[i * stereo_width + j] = (guchar)(
        0.299 * original_image[right_idx] + 
        0.587 * original_image[right_idx + 1] + 
        0.114 * original_image[right_idx + 2]);
    }
  }

  gimp_progress_init("Computing disparity map...");
  
  // Вычисление карты диспаратности
  gint *disparity_map = stereobm_compute(left_image, right_image, stereo_width, height, params);

  // Нормализация для отображения
  normalize_disparity_map_color(disparity_map, output_image, stereo_width, height, 
                         params->num_disparities);

  // Создаем новое изображение для результата
  GimpImage *new_image = gimp_image_new(stereo_width, height, GIMP_RGB);
  GimpLayer *new_layer = gimp_layer_new(new_image, "Disparity Map", 
                                       stereo_width, height, GIMP_RGB_IMAGE,
                                       100, GIMP_LAYER_MODE_NORMAL);
  
  gimp_image_insert_layer(new_image, new_layer, NULL, 0);
  
 // Записываем результат
  GeglBuffer *output_buffer = gimp_drawable_get_buffer(GIMP_DRAWABLE(new_layer));
  gegl_buffer_set(output_buffer, NULL, 0, babl_format("R'G'B' u8"), output_image,
                  GEGL_AUTO_ROWSTRIDE);
  
  gimp_drawable_update(GIMP_DRAWABLE(new_layer), 0, 0, stereo_width, height);
  
  // Освобождаем память
  g_free(original_image);
  g_free(left_image);
  g_free(right_image);
  g_free(output_image);
  g_free(disparity_map);
  
  g_object_unref(buffer);
  g_object_unref(output_buffer);
  
  // Отображаем результат
  gimp_display_new(new_image);
  
  gimp_message("Disparity map computed successfully!");
}