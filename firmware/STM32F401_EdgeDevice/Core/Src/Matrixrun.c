#include "Matrixrun.h"

#include "main.h"
#include "../../User/GUI/Data_Manager.h"
#include "../../User/RGBMatrix/RGBMatrix_device.h"
#include "../../User/GUI/Display_Screens.h"

static uint8_t congestion_status[8] = {2, 0, 1, 1, 0, 2, 1, 0};
static uint8_t dashboard_dirty = 1U;
static uint8_t gas_dirty = 1U;
static uint8_t current_screen = 0U; // 0: Dashboard, 1: CO/CO2
static uint32_t last_refresh_tick = 0U;
static uint32_t last_switch_tick = 0U;

#define DASHBOARD_REFRESH_MS 100U
#define DASHBOARD_SHOW_MS 10000U
#define GAS_SHOW_MS 5000U

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
  dashboard_dirty = 0U;
  gas_dirty = 1U;
  current_screen = 0U;
  last_refresh_tick = HAL_GetTick();
  last_switch_tick = HAL_GetTick();
}

void MatrixRun_Run(void)
{
  uint32_t now = HAL_GetTick();
  uint8_t need_refresh;
  uint32_t stay_ms = (current_screen == 0U) ? DASHBOARD_SHOW_MS : GAS_SHOW_MS;

  if ((now - last_switch_tick) >= stay_ms)
  {
    current_screen = (current_screen == 0U) ? 1U : 0U;
    last_switch_tick = now;

    if (current_screen == 0U)
    {
      dashboard_dirty = 1U;
    }
    else
    {
      gas_dirty = 1U;
    }
  }

  need_refresh = (current_screen == 0U) ? dashboard_dirty : gas_dirty;
  if ((now - last_refresh_tick) >= DASHBOARD_REFRESH_MS)
  {
    need_refresh = 1U;
  }

  if (!need_refresh)
  {
    return;
  }

  if (current_screen == 0U)
  {
    MatrixRun_ShowDashboard();
    dashboard_dirty = 0U;
  }
  else
  {
    Screen_Show_CO2(&g_db_data);
    gas_dirty = 0U;
  }

  last_refresh_tick = now;
}

void MatrixRun_SetCongestionBulk(const uint8_t data[8])
{
  uint8_t changed = 0U;

  for (int i = 0; i < 8; i++)
  {
    if (congestion_status[i] != data[i])
    {
      changed = 1U;
    }
    congestion_status[i] = data[i];
  }

  if (changed)
  {
    dashboard_dirty = 1U;
  }
}
