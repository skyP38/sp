#ifndef MOSAIC_H
#define MOSAIC_H

#include <libgimp/gimp.h>
#include "watermark_logic.h"

gboolean apply_mosaic_watermark(GimpImage *image, GimpDrawable *drawable, WatermarkParams *params);

#endif