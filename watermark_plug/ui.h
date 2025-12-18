#ifndef UI_H
#define UI_H

#include <libgimp/gimp.h>
#include <gtk/gtk.h>

// Основная структура параметров водяного знака
typedef struct {
    // Тип водяного знака 0 - текст, 1 - изображение
    gint watermark_type;  
    
    // Текстовые параметры
    gchar *text_content; // содержание 
    gchar *font_name; // имя шрифта
    gdouble font_size; // размер шрифта
    
    // Путь до изображения
    gchar *image_path; 
    
    // Параметры размещения
    gint placement_type;  // 0 - единичный, 1 - мозаика
    gint position_type;   // 0-центр, 1-левый верх, 2-правый верх, 3-левый низ, 4-правый низ, 5-случайно, 6-точные координаты
    gint custom_x; // координата х для точного размещения
    gint custom_y; // координата y для точного размещения

    // Мозаичные параметры
    gint mosaic_rows;  // количество строк
    gint mosaic_cols; // количество столбцов
    gdouble mosaic_spacing; // отступ
    
    // Поворот
    gdouble rotation_angle;
    // Масштабирование
    gdouble scale_factor;
    
    // Прозрачность
    gdouble opacity;
    
} WatermarkParams;

// Создание параметров водяной марки
WatermarkParams* create_watermark_params(void);

// Освобождение параметров водяной марки
void free_watermark_params(WatermarkParams *params);

// Отображение диалогового окна
gboolean show_watermark_dialog(WatermarkParams *params);


// Обработчики сигналов
void on_watermark_type_changed(GtkComboBox *combo, gpointer user_data);
void on_placement_type_changed(GtkComboBox *combo, gpointer user_data);
void on_position_type_changed(GtkComboBox *combo, gpointer user_data);
void on_browse_image_clicked(GtkButton *button, gpointer user_data);
#endif