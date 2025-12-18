#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <string.h>

#include "ui.h"
#include "watermark_logic.h"

// Определение констант для плагина
#define PLUG_IN_PROC   "plug-in-watermark"
#define PLUG_IN_ROLE   "watermark"

typedef struct _Watermark  Watermark;
typedef struct _WatermarkClass WatermarkClass;

// Структура экземпляра плагина
struct _Watermark
{
  GimpPlugIn      parent_instance;
};

struct _WatermarkClass
{
  GimpPlugInClass parent_class;
};

// Макросы для системы типов GObject
#define WATERMARK_TYPE  (watermark_get_type ())
G_DECLARE_FINAL_TYPE (WatermarkClass, watermark, WATERMARK,, GimpPlugIn)
G_DEFINE_TYPE (Watermark, watermark, GIMP_TYPE_PLUG_IN)

static GList * watermark_query_procedures (GimpPlugIn *plug_in);
static GimpProcedure * watermark_create_procedure (GimpPlugIn *plug_in,
                                            const gchar *name);
static GimpValueArray * watermark_run (GimpProcedure *procedure,
                                GimpRunMode run_mode,
                                GimpImage *image,
                                GimpDrawable **drawables,
                                GimpProcedureConfig *config,
                                gpointer run_data);

// Инициализация класса плагина
static void watermark_class_init (WatermarkClass *klass)
{
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);

  plug_in_class->query_procedures = watermark_query_procedures;
  plug_in_class->create_procedure = watermark_create_procedure;
}

// Инициализация экземпляра плагина
static void watermark_init (Watermark *watermark)
{
}

// Запрос процедур, предоставляемых плагином
static GList * watermark_query_procedures (GimpPlugIn *plug_in)
{
  return g_list_append (NULL, g_strdup (PLUG_IN_PROC));
}

// Создание процедуры плагина
static GimpProcedure * watermark_create_procedure (GimpPlugIn  *plug_in,
                     const gchar *name)
{
  GimpProcedure *procedure = NULL;

  if (!g_strcmp0 (name, PLUG_IN_PROC))
    {
        procedure = gimp_image_procedure_new (plug_in, name,
                                            GIMP_PDB_PROC_TYPE_PLUGIN,
                                            watermark_run, NULL, NULL);

        gimp_procedure_set_image_types (procedure, "RGB*, GRAY*");
        gimp_procedure_set_sensitivity_mask (procedure,
                                           GIMP_PROCEDURE_SENSITIVE_DRAWABLE);

        gimp_procedure_set_menu_label (procedure, "_Watermark Embedder");
        gimp_procedure_add_menu_path (procedure, "<Image>/Filters/DDDDDDDD/");

        gimp_procedure_set_documentation (procedure,
                                        "Watermark plugin",
                                        "Adds watermark to image",
                                        NULL);
        gimp_procedure_set_attribution (procedure, "Ogo",
                                        "Ogo, OGO project",
                                        "2025");
    }

  return procedure;
}

// Основная функция выполнения плагина
static GimpValueArray * watermark_run (GimpProcedure *procedure,
                                        GimpRunMode run_mode,
                                        GimpImage *image,
                                        GimpDrawable **drawables,
                                        GimpProcedureConfig *config,
                                        gpointer run_data)
{
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  GError *error = NULL;

  // Проверяем режим выполнения - поддерживаем только интерактивный
  if (run_mode != GIMP_RUN_INTERACTIVE) {
    g_set_error(&error, GIMP_PLUG_IN_ERROR, 0,
                "This plugin only supports interactive mode");
    status = GIMP_PDB_CALLING_ERROR;
    return gimp_procedure_new_return_values(procedure, status, error);
  }
    
  // Инициализируем UI
  gimp_ui_init("watermark");

  GimpDrawable *drawable;
    
  gint n_drawables = g_list_length((GList *)drawables);
  if (n_drawables != 1) {
    g_set_error(&error, GIMP_PLUG_IN_ERROR, 0,
                "Procedure requires exactly one drawable");
    status = GIMP_PDB_EXECUTION_ERROR;
    return gimp_procedure_new_return_values (procedure, status, error);
  }
  drawable = drawables[0];

  gimp_message("Watermark: Starting...");

  // Создаем параметры и показываем диалог
  WatermarkParams *params = create_watermark_params();
  gboolean success = FALSE;

  if (show_watermark_dialog(params)) {
    // Применяем водяной знак
    success = apply_watermark_to_image(image, drawable, params);
      
    if (!success) {
      status = GIMP_PDB_EXECUTION_ERROR;
      g_set_error(&error, GIMP_PLUG_IN_ERROR, 0,
                    "Failed to apply watermark");
    } else {
      gimp_message("Watermark: Successfully applied watermark");
    }

  } else {
    status = GIMP_PDB_CANCEL;
    gimp_message("Watermark: Operation cancelled by user");
  }

  free_watermark_params(params);
  return gimp_procedure_new_return_values(procedure, status, error);
}

GIMP_MAIN (WATERMARK_TYPE)