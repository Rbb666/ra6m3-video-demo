#ifndef __DRV_G2D_H__
#define __DRV_G2D_H__

#include <rtthread.h>
#include "hal_data.h"

void LCDCONF_DrawJPEG(int32_t      x,
                      int32_t      y,
                      const void *p,
                      int32_t      xSize,
                      int32_t      ySize);
#endif
