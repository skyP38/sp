#include "stereobm.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Макрос для ограничения значения в диапазоне [low, high]
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

// Предварительная фильтрация X-Sobel
void prefilter_xsobel(const guchar *input, guchar *output, 
                     gint width, gint height, gint pre_filter_cap) {
    
    const gint OFS = 256*4, TABSZ = OFS*2 + 256;
    guchar tab[TABSZ];

    // Создание таблицы поиска для быстрого ограничения значений
    for(gint x = 0; x < TABSZ; x++)
        tab[x] = (guchar)(x - OFS < -pre_filter_cap ? 0 : x - OFS > pre_filter_cap ? pre_filter_cap*2 : x - OFS + pre_filter_cap);
    
    guchar border_value = tab[0 + OFS]; // val0 

    // фильтр Собеля 
    for(gint i = 1; i < height - 1; i++) {
        for(gint j = 1; j < width - 1; j++) {
            gint d0 = input[(i-1)*width + j+1] - input[(i-1)*width + j-1];
            gint d1 = input[i*width + j+1] - input[i*width + j-1];
            gint d2 = input[(i+1)*width + j+1] - input[(i+1)*width + j-1];
            
            gint val = d0 + d1*2 + d2;
            output[i*width + j] = tab[val + OFS];
        }
    }

    // Обработка границ
    for(gint i = 0; i < height; i++) {
        output[i*width] = border_value;
        output[i*width + width - 1] = border_value;
    }
    for(gint j = 0; j < width; j++) {
        output[j] = border_value;
        output[(height-1)*width + j] = border_value;
    }
}

// Вычисление текстуры в окрестности точки
gint compute_texture(const guchar* img, gint x, gint y, gint width, gint height, gint block_size, const guchar* tab) {
    gint texture = 0;
    gint half_block = block_size / 2;

    if(x < half_block || x >= width - half_block - 1 ||
       y < half_block || y >= height - half_block - 1) {
        return 0;
    }
    
    // Суммирование значений текстуры в окне
    for(gint i = -half_block; i <= half_block; i++) {
        for(gint j = -half_block; j <= half_block; j++) {
            gint nx = x + j;
            gint ny = y + i;
            gint lval = img[ny * width + nx];
            texture += tab[lval]; 
        }
    }
    return texture;
}

// Основная функция вычисления карты диспаратности
gint* stereobm_compute(const guchar *left_img, const guchar *right_img,
                             gint width, gint height, StereoBMParams *params) {
  gint i, j, d;
  gint half_block = params->block_size / 2;
  gint min_disparity = 0;

  // Выделение памяти для отфильтрованных изображений
  guchar *left_filtered = g_new0(guchar, width * height);
  guchar *right_filtered = g_new0(guchar, width * height);

  // Предварительная фильтрация обоих изображений
  prefilter_xsobel(left_img, left_filtered, width, height, params->pre_filter_cap);
  prefilter_xsobel(right_img, right_filtered, width, height, params->pre_filter_cap);

  gint *disparity_map = g_new0(gint, width * height);

  // Таблица для вычисления текстуры
  const gint TABSZ = 256;
  guchar tab[TABSZ];
  for(gint x = 0; x < TABSZ; x++)
    tab[x] = (guchar)abs(x - params->pre_filter_cap);
  
  // Основной цикл по всем пикселям изображения
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      // Вычисление текстуры в текущей точке
      gint texture = compute_texture(left_filtered, j, i, width, height, params->block_size, tab);

      // Отсечение слаботекстурированных областей
      if (texture < params->texture_threshold) {
        disparity_map[i * width + j] = 0;
        continue;
      }
      
      // Поиск наилучшей диспаратности
      gint best_disparity = 0;
      gint best_cost = G_MAXINT;
      gint second_best_cost = G_MAXINT;

      // Перебор всех возможных диспаратностей
      for (d = 0; d < params->num_disparities; d++) {
        gint right_x = j - d;

        // границы для правого изображения
        if (right_x < half_block || right_x >= width - half_block - 1) {
          continue;
        }

        gint sad = 0;
        
        // SAD для текущей диспаратности
        for (gint bi = -half_block; bi <= half_block; bi++) {
          for (gint bj = -half_block; bj <= half_block; bj++) {
            gint left_idx = (i + bi) * width + (j + bj);
            gint right_idx = (i + bi) * width + (right_x + bj);
            
            sad += abs(left_filtered[left_idx] - right_filtered[right_idx]);
          }
        }

        if (sad < best_cost) {
          second_best_cost = best_cost;
          best_cost = sad;
          best_disparity = d;
        } else if (sad < second_best_cost) {
          second_best_cost = sad;
        }
      }

      // проверка качества
      if (best_cost == G_MAXINT) {
        disparity_map[i * width + j] = 0;
        continue;
      } 

      gint uniqueness_threshold;
      if (best_cost > 0) {
          uniqueness_threshold = (best_cost * params->uniqueness_ratio) / 100;
      } else {
          uniqueness_threshold = 0;  
      }

      // Проверка с минимальным порогом 
      if (second_best_cost == G_MAXINT) {
          disparity_map[i * width + j] = (best_disparity + min_disparity) * 16;
      } else if (second_best_cost - best_cost > uniqueness_threshold) {
          disparity_map[i * width + j] = (best_disparity + min_disparity) * 16;
      } else {
          disparity_map[i * width + j] = 0;
      }
    }
 
    // Обновление прогресса
    if (i % 10 == 0) {
      gdouble progress = (gdouble)(i - half_block) / (gdouble)(height - 2 * half_block);
      gimp_progress_update(progress);
    }
  }

  g_free(left_filtered);
  g_free(right_filtered);

  return disparity_map;
}

// Функция для цветной нормализации карты диспаратности
void normalize_disparity_map_color(gint *disparity_map, guchar *output, 
                                   gint width, gint height, gint num_disparities) {
  
  gint min_disp = G_MAXINT;
  gint max_disp = 0;

  for (gint i = 0; i < width * height; i++) {
    if (disparity_map[i] > 0) {
      if (disparity_map[i] < min_disp) min_disp = disparity_map[i];
      if (disparity_map[i] > max_disp) max_disp = disparity_map[i];
    }
  }
  
  if (min_disp == G_MAXINT) {
    min_disp = 0;
    max_disp = num_disparities * 16;
  }
  
  gint range = max_disp - min_disp;
  if (range == 0) range = 1;
  
  for (gint i = 0; i < width * height; i++) {
      gint idx = i * 3;
      gfloat normalized = (gfloat)(disparity_map[i] - min_disp) / range;

      gint r, g, b;

      if (normalized < 0.125) {
        r = 0;
        g = 0;
        b = 128 + (gint)(normalized * 8 * 127); 
      } else if (normalized < 0.375) {
        r = 0;
        g = (gint)((normalized - 0.125) * 4 * 255);
        b = 255;
      } else if (normalized < 0.625) {
        r = (gint)((normalized - 0.375) * 4 * 255);
        g = 255;
        b = (gint)((0.625 - normalized) * 4 * 255);
      } else if (normalized < 0.875) {
        r = 255;
        g = (gint)((0.875 - normalized) * 4 * 255);
        b = 0;
      } else {
        r = 255;
        g = 0;
        b = 0;
      }

      output[idx] = r;     // R
      output[idx + 1] = g; // G
      output[idx + 2] = b; // B
  }
}