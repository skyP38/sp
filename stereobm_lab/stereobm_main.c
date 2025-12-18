#include "stereobm.h"

#define PLUG_IN_PROC   "plug-in-stereobm"
#define PLUG_IN_ROLE   "stereobm"

typedef struct _StereoBM  StereoBM;
typedef struct _StereoBMClass StereoBMClass;

// Структура экземпляра плагина
struct _StereoBM
{
  GimpPlugIn      parent_instance;
};

// Структура класса плагина
struct _StereoBMClass
{
  GimpPlugInClass parent_class;
};

// Макросы для работы с системой типов GLib
#define STEREOBM_TYPE  (stereobm_get_type ())
G_DECLARE_FINAL_TYPE (StereoBMClass, stereobm, STEREOBM,, GimpPlugIn)
G_DEFINE_TYPE (StereoBM, stereobm, GIMP_TYPE_PLUG_IN)

static GList * stereobm_query_procedures (GimpPlugIn *plug_in);
static GimpProcedure * stereobm_create_procedure (GimpPlugIn *plug_in,
                                            const gchar *name);
static GimpValueArray * stereobm_run (GimpProcedure *procedure,
                                GimpRunMode run_mode,
                                GimpImage *image,
                                GimpDrawable **drawables,
                                GimpProcedureConfig *config,
                                gpointer run_data);

                                // Инициализация класса плагина
static void stereobm_class_init (StereoBMClass *klass)
{
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);

  plug_in_class->query_procedures = stereobm_query_procedures;
  plug_in_class->create_procedure = stereobm_create_procedure;
}

// Инициализация экземпляра плагина
static void stereobm_init (StereoBM *stereobm)
{
}

// Запрос списка процедур, предоставляемых плагином
static GList * stereobm_query_procedures (GimpPlugIn *plug_in)
{
  return g_list_append (NULL, g_strdup (PLUG_IN_PROC));
}

// Создание процедуры плагина
static GimpProcedure * stereobm_create_procedure (GimpPlugIn  *plug_in,
                     const gchar *name)
{
  GimpProcedure *procedure = NULL;

  if (!g_strcmp0 (name, PLUG_IN_PROC))
    {
        // Создание новой процедуры GIMP
        procedure = gimp_image_procedure_new (plug_in, name,
                                            GIMP_PDB_PROC_TYPE_PLUGIN,
                                            stereobm_run, NULL, NULL);

        // Настройка параметров процедуры
        gimp_procedure_set_image_types (procedure, "RGB*, GRAY*");
        gimp_procedure_set_sensitivity_mask (procedure,
                                           GIMP_PROCEDURE_SENSITIVE_DRAWABLE);

        // Настройка меню
        gimp_procedure_set_menu_label (procedure, "_Stereo BM");
        gimp_procedure_add_menu_path (procedure, "<Image>/Filters/SSSSSSSSS");

        // Документация
        gimp_procedure_set_documentation (procedure,
                                        "Stereo Block Matching",
                                        "Computes disparity map using Stereo Block Matching with SAD",
                                        NULL);
        gimp_procedure_set_attribution (procedure, "Ogo",
                                        "Ogo, OGO project",
                                        "2025");
    }

  return procedure;
}

// Основная функция выполнения плагина
static GimpValueArray * stereobm_run (GimpProcedure *procedure,
                                        GimpRunMode run_mode,
                                        GimpImage *image,
                                        GimpDrawable **drawables,
                                        GimpProcedureConfig *config,
                                        gpointer run_data)
{
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  GError *error = NULL;

  // Проверяем режим выполнения - поддержка только интерактивного
  if (run_mode != GIMP_RUN_INTERACTIVE) {
    g_set_error(&error, GIMP_PLUG_IN_ERROR, 0,
                "This plugin only supports interactive mode");
    status = GIMP_PDB_CALLING_ERROR;
    return gimp_procedure_new_return_values(procedure, status, error);
  }
    
  // Инициализация UI
  gimp_ui_init("stereobm");

  GimpDrawable *drawable;
    
  gint n_drawables = g_list_length((GList *)drawables);
  if (n_drawables != 1) {
    g_set_error(&error, GIMP_PLUG_IN_ERROR, 0,
                "Procedure requires exactly one drawable");
    status = GIMP_PDB_EXECUTION_ERROR;
    return gimp_procedure_new_return_values (procedure, status, error);
  }
  drawable = drawables[0];

  gimp_progress_init("Computing Stereo BM");

  StereoBMParams params = {64, 15, 31, 10, 15};

  // Отображение диалога параметров
  if (stereobm_dialog(&params)) {
    // Вычисление карты диспаратности
    stereobm_plugin(procedure, drawable, &params);
    gimp_message("Stereo BM: Successfully computed disparity map");
  } else {
    status = GIMP_PDB_CANCEL;
    gimp_message("Stereo BM: Operation cancelled by user");
  }

  return gimp_procedure_new_return_values(procedure, status, error);
}

GIMP_MAIN (STEREOBM_TYPE)