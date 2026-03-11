#include "Display_Screens.h"
#include <stdio.h>

static uint16_t status_to_color(uint8_t status)
{
    switch (status)
    {
    case 0:
        return GREEN;
    case 1:
        return YELLOW;
    case 2:
        return RED;
    default:
        return WHITE;
    }
}

static void draw_tiny_7seg_digit(uint16_t x, uint16_t y, uint8_t digit, uint16_t color)
{
    // 3x5 body (+ optional middle), compact enough for 64x32 top row
    const uint8_t seg_map[10] = {
        0x3F, // 0: a b c d e f
        0x06, // 1: b c
        0x5B, // 2: a b d e g
        0x4F, // 3: a b c d g
        0x66, // 4: b c f g
        0x6D, // 5: a c d f g
        0x7D, // 6: a c d e f g
        0x07, // 7: a b c
        0x7F, // 8: a b c d e f g
        0x6F  // 9: a b c d f g
    };
    uint8_t m;

    if (digit > 9)
    {
        return;
    }

    m = seg_map[digit];
    if (m & 0x01)
        Paint_DrawLine(x, y, x + 2, y, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // a
    if (m & 0x02)
        Paint_DrawLine(x + 2, y, x + 2, y + 2, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // b
    if (m & 0x04)
        Paint_DrawLine(x + 2, y + 2, x + 2, y + 4, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // c
    if (m & 0x08)
        Paint_DrawLine(x, y + 4, x + 2, y + 4, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // d
    if (m & 0x10)
        Paint_DrawLine(x, y + 2, x, y + 4, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // e
    if (m & 0x20)
        Paint_DrawLine(x, y, x, y + 2, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // f
    if (m & 0x40)
        Paint_DrawLine(x, y + 2, x + 2, y + 2, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID); // g
}

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo)
    {
        return lo;
    }
    if (v > hi)
    {
        return hi;
    }
    return v;
}

static uint16_t co2_to_color(int co2_ppm)
{
    if (co2_ppm <= 400)
    {
        return GREEN;
    }
    if (co2_ppm <= 700)
    {
        return YELLOW;
    }
    return RED;
}

static uint16_t co_to_color(int co_deci_ppm)
{
    if (co_deci_ppm <= 30) // 0.0~3.0 ppm
    {
        return GREEN;
    }
    if (co_deci_ppm <= 80) // 3.1~8.0 ppm
    {
        return YELLOW;
    }
    return RED; // 8.1+ ppm
}

static void draw_tiny_7seg_3digit(uint16_t x, uint16_t y, int value, uint16_t color)
{
    int v = value;
    uint8_t d2;
    uint8_t d1;
    uint8_t d0;

    if (v < 0)
    {
        v = 0;
    }
    if (v > 999)
    {
        v = 999;
    }

    d2 = (uint8_t)(v / 100);
    d1 = (uint8_t)((v / 10) % 10);
    d0 = (uint8_t)(v % 10);

    draw_tiny_7seg_digit(x, y, d2, color);
    draw_tiny_7seg_digit(x + 4, y, d1, color);
    draw_tiny_7seg_digit(x + 8, y, d0, color);
}

static void draw_tiny_7seg_1decimal(uint16_t x, uint16_t y, int deci_value, uint16_t color)
{
    int v = deci_value;
    uint8_t int_part;
    uint8_t tens_part;
    uint8_t ones_part;
    uint8_t frac_part;
    uint16_t ones_x;

    if (v < 0)
    {
        v = 0;
    }
    if (v > 999)
    {
        v = 999;
    }

    int_part = (uint8_t)(v / 10);
    tens_part = (uint8_t)(int_part / 10);
    ones_part = (uint8_t)(int_part % 10);
    frac_part = (uint8_t)(v % 10);

    if (int_part >= 10U)
    {
        draw_tiny_7seg_digit(x, y, tens_part, color);
        ones_x = x + 4;
    }
    else
    {
        ones_x = x;
    }

    draw_tiny_7seg_digit(ones_x, y, ones_part, color);
    Paint_DrawPoint(ones_x + 4, y + 4, color, DOT_PIXEL_1X1, DOT_STYLE_DFT);
    draw_tiny_7seg_digit(ones_x + 6, y, frac_part, color);
}

static void Screen_Show_DualMetricLayout(const char *top_label,
                                         const char *bottom_label,
                                         int top_deci_value,
                                         int bottom_deci_value,
                                         const char *top_unit,
                                         const char *bottom_unit,
                                         uint16_t value_x_offset,
                                         uint16_t unit_x_offset,
                                         uint8_t top_show_decimal,
                                         uint8_t show_color_box,
                                         uint16_t top_color,
                                         uint16_t bottom_color)
{
    // Two-row layout (top 16px / bottom 16px)
    Paint_DrawRectangle(1, 1, 64, 15, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(1, 17, 64, 32, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

    Paint_DrawString_EN(5, 5, top_label, &Font8, BLACK, RED);
    if (top_show_decimal != 0U)
    {
        draw_tiny_7seg_1decimal((uint16_t)(22 + value_x_offset), 7, top_deci_value, WHITE);
    }
    else
    {
        draw_tiny_7seg_3digit((uint16_t)(23 + value_x_offset), 7, top_deci_value / 10, WHITE);
    }
    Paint_DrawString_EN((uint16_t)(37 + unit_x_offset), 5, top_unit, &Font8, BLACK, YELLOW);
    if (show_color_box != 0U)
    {
        Paint_DrawPoint(57, 8, top_color, DOT_PIXEL_4X4, DOT_STYLE_DFT);
    }

    Paint_DrawString_EN(5, 21, bottom_label, &Font8, BLACK, BLUE);
    draw_tiny_7seg_1decimal((uint16_t)(22 + value_x_offset), 23, bottom_deci_value, WHITE);
    Paint_DrawString_EN((uint16_t)(37 + unit_x_offset), 21, bottom_unit, &Font8, BLACK, YELLOW);
    if (show_color_box != 0U)
    {
        Paint_DrawPoint(57, 24, bottom_color, DOT_PIXEL_4X4, DOT_STYLE_DFT);
    }
}

static void draw_center_triangle_marker(uint16_t center_x, uint16_t top_y, uint16_t color)
{
    // 4-level triangle:
    // top: 1px, then 3px, 5px, bottom: 7px
    Paint_DrawPoint(center_x, top_y + 0, color, DOT_PIXEL_1X1, DOT_STYLE_DFT);
    Paint_DrawLine(center_x - 1, top_y + 1, center_x + 1, top_y + 1, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(center_x - 2, top_y + 2, center_x + 2, top_y + 2, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(center_x - 3, top_y + 3, center_x + 3, top_y + 3, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
}

static void draw_long_left_arrow(uint16_t start_x, uint16_t y, uint16_t color)
{
    // Pointed head (vertical heights: 1,3,5,7 from tip outward)
    Paint_DrawLine(3, y, 3, y, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(4, y - 1, 4, y + 1, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(5, y - 2, 5, y + 2, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(6, y - 3, 6, y + 3, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    // Long shaft
    Paint_DrawLine(7, y, start_x, y, color, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
}

static void draw_long_right_arrow(uint16_t start_x, uint16_t y, uint16_t color)
{
    // Pointed head (vertical heights: 1,3,5,7 from tip outward)
    Paint_DrawLine(60, y, 60, y, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(59, y - 1, 59, y + 1, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(58, y - 2, 58, y + 2, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(57, y - 3, 57, y + 3, color, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    // Long shaft
    Paint_DrawLine(start_x, y, 56, y, color, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
}

void Screen_Show_StatusRow(const uint8_t status[8])
{
    uint8_t i;
    uint8_t left_good = 0;
    uint8_t right_good = 0;
    const uint16_t center_x = 34;
    const uint16_t guide_y = 26;

    for (i = 0; i < 8; i++)
    {
        uint16_t car_x_start = 7 + (i * 7);
        uint16_t car_x_end = car_x_start + 5;
        uint16_t car_y_start = 12;
        uint16_t car_y_end = 19;
        uint16_t wheel_y = 21;

        draw_tiny_7seg_digit((uint16_t)(car_x_start + ((i >= 1U) ? 2U : 1U)), 6, (uint8_t)(i + 1), WHITE);

        Paint_DrawRectangle(car_x_start, car_y_start, car_x_end, car_y_end, status_to_color(status[i]), DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawRectangle(car_x_start, car_y_start, car_x_end, car_y_end, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

        Paint_DrawRectangle(car_x_start + 2, car_y_start + 2, car_x_end - 2, car_y_end - 2, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);

        Paint_DrawPoint(car_x_start + 1, wheel_y, WHITE, DOT_PIXEL_1X1, DOT_STYLE_DFT);
        Paint_DrawPoint(car_x_end - 1, wheel_y, WHITE, DOT_PIXEL_1X1, DOT_STYLE_DFT);

        if (status[i] == 0U)
        {
            if (i < 4U)
            {
                left_good++;
            }
            else
            {
                right_good++;
            }
        }
    }

    Paint_DrawPoint(6, 16, WHITE, DOT_PIXEL_2X2, DOT_STYLE_DFT);

    // Current position marker between car 4 and 5
    draw_center_triangle_marker(center_x, 21, RED);

    // Guidance arrow toward side with more GREEN cars
    if (left_good > right_good)
    {
        draw_long_left_arrow(center_x - 3, guide_y, GREEN);
    }
    else if (right_good > left_good)
    {
        draw_long_right_arrow(center_x + 3, guide_y, GREEN);
    }
    else
    {
        draw_long_left_arrow(center_x - 3, guide_y, GREEN);
        draw_long_right_arrow(center_x + 3, guide_y, GREEN);
    }
}

void Screen_Show_CO2(DB_Data_t *data)
{
    int co2_ppm;
    int co_ppm;
    uint16_t co2_color;
    uint16_t co_color;

    co2_ppm = (int)(data->co2_val + 0.5);
    co_ppm = (int)(data->co_val * 10.0 + 0.5);
    co2_color = co2_to_color(co2_ppm);
    co_color = co_to_color(co_ppm);

    Screen_Show_DualMetricLayout("CO2", "CO", co2_ppm * 10, co_ppm, "PPM", "PPM", 0, 0, 0U, 1U, co2_color, co_color);
}

void Screen_Show_TempHum(DB_Data_t *data)
{
    int temp_deci = (int)(data->temp_val * 10.0 + 0.5);
    int hum_deci = (int)(data->hum_val * 10.0 + 0.5);

    Screen_Show_DualMetricLayout("TEMP", "HUM", temp_deci, hum_deci, " C", " *", 5, 3, 1U, 0U, WHITE, WHITE);
}
