#ifndef ST7735_H
#define ST7735_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#define ST7735_DC   16
#define ST7735_RST  (-1)

#define ST7735_SDA   (21)
#define ST7735_SCL   (22)
#define ST7735_CS    (-1)

void st7735_init(void);
void st7735_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif