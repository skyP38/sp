#include "ui.h"
#include "ui_pages.h"

static void save_all_values(WatermarkParams *params, WidgetReferences *refs);
static void save_type_page_values(GtkWidget *notebook, WatermarkParams *params);
static void save_text_page_values(GtkWidget *notebook, WatermarkParams *params);
static void save_image_page_values(GtkWidget *notebook, WatermarkParams *params);
static void save_placement_page_values(GtkWidget *notebook, WatermarkParams *params);
static void save_transform_page_values(GtkWidget *notebook, WatermarkParams *params);

static void update_page_visibility(GtkNotebook *notebook, gint watermark_type);

WidgetReferences refs = {
    .text_entry = NULL,
    .font_button = NULL,
    .font_size_spin = NULL,
    .watermark_type_combo = NULL,
    .opacity_scale = NULL,
    .rotation_spin = NULL,
    .scale_scale = NULL,
    .position_combo = NULL,
    .custom_x_spin = NULL,
    .custom_y_spin = NULL,
    .placement_combo = NULL,
    .mosaic_rows_spin = NULL,      
    .mosaic_cols_spin = NULL,      
    .mosaic_spacing_spin = NULL   
};

// Создание главного диалогового окна
gboolean show_watermark_dialog(WatermarkParams *params)
{
    g_print("Watermark: show_watermark_dialog called\n");
    
    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *notebook;
    GtkWidget *type_page, *placement_page, *transform_page;
    GtkWidget *text_page = NULL, *image_page = NULL;
    gint response;
    
    // Создание диалогового окна
    dialog = gtk_dialog_new_with_buttons("Watermark Plugin",
                                   NULL,
                                   GTK_DIALOG_MODAL,
                                   "_Cancel", GTK_RESPONSE_CANCEL,
                                   "_OK", GTK_RESPONSE_OK,
                                   NULL);
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    // Создание notebook с вкладками
    notebook = gtk_notebook_new();
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 5);
    gtk_box_pack_start(GTK_BOX(content_area), notebook, TRUE, TRUE, 0);
    
    g_object_set_data(G_OBJECT(dialog), "watermark-notebook", notebook);
    
    // Создание страниц
    type_page = create_type_selection_page(params, &refs);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), type_page, 
                           gtk_label_new("Тип водяного знака"));
    
    // Страницы для текста и изображения 
    text_page = create_text_parameters_page(params, &refs);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), text_page, 
                           gtk_label_new("Текст"));

    image_page = create_image_parameters_page(params, &refs);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), image_page, 
                           gtk_label_new("Изображение"));
    
    placement_page = create_placement_page(params, &refs);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), placement_page, 
                           gtk_label_new("Размещение"));
    
    transform_page = create_transform_page(params, &refs);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), transform_page, 
                           gtk_label_new("Трансформация"));
    
    // Настройка обработчика изменения типа водяного знака
    if (refs.watermark_type_combo) {
        g_object_set_data(G_OBJECT(refs.watermark_type_combo), "watermark-params", params);
        g_object_set_data(G_OBJECT(refs.watermark_type_combo), "watermark-notebook", notebook);
        g_signal_connect(refs.watermark_type_combo, "changed", G_CALLBACK(on_watermark_type_changed), notebook);
        g_print("DEBUG: Signal connected to type combo: %p\n", refs.watermark_type_combo);
    } else {
        g_print("ERROR: watermark_type_combo is NULL!\n");
    }
    
    // Показываем все виджеты
    gtk_widget_show_all(dialog);

    // Обновляем видимость страниц после показа
    update_page_visibility(GTK_NOTEBOOK(notebook), params->watermark_type);

    // Настройка видимости фреймов размещения
    GtkWidget *placement_page_one = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 3);
    if (refs.placement_combo) {
        // Вызываем обработчик чтобы установить правильную видимость
        on_placement_type_changed(GTK_COMBO_BOX(refs.placement_combo), placement_page_one);
    }
    
    // Установка первой страницы активной
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);

    // Обрабатываем ответ
    response = gtk_dialog_run(GTK_DIALOG(dialog));

    // Сохраняем значения если нажали OK
    if (response == GTK_RESPONSE_OK) {
        g_print("DEBUG: Before save_all_values - combo value: %d\n", 
           gtk_combo_box_get_active(GTK_COMBO_BOX(refs.watermark_type_combo)));
        g_print("DEBUG: Before save_all_values - params->watermark_type: %d\n", 
           params->watermark_type);
        save_all_values(params, &refs);
        g_print("Watermark: Dialog values saved\n");
    }

    gtk_widget_destroy(dialog);
    
    return (response == GTK_RESPONSE_OK);
}


