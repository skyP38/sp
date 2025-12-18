#ifndef TEXT_H
#define TEXT_H

#include <libgimp/gimp.h>
#include "watermark_logic.h"

GimpLayer* create_text_watermark_layer(GimpImage *image, WatermarkParams *params);
gboolean apply_single_text_watermark(GimpImage *image, GimpDrawable *drawable, WatermarkParams *params);


#endif