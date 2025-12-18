#ifndef STEREOBM_H
#define STEREOBM_H

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

typedef struct {
  gint num_disparities;
  gint block_size;
  gint pre_filter_cap;
  gint texture_threshold;
  gint uniqueness_ratio;
} StereoBMParams;

gint compute_sad(const guchar *left_block, const guchar *right_block, 
                gint block_size, gint block_width, gint pre_filter_cap);
gint* stereobm_compute(const guchar *left_img, const guchar *right_img,
                      gint width, gint height, StereoBMParams *params);
void normalize_disparity_map(gint *disparity_map, guchar *output, 
                           gint width, gint height, gint num_disparities);
void stereobm_plugin(GimpProcedure *procedure, GimpDrawable *drawable, 
                    StereoBMParams *params);
gboolean stereobm_dialog(StereoBMParams *params);

void normalize_disparity_map_color(gint *disparity_map, guchar *output, 
                                 gint width, gint height, gint num_disparities);

#endif