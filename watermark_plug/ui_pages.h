#ifndef UI_PAGES_H
#define UI_PAGES_H

#include <libgimp/gimp.h>
#include <gtk/gtk.h>
#include "ui.h"

// Структура для хранения ссылок на виджеты
typedef struct {
    GtkWidget *text_entry;
    GtkWidget *font_button;
    GtkWidget *font_size_spin;
    GtkWidget *watermark_type_combo;
    GtkWidget *opacity_scale;
    GtkWidget *rotation_spin;
    GtkWidget *scale_scale;
    GtkWidget *position_combo;
    GtkWidget *custom_x_spin;
    GtkWidget *custom_y_spin;
    GtkWidget *placement_combo;
    GtkWidget *mosaic_rows_spin;
    GtkWidget *mosaic_cols_spin;
    GtkWidget *mosaic_spacing_spin;
} WidgetReferences;

// Создание страницы выбора типа водяного знака
GtkWidget* create_type_selection_page(WatermarkParams *params, WidgetReferences *refs);
// Создание страницы выбора текстовых параметров
GtkWidget* create_text_parameters_page(WatermarkParams *params, WidgetReferences *refs);
// Создание страницы выбора параметров изображения
GtkWidget* create_image_parameters_page(WatermarkParams *params, WidgetReferences *refs);
// Создание страницы выбора параметров размещения
GtkWidget* create_placement_page(WatermarkParams *params, WidgetReferences *refs);
// Создание страницы выбора параметров трансформации
GtkWidget* create_transform_page(WatermarkParams *params, WidgetReferences *refs);

#endif