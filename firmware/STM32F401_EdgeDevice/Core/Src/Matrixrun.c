#include "Matrixrun.h"

#include "main.h"
#include "Data_Manager.h"
#include "Display_Screens.h"
#include "RGBMatrix_device.h"

#define SCREEN_DASHBOARD 0U
#define SCREEN_CO2       1U
#define SCREEN_TEMP_HUM  2U
#define SCREEN_TRAIN     3U

static uint8_t congestion_status[8] = {2, 0, 1, 1, 0, 2, 1, 0};
static uint8_t dashboard_dirty = 1U;
static uint8_t current_screen = 0U; // 0: Dashboard, 1: CO/CO2, 2: TEMP/HUM
static uint8_t auto_cycle_enabled = 1U;
static uint8_t train_dest_code = 0U; // 1: DAEWHA, 2: GUPABAL, 0: UNKNOWN
static uint8_t train_event_active = 0U;
static uint8_t dashboard_guide_mode = 0U; // 0: side guidance, 1: per-platform up arrows
static int8_t train_text_offset_x = 0;
static int8_t train_text_offset_dir = 1;
static uint32_t last_refresh_tick = 0U;
static uint32_t last_switch_tick = 0U;
static uint32_t last_dashboard_mode_tick = 0U;
static uint32_t train_event_start_tick = 0U;

#define DASHBOARD_REFRESH_MS 5000U
#define TRAIN_ANIM_REFRESH_MS 120U
#define DASHBOARD_GUIDE_TOGGLE_MS 2000U
#define TRAIN_EVENT_SHOW_MS 10000U

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
  current_screen = SCREEN_DASHBOARD;
  auto_cycle_enabled = 1U;
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

  if (train_event_active != 0U)
  {
    if (current_screen != SCREEN_TRAIN)
    {
      HUB75_Clear();
      current_screen = SCREEN_TRAIN;
      screen_changed = 1U;
      last_switch_tick = now;
      last_refresh_tick = 0U;
    }

    if ((now - train_event_start_tick) >= TRAIN_EVENT_SHOW_MS)
    {
      train_event_active = 0U;
      if (auto_cycle_enabled != 0U)
      {
        HUB75_Clear();
        current_screen = SCREEN_DASHBOARD;
        dashboard_dirty = 1U;
        dashboard_guide_mode = 0U;
        last_dashboard_mode_tick = now;
        last_switch_tick = now;
        screen_changed = 1U;
      }
    }
  }

  if (current_screen == SCREEN_DASHBOARD)
  {
    stay_ms = DASHBOARD_SHOW_MS;

  }
  else if (current_screen == SCREEN_CO2)
  {
    stay_ms = GAS_SHOW_MS;
  }
  else
  {
    stay_ms = TEMP_HUM_SHOW_MS;
  }

  if (auto_cycle_enabled &&
      (current_screen <= SCREEN_TEMP_HUM) &&
      ((now - last_switch_tick) >= stay_ms))
  {
    HUB75_Clear();
    current_screen = (uint8_t)((current_screen + 1U) % 3U);
    last_switch_tick = now;
    screen_changed = 1U;

    if (current_screen == SCREEN_DASHBOARD)
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

  if ((current_screen == SCREEN_DASHBOARD) && ((now - last_dashboard_mode_tick) >= DASHBOARD_GUIDE_TOGGLE_MS))
  {
    dashboard_guide_mode = (uint8_t)(dashboard_guide_mode ^ 1U);
    last_dashboard_mode_tick = now;
    dashboard_dirty = 1U;
  }

  need_refresh = screen_changed;
  if ((current_screen == SCREEN_DASHBOARD) && (dashboard_dirty != 0U))
  {
    need_refresh = 1U;
  }
  if (((current_screen == SCREEN_TRAIN) && ((now - last_refresh_tick) >= TRAIN_ANIM_REFRESH_MS)) ||
      ((current_screen != SCREEN_TRAIN) && ((now - last_refresh_tick) >= DASHBOARD_REFRESH_MS)))
  {
    need_refresh = 1U;
  }

  if (!need_refresh)
  {
    return;
  }

  if (current_screen == SCREEN_DASHBOARD)
  {
    MatrixRun_ShowDashboard();
    dashboard_dirty = 0U;
    //
  }
  else if (current_screen == SCREEN_CO2)
  {
    Screen_Show_CO2(&g_db_data);
  }
  else if (current_screen == SCREEN_TEMP_HUM)
  {
    Screen_Show_TempHum(&g_db_data);
  }
  else
  {
    Screen_Show_Train(train_dest_code, train_text_offset_x);
    train_text_offset_x = (int8_t)(train_text_offset_x + train_text_offset_dir);
    if (train_text_offset_x >= 6)
    {
      train_text_offset_x = 6;
      train_text_offset_dir = -1;
    }
    else if (train_text_offset_x <= -6)
    {
      train_text_offset_x = -6;
      train_text_offset_dir = 1;
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
    if (screen > SCREEN_TRAIN) return;
    HUB75_Clear();
    //auto_cycle_enabled = 0U;
    current_screen = screen;
    last_switch_tick = HAL_GetTick();  // 타이머 리셋 (바로 안 넘어가게)
    last_refresh_tick = 0U;
}

void MatrixRun_SetAutoCycle(uint8_t enable)
{
    auto_cycle_enabled = (enable != 0U) ? 1U : 0U;
    if (auto_cycle_enabled)
    {
        current_screen = SCREEN_DASHBOARD;
        dashboard_dirty = 1U;
        dashboard_guide_mode = 0U;
        last_dashboard_mode_tick = HAL_GetTick();
    }
    HUB75_Clear();
    last_switch_tick = HAL_GetTick();
    last_refresh_tick = 0U;
}

void MatrixRun_SetTrainDest(uint8_t dest_code)
{
    if (dest_code > 2U)
    {
        dest_code = 0U;
    }
    train_dest_code = dest_code;

    if (dest_code != 0U)
    {
        train_event_active = 1U;
        train_event_start_tick = HAL_GetTick();
        train_text_offset_x = 0;
        train_text_offset_dir = 1;
    }
    else
    {
        train_event_active = 0U;
    }

    if (current_screen == SCREEN_TRAIN)
    {
        last_refresh_tick = 0U;
    }
}

uint8_t MatrixRun_GetTrainDest(void)
{
    return train_dest_code;
}
