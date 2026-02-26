#include "Matrixrun.h"

#include "main.h"
#include "Data_Manager.h"
#include "Display_Screens.h"
#include "RGBMatrix_device.h"

#define DASHBOARD_REFRESH_INTERVAL_MS 200U

static uint32_t last_tick = 0;
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
  last_tick = HAL_GetTick();
}

void MatrixRun_Run(void)
{
  if ((HAL_GetTick() - last_tick) <= DASHBOARD_REFRESH_INTERVAL_MS)
  {
    return;
  }

  last_tick = HAL_GetTick();
  MatrixRun_ShowDashboard();
}

void MatrixRun_SetCongestionBulk(const uint8_t data[8])
{
  for (int i = 0; i < 8; i++)
  {
    congestion_status[i] = data[i];
  }

  // Update immediately when new UART data arrives.
  MatrixRun_ShowDashboard();
  last_tick = HAL_GetTick();
}
