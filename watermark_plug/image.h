#ifndef IMAGE_H
#define IMAGE_H

#include <libgimp/gimp.h>
#include "watermark_logic.h"

GimpLayer* create_image_watermark_layer(GimpImage *image, WatermarkParams *params);
gboolean apply_single_image_watermark(GimpImage *image, GimpDrawable *drawable, WatermarkParams *params);


#endif