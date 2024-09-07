#include "st7735.h"
#include "disp_spi.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "ST7735"
#define cbi(reg, bitmask) gpio_set_level(reg, 0)
#define sbi(reg, bitmask) gpio_set_level(reg, 1)
#define pulse_high(reg, bitmask) \
    sbi(reg, bitmask);           \
    cbi(reg, bitmask);
#define pulse_low(reg, bitmask) \
    cbi(reg, bitmask);          \
    sbi(reg, bitmask);

#define swap_utft(type, i, j) {type t = i; i = j; j = t;}
#define ST7735_MADCTL       0x36

// #define COLSTART            26
// #define ROWSTART            1

#define COLSTART            0
#define ROWSTART            0

typedef struct
{
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; // No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

static void st7735_send_cmd(uint8_t cmd);
static void st7735_send_data(void *data, uint16_t length);
static void st7735_send_color(void *data, uint16_t length);
static void clrScr();
void setColor(int r, int g, int b);
void fillRect(int x1, int y1, int x2, int y2);
static int display_transfer_mode = 1;
static int st7735_portrait_mode = 0;

static int disp_x_size = 127;
static int disp_y_size = 127;
static int fcolorr,fcolorg,fcolorb;

static void LCD_Writ_Bus(char VH, char VL, int mode)
{
    switch (mode)
    {
    case 1:

        if (VH == 1)
            gpio_set_level(ST7735_DC, 1);
        else
            gpio_set_level(ST7735_DC, 0);

        if (VL & 0x80)
            gpio_set_level(ST7735_SDA, 1);
        else
            gpio_set_level(ST7735_SDA, 0);
        pulse_low(ST7735_SCL, 0);

        if (VL & 0x40)
            gpio_set_level(ST7735_SDA, 1);
        else
            gpio_set_level(ST7735_SDA, 0);
        pulse_low(ST7735_SCL, 0);

        if (VL & 0x20)
            gpio_set_level(ST7735_SDA, 1);
        else
            gpio_set_level(ST7735_SDA, 0);
        pulse_low(ST7735_SCL, 0);

        if (VL & 0x10)
            gpio_set_level(ST7735_SDA, 1);
        else
            gpio_set_level(ST7735_SDA, 0);
        pulse_low(ST7735_SCL, 0);

        if (VL & 0x08)
            gpio_set_level(ST7735_SDA, 1);
        else
            gpio_set_level(ST7735_SDA, 0);
        pulse_low(ST7735_SCL, 0);

        if (VL & 0x04)
            gpio_set_level(ST7735_SDA, 1);
        else
            gpio_set_level(ST7735_SDA, 0);
        pulse_low(ST7735_SCL, 0);

        if (VL & 0x02)
            gpio_set_level(ST7735_SDA, 1);
        else
            gpio_set_level(ST7735_SDA, 0);
        pulse_low(ST7735_SCL, 0);

        if (VL & 0x01)
            gpio_set_level(ST7735_SDA, 1);
        else
            gpio_set_level(ST7735_SDA, 0);
        pulse_low(ST7735_SCL, 0);

        break;

    default:
        printf("mode error\r\n");
        break;
    }
}

static void LCD_Write_DATA(char VL)
{
    LCD_Writ_Bus(0x01, VL, display_transfer_mode);
}

static void LCD_Write_DATA_2(char VH, char VL)
{
    LCD_Writ_Bus(0x01, VH, display_transfer_mode);
    LCD_Writ_Bus(0x01, VL, display_transfer_mode);
}

static void LCD_Write_COM(char VL)
{
    LCD_Writ_Bus(0x00, VL, display_transfer_mode);
}

static void st7735_send_cmd(uint8_t cmd)
{
    LCD_Write_COM(cmd);
}

static void st7735_send_data(void *data, uint16_t length)
{
    char *chardata = (char *)data;
    char sendonedata = 0;

    for (int i = 0; i < length; i++)
    {
        sendonedata = chardata[i];
        LCD_Write_DATA(sendonedata);
    }
}

static void st7735_set_orientation(uint8_t orientation)
{
    const char *orientation_str[] = {
        "PORTRAIT", "PORTRAIT_INVERTED", "LANDSCAPE", "LANDSCAPE_INVERTED"
    };

    ESP_LOGD(TAG, "Display orientation: %s", orientation_str[orientation]);

    /*
        Portrait:  0xC8 = ST77XX_MADCTL_MX | ST77XX_MADCTL_MY | ST77XX_MADCTL_BGR
        Landscape: 0xA8 = ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | ST77XX_MADCTL_BGR
        Remark: "inverted" is ignored here
    */
    uint8_t data[] = {0xC8, 0xC8, 0xA8, 0xA8};

    ESP_LOGD(TAG, "0x36 command value: 0x%02X", data[orientation]);

    st7735_send_cmd(ST7735_MADCTL);
    st7735_send_data((void *) &data[orientation], 1);
}

void st7735_init(void)
{
    //     lcd_init_cmd_t init_cmds[] = {
    //         {ST7735_SWRESET, {0}, 0x80},                               // Software reset, 0 args, w/delay 150
    //         {ST7735_SLPOUT, {0}, 0x80},                                // Out of sleep mode, 0 args, w/delay 500
    //         {ST7735_FRMCTR1, {0x01, 0x2C, 0x2D}, 3},                   // Frame rate ctrl - normal mode, 3 args: Rate = fosc/(1x2+40) * (LINE+2C+2D)
    //         {ST7735_FRMCTR2, {0x01, 0x2C, 0x2D}, 3},                   // Frame rate control - idle mode, 3 args:Rate = fosc/(1x2+40) * (LINE+2C+2D)
    //         {ST7735_FRMCTR3, {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D}, 6}, // Frame rate ctrl - partial mode, 6 args:Dot inversion mode. Line inversion mode
    //         {ST7735_INVCTR, {0x07}, 1},                                // Display inversion ctrl, 1 arg, no delay:No inversion
    //         {ST7735_PWCTR1, {0xA2, 0x02, 0x84}, 3},                    // Power control, 3 args, no delay:-4.6V AUTO mode
    //         {ST7735_PWCTR2, {0xC5}, 1},                                // Power control, 1 arg, no delay:VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
    //         {ST7735_PWCTR3, {0x0A, 0x00}, 2},                          // Power control, 2 args, no delay: Opamp current small, Boost frequency
    //         {ST7735_PWCTR4, {0x8A, 0x2A}, 2},                          // Power control, 2 args, no delay: BCLK/2, Opamp current small & Medium low
    //         {ST7735_PWCTR5, {0x8A, 0xEE}, 2},                          // Power control, 2 args, no delay:
    //         {ST7735_VMCTR1, {0x0E}, 1},                                // Power control, 1 arg, no delay:
    // #if ST7735S_INVERT_COLORS == 1
    //         {ST7735_INVON, {0}, 0}, // set inverted mode
    // #else
    //         {ST7735_INVOFF, {0}, 0}, // set non-inverted mode
    // #endif
    //         {ST7735_COLMOD, {0x05}, 1},                                                                                             // set color mode, 1 arg, no delay: 16-bit color
    //         {ST7735_GMCTRP1, {0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10}, 16}, // 16 args, no delay:
    //         {ST7735_GMCTRN1, {0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10}, 16}, // 16 args, no delay:
    //         {ST7735_NORON, {0}, TFT_INIT_DELAY},                                                                                    // Normal display on, no args, w/delay 10 ms delay
    //         {ST7735_DISPON, {0}, TFT_INIT_DELAY},                                                                                   // Main screen turn on, no args w/delay 100 ms delay
    //         {0, {0}, 0xff}};

    // Initialize non-SPI GPIOs
    gpio_pad_select_gpio(ST7735_DC);
    gpio_set_direction(ST7735_DC, GPIO_MODE_OUTPUT);

    // gpio_pad_select_gpio(ST7735_RST);
    // gpio_set_direction(ST7735_RST, GPIO_MODE_OUTPUT);

    // gpio_pad_select_gpio(ST7735_CS);
    // gpio_set_direction(ST7735_CS, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(ST7735_SDA);
    gpio_set_direction(ST7735_SDA, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(ST7735_SCL);
    gpio_set_direction(ST7735_SCL, GPIO_MODE_OUTPUT);

    // Reset the display
    // gpio_set_level(ST7735_RST, 1);
    // vTaskDelay(5 / portTICK_RATE_MS);
    // gpio_set_level(ST7735_RST, 0);
    // vTaskDelay(15 / portTICK_RATE_MS);
    // gpio_set_level(ST7735_RST, 1);
    // vTaskDelay(15 / portTICK_RATE_MS);

    // gpio_set_level(ST7735_CS, 0);

    ESP_LOGI(TAG, "ST7735 initialization.");

    // //Send all the commands
    // uint16_t cmd = 0;
    // while (init_cmds[cmd].databytes!=0xff) {
    //     st7735_send_cmd(init_cmds[cmd].cmd);
    //     st7735_send_data(init_cmds[cmd].data, init_cmds[cmd].databytes&0x1F);
    //     if (init_cmds[cmd].databytes & 0x80) {
    //         vTaskDelay(100 / portTICK_RATE_MS);
    //     }
    //     cmd++;
    // }

    LCD_Write_COM(0x11); // Sleep exit
    vTaskDelay(12 / portTICK_RATE_MS);

    // ST7735R Frame Rate
    LCD_Write_COM(0xB1);
    LCD_Write_DATA(0x01);
    LCD_Write_DATA(0x2C);
    LCD_Write_DATA(0x2D);
    LCD_Write_COM(0xB2);
    LCD_Write_DATA(0x01);
    LCD_Write_DATA(0x2C);
    LCD_Write_DATA(0x2D);
    LCD_Write_COM(0xB3);
    LCD_Write_DATA(0x01);
    LCD_Write_DATA(0x2C);
    LCD_Write_DATA(0x2D);
    LCD_Write_DATA(0x01);
    LCD_Write_DATA(0x2C);
    LCD_Write_DATA(0x2D);

    LCD_Write_COM(0xB4); // Column inversion
    LCD_Write_DATA(0x07);

    // ST7735R Power Sequence
    LCD_Write_COM(0xC0);
    LCD_Write_DATA(0xA2);
    LCD_Write_DATA(0x02);
    LCD_Write_DATA(0x84);
    LCD_Write_COM(0xC1);
    LCD_Write_DATA(0xC5);
    LCD_Write_COM(0xC2);
    LCD_Write_DATA(0x0A);
    LCD_Write_DATA(0x00);
    LCD_Write_COM(0xC3);
    LCD_Write_DATA(0x8A);
    LCD_Write_DATA(0x2A);
    LCD_Write_COM(0xC4);
    LCD_Write_DATA(0x8A);
    LCD_Write_DATA(0xEE);

    LCD_Write_COM(0xC5); // VCOM
    LCD_Write_DATA(0x0E);

    // LCD_Write_COM(0x36); // MX, MY, RGB mode
    // LCD_Write_DATA(0xC8);

    // ST7735R Gamma Sequence
    LCD_Write_COM(0xe0);
    LCD_Write_DATA(0x0f);
    LCD_Write_DATA(0x1a);
    LCD_Write_DATA(0x0f);
    LCD_Write_DATA(0x18);
    LCD_Write_DATA(0x2f);
    LCD_Write_DATA(0x28);
    LCD_Write_DATA(0x20);
    LCD_Write_DATA(0x22);
    LCD_Write_DATA(0x1f);
    LCD_Write_DATA(0x1b);
    LCD_Write_DATA(0x23);
    LCD_Write_DATA(0x37);
    LCD_Write_DATA(0x00);

    LCD_Write_DATA(0x07);
    LCD_Write_DATA(0x02);
    LCD_Write_DATA(0x10);

    LCD_Write_COM(0xe1);
    LCD_Write_DATA(0x0f);
    LCD_Write_DATA(0x1b);
    LCD_Write_DATA(0x0f);
    LCD_Write_DATA(0x17);
    LCD_Write_DATA(0x33);
    LCD_Write_DATA(0x2c);
    LCD_Write_DATA(0x29);
    LCD_Write_DATA(0x2e);
    LCD_Write_DATA(0x30);
    LCD_Write_DATA(0x30);
    LCD_Write_DATA(0x39);
    LCD_Write_DATA(0x3f);
    LCD_Write_DATA(0x00);
    LCD_Write_DATA(0x07);
    LCD_Write_DATA(0x03);
    LCD_Write_DATA(0x10);

    LCD_Write_COM(0x2a);
    LCD_Write_DATA(0x00);
    LCD_Write_DATA(0x00);
    LCD_Write_DATA(0x00);
    LCD_Write_DATA(0x7f);
    LCD_Write_COM(0x2b);
    LCD_Write_DATA(0x00);
    LCD_Write_DATA(0x00);
    LCD_Write_DATA(0x00);
    LCD_Write_DATA(0x9f);

    LCD_Write_COM(0xF0); // Enable test command
    LCD_Write_DATA(0x01);
    LCD_Write_COM(0xF6); // Disable ram power save mode
    LCD_Write_DATA(0x00);

    LCD_Write_COM(0x3A); // 65k mode
    LCD_Write_DATA(0x05);
    LCD_Write_COM(0x29); // Display on

    clrScr();
    // setColor(255, 0, 0);
    // fillRect(0, 0, 127, 64);

    st7735_set_orientation(1);
}

static void setXY(char x1, char y1, char x2, char y2)
{
    LCD_Write_COM(0x2a);
    LCD_Write_DATA(x1 >> 8);
    LCD_Write_DATA(x1 + 2);
    LCD_Write_DATA(x2 >> 8);
    LCD_Write_DATA(x2 + 2);

    LCD_Write_COM(0x2b);
    LCD_Write_DATA(y1 >> 8);
    LCD_Write_DATA(y1 + 3);
    LCD_Write_DATA(y2 >> 8);
    LCD_Write_DATA(y2 + 3);

    LCD_Write_COM(0x2c);
}

static void clrXY()
{
    setXY(0, 0, disp_x_size, disp_y_size);
}

static void clrScr()
{
    long i;

    clrXY();

    for (i = 0; i < ((127 + 1) * (127 + 1)); i++)
    {
        LCD_Writ_Bus(1, 0, display_transfer_mode);
        LCD_Writ_Bus(1, 0, display_transfer_mode);
    }
}

void setColor(int r, int g, int b)
{
    fcolorr = r;
    fcolorg = g;
    fcolorb = b;
}

void drawHLine(int x, int y, int l)
{
    char ch, cl;

    ch = ((fcolorr & 248) | fcolorg >> 5);
    cl = ((fcolorg & 28) << 3 | fcolorb >> 3);

    setXY(x, y, x + l, y);

    for (int i = 0; i < l + 1; i++)
    {
        LCD_Write_DATA_2(ch, cl);
    }

    clrXY();
}

void fillRect(int x1, int y1, int x2, int y2)
{
    if (x1 > x2)
    {
        swap_utft(int, x1, x2);
    }
    if (y1 > y2)
    {
        swap_utft(int, y1, y2);
    }

    for (int i = 0; i < ((y2 - y1) / 2) + 1; i++)
    {
        drawHLine(x1, y1 + i, x2 - x1);
        drawHLine(x1, y2 - i, x2 - x1);
    }
}

static void st7735_send_color(void *data, uint16_t length)
{
    char *chardata = (char *)data;
    char sendonedata = 0;

    for (int i = 0; i < length; i++)
    {
        sendonedata = chardata[i];
        LCD_Write_DATA(sendonedata);
    }
}

void st7735_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    ESP_LOGI(TAG, "ST7735 flush. x1:%d y1:%d x2:%d y2:%d\n",area->x1, area->y1, area->x2, area->y2);

#if 0
    #define BUF_W 20
    #define BUF_H 10

        lv_color_t buf[BUF_W * BUF_H];
        lv_color_t *buf_p = buf;
        printf("sizeof %d\r\n",sizeof(lv_color_t));
        uint16_t x, y;
        for (y = 0; y < BUF_H; y++)
        {
            lv_color_t c = lv_color_mix(LV_COLOR_BLUE, LV_COLOR_RED, (y * 255) / BUF_H);
            for (x = 0; x < BUF_W; x++)
            {
                (*buf_p) = c;
                buf_p++;
            }
        }

        lv_area_t a;
        a.x1 = 10;
        a.y1 = 10;
        a.x2 = a.x1 + BUF_W - 1;
        a.y2 = a.y1 + BUF_H - 1;
        area = &a;
        color_map = buf;
#endif

    uint8_t data[4];

    /*Column addresses*/
    st7735_send_cmd(0x2A);
    data[0] = (area->x1 >> 8) & 0xFF;
    data[1] = (area->x1 & 0xFF) + (st7735_portrait_mode ? COLSTART : ROWSTART);
    data[2] = (area->x2 >> 8) & 0xFF;
    data[3] = (area->x2 & 0xFF) + (st7735_portrait_mode ? COLSTART : ROWSTART);
    st7735_send_data(data, 4);

    /*Page addresses*/
    st7735_send_cmd(0x2B);
    data[0] = (area->y1 >> 8) & 0xFF;
    data[1] = (area->y1 & 0xFF) + (st7735_portrait_mode ? ROWSTART : COLSTART);
    data[2] = (area->y2 >> 8) & 0xFF;
    data[3] = (area->y2 & 0xFF) + (st7735_portrait_mode ? ROWSTART : COLSTART);
    st7735_send_data(data, 4);

    /*Memory write*/
    st7735_send_cmd(0x2C);

    //testcode
    // char ch, cl;
    // setColor(255, 0, 0);

    // ch = ((fcolorr & 248) | fcolorg >> 5);
    // cl = ((fcolorg & 28) << 3 | fcolorb >> 3);

    // for (int i = 0; i < 127 + 1; i++)
    // {
    //     LCD_Write_DATA_2(ch, cl);
    // }

    uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);
    ESP_LOGI(TAG,"size %d\n",size);
    st7735_send_color((void*)color_map, size * 2);
}