// Обработчик изменения типа водяного знака
void on_watermark_type_changed(GtkComboBox *combo, gpointer user_data)
{
    GtkNotebook *notebook = GTK_NOTEBOOK(user_data);
    gint active = gtk_combo_box_get_active(combo);
    
    g_print("Watermark type changed to: %d\n", active);

    WatermarkParams *params = g_object_get_data(G_OBJECT(combo), "watermark-params");
    if (params) {
        params->watermark_type = active;
        g_print("DEBUG: Updated params->watermark_type to: %d\n", params->watermark_type);
    }
    update_page_visibility(notebook, active);
}

// Обработчик изменения типа размещения
void on_placement_type_changed(GtkComboBox *combo, gpointer user_data)
{
    GtkWidget *vbox = GTK_WIDGET(user_data);
    GtkWidget *mosaic_frame = NULL, *single_frame = NULL;
    
    // Ищем фрейм мозаики
    GList *children = gtk_container_get_children(GTK_CONTAINER(vbox));
    for (GList *iter = children; iter != NULL; iter = iter->next) {
        if (GTK_IS_FRAME(iter->data)) {
            GtkWidget *frame = GTK_WIDGET(iter->data);
            const gchar *label = gtk_frame_get_label(GTK_FRAME(frame));
            if (label && g_strcmp0(label, "Параметры мозаики") == 0) {
                mosaic_frame = frame;
            } 
            else if (label && g_strcmp0(label, "Параметры единичного размещения") == 0) {
                single_frame = frame;
            }
        }
    }
    g_list_free(children);
    
    // Управление видимостью фреймов в зависимости от выбранного типа
    if (mosaic_frame) {
        gint active = gtk_combo_box_get_active(combo);
        if (active == 1) { // Мозаика
            gtk_widget_hide(single_frame);
            gtk_widget_show(mosaic_frame);
        } else { // Единичный
            gtk_widget_show(single_frame);
            gtk_widget_hide(mosaic_frame);
        }
    }
}

// Обработчик изменения типа позиции (при единичном размещении)
void on_position_type_changed(GtkComboBox *combo, gpointer user_data)
{
    GtkWidget *parent = gtk_widget_get_parent(GTK_WIDGET(combo));
    GtkWidget *coord_box = NULL;
    GList *children = gtk_container_get_children(GTK_CONTAINER(parent));
    
    // Ищем блок координат
    for (GList *iter = children; iter != NULL; iter = iter->next) {
        if (GTK_IS_BOX(iter->data)) {
            coord_box = GTK_WIDGET(iter->data);
            break;
        }
    }
    
    g_list_free(children);
    
    // Управление видимостью блока координат
    if (coord_box) {
        gint active = gtk_combo_box_get_active(combo);
        if (active == 6) { // Точные координаты
            gtk_widget_show(coord_box);
        } else {
            gtk_widget_hide(coord_box);
        }
    }
}

// Обработчик сигнала добавления изображения
void on_browse_image_clicked(GtkButton *button, gpointer user_data)
{
    GtkWidget *entry = GTK_WIDGET(user_data);
    GtkWidget *dialog;
    GtkFileFilter *filter;

    dialog = gtk_file_chooser_dialog_new("Выберите изображение",
                                       NULL,
                                       GTK_FILE_CHOOSER_ACTION_OPEN,
                                       "_Cancel", GTK_RESPONSE_CANCEL,
                                        "_Open", GTK_RESPONSE_ACCEPT,
                                       NULL);
    
    // Добавляем фильтр для изображений
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Изображения");
    gtk_file_filter_add_pattern(filter, "*.png");
    gtk_file_filter_add_pattern(filter, "*.jpg");
    gtk_file_filter_add_pattern(filter, "*.jpeg");
    gtk_file_filter_add_pattern(filter, "*.gif");
    gtk_file_filter_add_pattern(filter, "*.bmp");
    gtk_file_filter_add_pattern(filter, "*.tiff");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    
    // Добавляем фильтр для всех файлов
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Все файлы");
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    // Обработка выбора файла
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *filename;
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(entry), filename);
        g_print("DEBUG: Selected image file: %s\n", filename);
        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
}

// Функция сохранения всех значений
static void save_all_values(WatermarkParams *params, WidgetReferences *refs)
{
    // Тип водяного знака
    if (refs->watermark_type_combo) {
        params->watermark_type = gtk_combo_box_get_active(GTK_COMBO_BOX(refs->watermark_type_combo));
        g_print("Saved watermark type: %d\n", params->watermark_type);
    }

    // Тип размещения
    if (refs->placement_combo) {
        params->placement_type = gtk_combo_box_get_active(GTK_COMBO_BOX(refs->placement_combo));
        g_print("Saved placement type: %d\n", params->placement_type);
    }

    // Сохраняем путь к изображению (если выбран тип "Изображение")
    if (params->watermark_type == 1) {
        GtkWidget *image_entry = g_object_get_data(G_OBJECT(refs->watermark_type_combo), "image-path-entry");
        if (image_entry && GTK_IS_ENTRY(image_entry)) {
            const gchar *image_path = gtk_entry_get_text(GTK_ENTRY(image_entry));
            g_free(params->image_path);
            params->image_path = g_strdup(image_path);
            g_print("Saved image path: '%s'\n", params->image_path ? params->image_path : "NULL");
            
            // Проверяем существование файла
            if (params->image_path && strlen(params->image_path) > 0) {
                if (!g_file_test(params->image_path, G_FILE_TEST_EXISTS)) {
                    g_print("WARNING: Image file does not exist: %s\n", params->image_path);
                }
            } else {
                g_print("WARNING: Image path is empty!\n");
            }
        } else {
            g_print("ERROR: Image path entry not found! Pointer: %p\n", image_entry);
        }
    }

    // Текстовые параметры 
    if (params->watermark_type == 0 && refs->text_entry) {
        const gchar *new_text = gtk_entry_get_text(GTK_ENTRY(refs->text_entry));
        g_free(params->text_content);
        params->text_content = g_strdup(new_text);
        g_print("Saved text: '%s'\n", params->text_content);
    }
        
    if (refs->font_button) {
        gchar *font_desc = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(refs->font_button));
        g_free(params->font_name);
        params->font_name = g_strdup(font_desc);
        g_free(font_desc);
        g_print("Saved font: %s\n", params->font_name);
    }
    
    if (refs->font_size_spin) {
        params->font_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(refs->font_size_spin));
        g_print("Saved font size: %.1f\n", params->font_size);
    }
    
    // Прозрачность
    if (refs->opacity_scale) {
        params->opacity = gtk_range_get_value(GTK_RANGE(refs->opacity_scale));
        g_print("Saved opacity: %.1f\n", params->opacity);
    }
    
    // Поворот
    if (refs->rotation_spin) {
        params->rotation_angle = gtk_spin_button_get_value(GTK_SPIN_BUTTON(refs->rotation_spin));
        g_print("Saved rotation: %.1f\n", params->rotation_angle);
    }
    
    // Масштаб
    if (refs->scale_scale) {
        params->scale_factor = gtk_range_get_value(GTK_RANGE(refs->scale_scale));
        g_print("Saved scale: %.1f\n", params->scale_factor);
    }
    
    // Позиционирование
    if (refs->position_combo) {
        params->position_type = gtk_combo_box_get_active(GTK_COMBO_BOX(refs->position_combo));
        g_print("Saved position: %d\n", params->position_type);
    }
    
    // Параметры мозаики
    if (refs->mosaic_rows_spin) {
        params->mosaic_rows = gtk_spin_button_get_value(GTK_SPIN_BUTTON(refs->mosaic_rows_spin));
        g_print("Saved mosaic rows: %d\n", params->mosaic_rows);
    } else {
        g_print("ERROR: mosaic_rows_spin not found!\n");
    }
    
    if (refs->mosaic_cols_spin) {
        params->mosaic_cols = gtk_spin_button_get_value(GTK_SPIN_BUTTON(refs->mosaic_cols_spin));
        g_print("Saved mosaic cols: %d\n", params->mosaic_cols);
    } else {
        g_print("ERROR: mosaic_cols_spin not found!\n");
    }

    if (refs->mosaic_spacing_spin) {
        params->mosaic_spacing = gtk_spin_button_get_value(GTK_SPIN_BUTTON(refs->mosaic_spacing_spin));
        g_print("Saved mosaic spacing: %.1f\n", params->mosaic_spacing);
    }
    
    // Точные координаты
    if (refs->custom_x_spin) {
        params->custom_x = gtk_spin_button_get_value(GTK_SPIN_BUTTON(refs->custom_x_spin));
        g_print("Saved custom X: %d\n", params->custom_x);
    }
    
    if (refs->custom_y_spin) {
        params->custom_y = gtk_spin_button_get_value(GTK_SPIN_BUTTON(refs->custom_y_spin));
        g_print("Saved custom Y: %d\n", params->custom_y);
    }
}

