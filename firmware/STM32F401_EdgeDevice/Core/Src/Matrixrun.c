#include "Matrixrun.h"

#include "main.h"
#include "Data_Manager.h"
#include "Display_Screens.h"
#include "RGBMatrix_device.h"

#define SCREEN_SWITCH_INTERVAL_MS 5000U

static uint32_t last_tick = 0;
static int screen_state = 0;
static uint8_t congestion_status[8] = {2,  2, 2, 2, 2, 2, 1, 0};

static void MatrixRun_ShowDashboard(void)
{
  HUB75_Clear();
  Screen_Show_StatusRow(congestion_status);
  Paint_DrawRectangle(1, 1, 64, 32, RED, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  HUB75_Display();
}

void MatrixRun_Init(void)
{
  MatrixRun_ShowDashboard();
  screen_state = 1;
  last_tick = HAL_GetTick();
}

void MatrixRun_Run(void)
{
  if ((HAL_GetTick() - last_tick) <= SCREEN_SWITCH_INTERVAL_MS)
  {
    return;
  }

  last_tick = HAL_GetTick();

  switch (screen_state)
  {
  case 0:
    MatrixRun_ShowDashboard();
    screen_state = 1;
    break;
  case 1:
    Screen_Show_Alert(&g_db_data);
    screen_state = 2;
    break;
  case 2:
    Screen_Show_CO2(&g_db_data);
    screen_state = 0;
    break;
  default:
    screen_state = 0;
    break;
  }
}

void MatrixRun_SetCongestionBulk(const uint8_t data[8])
{
  for (int i = 0; i < 8; i++)
  {
    congestion_status[i] = data[i];
  }
}
