# esp32_mini_tft
针对FT7735驱动的128*128 RGB_TFT做适配；
esp32模块是MINI D1 ESP32
在使用过程中发现lvgl中适配好的的TF7735驱动无法直接使用；固修改为自己的使用io方式的驱动

ST7735_DC   16
ST7735_SDA  21
ST7735_SCL  22
CS接地