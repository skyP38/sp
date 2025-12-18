#include "stereobm.h"

// Диалог параметров
gboolean stereobm_dialog(StereoBMParams *params) {
  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *grid;
  GtkWidget *spin_button;
  gboolean run;
  
  // Инициализация UI системы GIMP
  gimp_ui_init("stereobm");
  
  // Создание диалогового окна
  dialog = gimp_dialog_new("Stereo BM Parameters", "stereobm",
                           NULL, 0,
                           NULL, NULL,
                           "Cancel", GTK_RESPONSE_CANCEL,
                           "OK", GTK_RESPONSE_OK,
                           NULL);
  
  content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  
  // Создание сетки для размещения элементов
  grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
  gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
  gtk_container_add(GTK_CONTAINER(content_area), grid);
  
  // Num Disparities
  gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Num Disparities:"),
                  0, 0, 1, 1);
  
  spin_button = gtk_spin_button_new_with_range(16, 256, 16);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button), params->num_disparities);
  gtk_grid_attach(GTK_GRID(grid), spin_button, 1, 0, 1, 1);
  g_object_set_data(G_OBJECT(dialog), "num-disparities", spin_button);
  
  // Block Size
  gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Block Size:"),
                  0, 1, 1, 1);
  
  spin_button = gtk_spin_button_new_with_range(3, 21, 2);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button), params->block_size);
  gtk_grid_attach(GTK_GRID(grid), spin_button, 1, 1, 1, 1);
  g_object_set_data(G_OBJECT(dialog), "block-size", spin_button);
  
  // Pre Filter Cap
  gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Pre Filter Cap:"),
                  0, 2, 1, 1);
  
  spin_button = gtk_spin_button_new_with_range(1, 63, 1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button), params->pre_filter_cap);
  gtk_grid_attach(GTK_GRID(grid), spin_button, 1, 2, 1, 1);
  g_object_set_data(G_OBJECT(dialog), "pre-filter-cap", spin_button);
  
  // Texture Threshold
  gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Texture Threshold:"),
                  0, 3, 1, 1);
  
  spin_button = gtk_spin_button_new_with_range(0, 1000, 10);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button), params->texture_threshold);
  gtk_grid_attach(GTK_GRID(grid), spin_button, 1, 3, 1, 1);
  g_object_set_data(G_OBJECT(dialog), "texture-threshold", spin_button);
  
  // Uniqueness Ratio
  gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Uniqueness Ratio:"),
                  0, 4, 1, 1);
  
  spin_button = gtk_spin_button_new_with_range(0, 100, 5);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button), params->uniqueness_ratio);
  gtk_grid_attach(GTK_GRID(grid), spin_button, 1, 4, 1, 1);
  g_object_set_data(G_OBJECT(dialog), "uniqueness-ratio", spin_button);
  
  gtk_widget_show_all(dialog);
  
  run = (gimp_dialog_run(GIMP_DIALOG(dialog)) == GTK_RESPONSE_OK);
  
  if (run) {
    GtkWidget *widget;
    
    widget = g_object_get_data(G_OBJECT(dialog), "num-disparities");
    params->num_disparities = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
    
    widget = g_object_get_data(G_OBJECT(dialog), "block-size");
    params->block_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
    
    widget = g_object_get_data(G_OBJECT(dialog), "pre-filter-cap");
    params->pre_filter_cap = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
    
    widget = g_object_get_data(G_OBJECT(dialog), "texture-threshold");
    params->texture_threshold = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
    
    widget = g_object_get_data(G_OBJECT(dialog), "uniqueness-ratio");
    params->uniqueness_ratio = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
  }
  
  gtk_widget_destroy(dialog);
  
  return run;
}