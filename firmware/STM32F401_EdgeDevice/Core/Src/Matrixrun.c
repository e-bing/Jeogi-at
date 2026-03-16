#include "Matrixrun.h"

#include "main.h"
#include "Data_Manager.h"
#include "Display_Screens.h"
#include "RGBMatrix_device.h"

static uint8_t congestion_status[8] = {2, 0, 1, 1, 0, 2, 1, 0};
static uint8_t dashboard_dirty = 1U;
static uint8_t current_screen = 0U; // 0: Dashboard, 1: CO/CO2, 2: TEMP/HUM
static uint8_t dashboard_guide_mode = 0U; // 0: side guidance, 1: per-platform up arrows
static uint32_t last_refresh_tick = 0U;
static uint32_t last_switch_tick = 0U;
static uint32_t last_dashboard_mode_tick = 0U;

#define DASHBOARD_REFRESH_MS 5000U
#define DASHBOARD_GUIDE_TOGGLE_MS 2000U

#define DASHBOARD_SHOW_MS 10000U
#define GAS_SHOW_MS 5000U
#define TEMP_HUM_SHOW_MS 5000U

static void MatrixRun_ShowDashboard(void)
{
//  HUB75_Clear(); // first clear
  Screen_Show_StatusRow(congestion_status, dashboard_guide_mode);
  Paint_DrawRectangle(1, 1, 64, 32, RED, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
}

void MatrixRun_Init(void)
{
  MatrixRun_ShowDashboard();
  dashboard_dirty = 0U;
  current_screen = 0U;
  dashboard_guide_mode = 0U;
  last_refresh_tick = HAL_GetTick();
  last_switch_tick = HAL_GetTick();
  last_dashboard_mode_tick = HAL_GetTick();
}

void MatrixRun_Run(void)
{
  uint32_t now = HAL_GetTick();
  uint8_t need_refresh;
  uint8_t screen_changed = 0U;
  uint32_t stay_ms;

  if (current_screen == 0U)
  {
    stay_ms = DASHBOARD_SHOW_MS;

  }
  else if (current_screen == 1U)
  {
    stay_ms = GAS_SHOW_MS;
  }
  else
  {
    stay_ms = TEMP_HUM_SHOW_MS;
  }

  if ((now - last_switch_tick) >= stay_ms)
  {
    HUB75_Clear();
    current_screen = (uint8_t)((current_screen + 1U) % 3U);
    last_switch_tick = now;
    screen_changed = 1U;

    if (current_screen == 0U)
    {
      dashboard_dirty = 1U;
      dashboard_guide_mode = 0U;
      last_dashboard_mode_tick = now;


      // test: play guide wav
      printf("play guide wav run\r\n");
//      Audio_Guide((uint8_t[8]){0, 1, 1, 1, 1, 1, 1, 1});
      Audio_Guide(congestion_status);
    }
  }

  if ((current_screen == 0U) && ((now - last_dashboard_mode_tick) >= DASHBOARD_GUIDE_TOGGLE_MS))
  {
    dashboard_guide_mode = (uint8_t)(dashboard_guide_mode ^ 1U);
    last_dashboard_mode_tick = now;
    dashboard_dirty = 1U;
  }

  need_refresh = screen_changed;
  if ((current_screen == 0U) && (dashboard_dirty != 0U))
  {
    need_refresh = 1U;
  }
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
    //
  }
  else
  {
    if (current_screen == 1U)
    {
      Screen_Show_CO2(&g_db_data);
    }
    else
    {
      Screen_Show_TempHum(&g_db_data);
    }
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

void MatrixRun_SetScreen(uint8_t screen)
{
    if (screen > 2U) return;
    HUB75_Clear();
    current_screen = screen;
    last_switch_tick = HAL_GetTick();  // 타이머 리셋 (바로 안 넘어가게)
    last_refresh_tick = 0U;
}