// Функции сохранения для каждой страницы
static void save_type_page_values(GtkWidget *notebook, WatermarkParams *params)
{
    GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0);
    if (!page) return;
    
    GtkWidget *combo = g_object_get_data(G_OBJECT(page), "watermark-type-combo");
    if (combo) {
        params->watermark_type = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
        g_print("Saved watermark type: %d\n", params->watermark_type);
    }
}

// Функции сохранения для текстовой
static void save_text_page_values(GtkWidget *notebook, WatermarkParams *params)
{
    GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 1);
    if (!page) {
        g_print("ERROR: Text page not found!\n");
        return;
    }

    g_print("Text page found: %p\n", page);

    // Текст
    GtkWidget *entry = g_object_get_data(G_OBJECT(page), "text-content");
    if (entry) {
        const gchar *new_text = gtk_entry_get_text(GTK_ENTRY(entry));
        g_print("Raw text from entry: '%s'\n", new_text);
        g_free(params->text_content);
        params->text_content = g_strdup(new_text);
        g_print("Saved text: '%s'\n", params->text_content);
    } else {
        g_print("ERROR: Text entry not found!\n");
    }
    
    // Шрифт
    GtkWidget *font_button = g_object_get_data(G_OBJECT(page), "font-name");
    if (font_button) {
        gchar *font_desc = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(font_button));
        g_free(params->font_name);
        params->font_name = g_strdup(font_desc);
        g_free(font_desc);
        g_print("Saved font: %s\n", params->font_name);
    } else {
        g_print("ERROR: Font button not found!\n");
    }
    
    // Размер шрифта
    GtkWidget *spin = g_object_get_data(G_OBJECT(page), "font-size");
    if (spin) {
        params->font_size = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
        g_print("Saved font size: %.1f\n", params->font_size);
    } else {
        g_print("ERROR: Font size spin not found!\n");
    }
}

