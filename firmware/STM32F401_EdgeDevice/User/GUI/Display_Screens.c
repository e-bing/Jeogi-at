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

void Screen_Show_StatusRow(const uint8_t status[8])
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        char label[2];
        uint16_t text_x = 8 + (i * 6);
        uint16_t point_x = 12 + (i * 6);
        uint16_t rect_x_start = point_x - 3; // 6px width
        uint16_t rect_x_end = point_x + 2;
        uint16_t rect_y_start = 6; // 11px height
        uint16_t rect_y_end = 16;
        label[0] = (char)('1' + i);
        label[1] = '\0';

        Paint_DrawString_EN(text_x, 19, label, &Font8, (i == 0) ? FONT_BACKGROUND : BLACK, WHITE);
        Paint_DrawRectangle(rect_x_start, rect_y_start, rect_x_end, rect_y_end, status_to_color(status[i]), DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawRectangle(rect_x_start, rect_y_start, rect_x_end, rect_y_end, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        Paint_DrawLine(rect_x_start, rect_y_end, rect_x_end, rect_y_end, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    }
}

void Screen_Show_Dashboard(void)
{
    static const uint8_t demo_status[8] = {2, 2, 2, 2, 2, 2, 1, 0};

    HUB75_Clear();
    Screen_Show_StatusRow(demo_status);
    Paint_DrawRectangle(1, 1, 64, 32, RED, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    HUB75_Display();
}

void Screen_Show_CO2(DB_Data_t *data)
{
    HUB75_Clear();
    Paint_DrawString_EN(10, 3, "CO-2", &Font16, BLACK, YELLOW);
    Paint_DrawString_EN(40, 16, "ppm", &Font8, BLACK, RED);

    // DB에서 온 실제 값 출력
    Paint_DrawNum(10, 18, data->co2_val, &Font8, 2, FONT_BACKGROUND, WHITE);
    HUB75_Display();
}

void Screen_Show_Alert(DB_Data_t *data)
{
    const int target = data->target_num;

    HUB75_Clear();
    Paint_DrawString_EN(6, 3, "GO", &Font16, BLACK, RED);
    Paint_DrawString_EN(15, 16, "Number", &Font8, BLACK, YELLOW);

    char buf[5];
    sprintf(buf, "%d", data->target_num);
    Paint_DrawString_EN(52, 16, buf, &Font8, BLACK, WHITE);
    Paint_DrawString_EN(58, 16, "!", &Font8, BLACK, RED);

    if (target >= 1 && target <= 3)
    {
        Paint_DrawString_EN(0, 24, "<----------", &Font8, BLACK, BLUE);
    }
    else if (target >= 5 && target <= 8)
    {
        Paint_DrawString_EN(0, 24, "---------->", &Font8, BLACK, BLUE);
    }

    HUB75_Display();
}
