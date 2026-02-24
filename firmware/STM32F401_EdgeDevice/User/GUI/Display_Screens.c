#include "Display_Screens.h"
#include <stdio.h>

void Screen_Show_Dashboard(void)
{
    HUB75_Clear();

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
    HUB75_Clear();
    Paint_DrawString_EN(6, 3, "GO", &Font16, BLACK, RED);
    Paint_DrawString_EN(15, 16, "Number", &Font8, BLACK, BLUE);

    char buf[5];
    sprintf(buf, "%d", data->target_num);
    Paint_DrawString_EN(52, 16, buf, &Font8, BLACK, WHITE);
    Paint_DrawString_EN(58, 16, "!", &Font8, BLACK, RED);
    HUB75_Display();
}