// Функции сохранения для страницы изображения
static void save_image_page_values(GtkWidget *notebook, WatermarkParams *params)
{
    GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 2);
    GtkWidget *entry = g_object_get_data(G_OBJECT(page), "image-path");
    
    if (entry) {
        g_free(params->image_path);
        params->image_path = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    }
}

// Функции сохранения для страницы размещения
static void save_placement_page_values(GtkWidget *notebook, WatermarkParams *params)
{
    GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 3);
    
    // Тип размещения
    GtkWidget *combo = g_object_get_data(G_OBJECT(page), "placement-type");
    if (combo) {
        params->placement_type = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    }
    
    // Тип позиции
    combo = g_object_get_data(G_OBJECT(page), "position-type");
    if (combo) {
        params->position_type = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    }
    
    // Координаты
    GtkWidget *spin = g_object_get_data(G_OBJECT(page), "custom-x");
    if (spin) {
        params->custom_x = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
    }
    
    spin = g_object_get_data(G_OBJECT(page), "custom-y");
    if (spin) {
        params->custom_y = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
    }
}

// Функции сохранения для страницы трансформаций
static void save_transform_page_values(GtkWidget *notebook, WatermarkParams *params)
{
    GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 4);
    
    // Поворот
    GtkWidget *spin = g_object_get_data(G_OBJECT(page), "rotation-angle");
    if (spin) {
        params->rotation_angle = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
    }
    
    // Масштаб
    GtkWidget *scale = g_object_get_data(G_OBJECT(page), "scale-factor");
    if (scale) {
        params->scale_factor = gtk_range_get_value(GTK_RANGE(scale));
    }

    // Прозрачность
    GtkWidget *opacity = g_object_get_data(G_OBJECT(page), "opacity");
    if (opacity) {
        params->opacity = gtk_range_get_value(GTK_RANGE(opacity));
    }
}

// Функция обновления видимости страниц
static void update_page_visibility(GtkNotebook *notebook, gint watermark_type)
{
    // Страница 1 - текст, страница 2 - изображение
    GtkWidget *text_page = gtk_notebook_get_nth_page(notebook, 1);
    GtkWidget *image_page = gtk_notebook_get_nth_page(notebook, 2);
    
    if (watermark_type == 0) { // Текст
        if (text_page) {
            gtk_widget_show(text_page);
            g_print("DEBUG: Showing text page\n");
        }
        if (image_page) {
            gtk_widget_hide(image_page);
            g_print("DEBUG: Hiding image page\n");
        }
         // Обновляем метку вкладки
        gtk_notebook_set_tab_label_text(GTK_NOTEBOOK(notebook), text_page, "Текст");
    } else { // Изображение
        if (text_page) {
            gtk_widget_hide(text_page);
            g_print("DEBUG: Hiding text page\n");
        }
        if (image_page) {
            gtk_widget_show(image_page);
            g_print("DEBUG: Showing image page\n");
        }
        // Обновляем метку вкладки  
        gtk_notebook_set_tab_label_text(GTK_NOTEBOOK(notebook), image_page, "Изображение");
    }
}

// Функция для инициализации параметров
WatermarkParams* create_watermark_params(void)
{
    WatermarkParams *params = g_new0(WatermarkParams, 1);
    
    params->watermark_type = 0;
    params->text_content = g_strdup("Watermark");
    params->font_name = g_strdup("Sans");
    params->font_size = 24.0;
        
    params->placement_type = 1;
    params->position_type = 6;
    params->custom_x = 0;
    params->custom_y = 0;
    
    params->mosaic_rows = 3;
    params->mosaic_cols = 3;
    params->mosaic_spacing = 10.0;
    
    params->rotation_angle = 0.0;
    params->scale_factor = 1.0;
    
    params->opacity = 0.7;

    return params;
}

// Функция освобождения параметров
void free_watermark_params(WatermarkParams *params)
{
    if (!params) return;
    
    g_free(params->text_content);
    g_free(params->font_name);
    g_free(params->image_path);
    g_free(params);
}