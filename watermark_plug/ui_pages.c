#include "ui_pages.h"

// Страница выбора типа водяного знака
GtkWidget* create_type_selection_page(WatermarkParams *params, WidgetReferences *refs)
{
    // Создание вертикального контейнера
    GtkWidget *vbox, *hbox, *label, *combo;
    GtkListStore *store;
    GtkCellRenderer *renderer;
    
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    
    // Создание метки
    label = gtk_label_new("Выберите тип водяного знака:");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    
    // Горизонтальный контейнер для комбобокса
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new("Тип:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    // Создание хранилища данных для комбобокса
    store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    
    GtkTreeIter iter;
    // Добавление элемента "Текст"
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Текст", 1, 0, -1);
    
    // Добавление элемента "Изображение"
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Изображение", 1, 1, -1);
    
    // Создание комбобокса
    combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    
    // Настройка рендерера для отображения текста
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), renderer, "text", 0, NULL);
    
    // Установка активного элемента
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), params->watermark_type);
    gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);
    
    // Сохранение ссылки на виджет
    refs->watermark_type_combo = combo;
    g_print("DEBUG: Type combo created: %p\n", combo);
    
    return vbox;
}

// Страница параметров текста
GtkWidget* create_text_parameters_page(WatermarkParams *params, WidgetReferences *refs)
{
    GtkWidget *vbox, *hbox, *label, *entry, *font_button_local, *spin_button;
    
    // Основной вертикальный контейнер
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    
    // Поле для ввода текста
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new("Текст:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    entry = gtk_entry_new();
    if (params->text_content)
        gtk_entry_set_text(GTK_ENTRY(entry), params->text_content);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    
    refs->text_entry = entry;
    
    // Выбор шрифта
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new("Шрифт:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    font_button_local = gtk_font_button_new();
    if (params->font_name) {
        gtk_font_chooser_set_font(GTK_FONT_CHOOSER(font_button_local), params->font_name);
    } else {
        gtk_font_chooser_set_font(GTK_FONT_CHOOSER(font_button_local), "Sans 12");
    }
    gtk_box_pack_start(GTK_BOX(hbox), font_button_local, TRUE, TRUE, 0);
    
    refs->font_button = font_button_local;
    
    // Размер шрифта
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new("Размер шрифта:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    // Создание спин-кнопки для размера шрифта
    spin_button = gtk_spin_button_new_with_range(6.0, 72.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button), 
                            params->font_size > 0 ? params->font_size : 12.0);
    gtk_box_pack_start(GTK_BOX(hbox), spin_button, FALSE, FALSE, 0);
    
    refs->font_size_spin = spin_button;
    
    return vbox;
}

// Страница параметров изображения
GtkWidget* create_image_parameters_page(WatermarkParams *params, WidgetReferences *refs)
{
    GtkWidget *vbox, *hbox, *label, *entry, *button;
    
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    
    // Поле для пути к изображению
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_bottom(hbox, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new("Путь к изображению:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Выберите файл изображения...");
    if (params->image_path)
        gtk_entry_set_text(GTK_ENTRY(entry), params->image_path);
    
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    g_object_set_data(G_OBJECT(refs->watermark_type_combo), "image-path-entry", entry);
    g_print("DEBUG: Image path entry created: %p\n", entry);
    
    // Кнопка обзора файлов
    button = gtk_button_new_with_label("Обзор...");
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    // Подключение обработчика клика
    g_signal_connect(button, "clicked", 
                    G_CALLBACK(on_browse_image_clicked), 
                    entry);
    
    // Добавляем подсказку
    label = gtk_label_new("Выберите файл изображения для водяного знака");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_widget_set_margin_top(label, 10);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
    
    g_print("DEBUG: Image parameters page created successfully\n");

    return vbox;
}

// Страница размещения 
GtkWidget* create_placement_page(WatermarkParams *params, WidgetReferences *refs)
{
    GtkWidget *vbox, *hbox, *label, *combo, *frame, *coord_box;
    GtkListStore *store;
    GtkCellRenderer *renderer;
    
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    // Тип размещения (единичный/мозаика)
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new("Тип размещения:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    // Создание модели для комбобокса
    store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    GtkTreeIter iter;
    
    // Добавление опций
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Единичный", 1, 0, -1);
    
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Мозаика", 1, 1, -1);
    
    combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    
    // Настройка рендерера
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), renderer, "text", 0, NULL);
    
    // Установка активного элемента
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), params->placement_type);
    gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);

    refs->placement_combo = combo;
    g_print("DEBUG: Placement combo created and stored: %p\n", refs->placement_combo);
    
    // Контейнер для параметров единичного размещения
    GtkWidget *single_frame = gtk_frame_new("Параметры единичного размещения");
    gtk_box_pack_start(GTK_BOX(vbox), single_frame, FALSE, FALSE, 5);
    
    GtkWidget *single_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(single_vbox), 10);
    gtk_container_add(GTK_CONTAINER(single_frame), single_vbox);

    // Позиционирование для единичного размещения
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(single_vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new("Позиция:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    
    // Массив вариантов позиционирования
    const gchar *positions[] = {
        "Центр", "Левый верх", "Правый верх", 
        "Левый низ", "Правый низ", "Случайно", "Точные координаты"
    };
    
    for (gint i = 0; i < 7; i++) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, positions[i], 1, i, -1);
    }

    GtkWidget *position_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(position_combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(position_combo), renderer, "text", 0, NULL);
    
    gtk_combo_box_set_active(GTK_COMBO_BOX(position_combo), params->position_type);
    gtk_box_pack_start(GTK_BOX(hbox), position_combo, TRUE, TRUE, 0);
        
    refs->position_combo = position_combo;
    g_print("DEBUG: Placement combo created and stored: %p\n", refs->placement_combo);

    // Блок точных координат для единичного размещения
    coord_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(single_vbox), coord_box, FALSE, FALSE, 0);
    
    // Координата X
    label = gtk_label_new("X:");
    gtk_box_pack_start(GTK_BOX(coord_box), label, FALSE, FALSE, 0);
    
    GtkWidget *x_spin = gtk_spin_button_new_with_range(0, 10000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(x_spin), params->custom_x);
    gtk_box_pack_start(GTK_BOX(coord_box), x_spin, FALSE, FALSE, 0);
    
    refs->custom_x_spin = x_spin;

    // Координата Y
    label = gtk_label_new("Y:");
    gtk_box_pack_start(GTK_BOX(coord_box), label, FALSE, FALSE, 0);
    
    GtkWidget *y_spin = gtk_spin_button_new_with_range(0, 10000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(y_spin), params->custom_y);
    gtk_box_pack_start(GTK_BOX(coord_box), y_spin, FALSE, FALSE, 0);
    
    refs->custom_y_spin = y_spin;

    // Блок параметров мозаики
    GtkWidget *mosaic_frame = gtk_frame_new("Параметры мозаики");
    gtk_box_pack_start(GTK_BOX(vbox), mosaic_frame, FALSE, FALSE, 5);
    
    GtkWidget *mosaic_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(mosaic_vbox), 10);
    gtk_container_add(GTK_CONTAINER(mosaic_frame), mosaic_vbox);

    // Количество строк для мозаики
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(mosaic_vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new("Количество строк:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    GtkWidget *rows_spin = gtk_spin_button_new_with_range(1, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(rows_spin), params->mosaic_rows);
    gtk_box_pack_start(GTK_BOX(hbox), rows_spin, FALSE, FALSE, 0);
    
    refs->mosaic_rows_spin = rows_spin;

    // Количество колонок для мозаики
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(mosaic_vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new("Количество колонок:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    GtkWidget *cols_spin = gtk_spin_button_new_with_range(1, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(cols_spin), params->mosaic_cols);
    gtk_box_pack_start(GTK_BOX(hbox), cols_spin, FALSE, FALSE, 0);

    refs->mosaic_cols_spin = cols_spin;

    // Расстояние между плитками для мозаики
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(mosaic_vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new("Расстояние между плитками:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);


    GtkWidget *spacing_spin = gtk_spin_button_new_with_range(0.0, 100.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spacing_spin), params->mosaic_spacing);
    gtk_box_pack_start(GTK_BOX(hbox), spacing_spin, FALSE, FALSE, 0);
    
    refs->mosaic_spacing_spin = spacing_spin;

    // Подключаем сигналы для переключения видимости
    g_signal_connect(combo, "changed", G_CALLBACK(on_placement_type_changed), vbox);
    g_signal_connect(position_combo, "changed", G_CALLBACK(on_position_type_changed), NULL);

    return vbox;
}

// Страница трансформаций
GtkWidget* create_transform_page(WatermarkParams *params, WidgetReferences *refs)
{
    GtkWidget *vbox, *hbox, *label, *spin_button, *scale_widget, *opacity_widget;
    
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    
    // Поворот
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new("Поворот (градусы):");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    spin_button = gtk_spin_button_new_with_range(-180.0, 180.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button), params->rotation_angle);
    gtk_box_pack_start(GTK_BOX(hbox), spin_button, FALSE, FALSE, 0);
    
    refs->rotation_spin = spin_button;
    
    // Масштабирование
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new("Масштаб:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    scale_widget = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.1, 3.0, 0.05);
    gtk_scale_set_value_pos(GTK_SCALE(scale_widget), GTK_POS_RIGHT);
    gtk_range_set_value(GTK_RANGE(scale_widget), 
                       params->scale_factor > 0 ? params->scale_factor : 1.0);
    gtk_box_pack_start(GTK_BOX(hbox), scale_widget, TRUE, TRUE, 0);
    
    refs->scale_scale = scale_widget;
    
    // Прозрачность
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    label = gtk_label_new("Прозрачность:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    opacity_widget = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.05);
    gtk_scale_set_value_pos(GTK_SCALE(opacity_widget), GTK_POS_RIGHT);
    gtk_range_set_value(GTK_RANGE(opacity_widget), 
                       params->opacity >= 0 ? params->opacity : 0.5);
    gtk_box_pack_start(GTK_BOX(hbox), opacity_widget, TRUE, TRUE, 0);
    
    refs->opacity_scale = opacity_widget;
    
    return vbox;
}
