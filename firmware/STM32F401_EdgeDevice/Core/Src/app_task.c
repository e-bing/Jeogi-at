#include "app_task.h"
#include "../../User/GUI/Data_Manager.h"
#include "../../User/RGBMatrix/RGBMatrix_device.h"
#include "MQ135.h"
#include "MQ7.h"
#include "Matrixrun.h"
#include "adc.h"
#include "uart_handler.h"
#include "usart.h"

static uint32_t s_sensor_tick = 0U;

#define SENSOR_READ_INTERVAL_MS 10000U
#define SENSOR_EMA_ALPHA        0.2f

static void AppTask_UpdateGasSensors(void)
{
  uint32_t now = HAL_GetTick();
  if ((now - s_sensor_tick) < SENSOR_READ_INTERVAL_MS)
  {
    return;
  }

  s_sensor_tick = now;

  float co = MQ7_ReadCO(&hadc1, SENSOR_EMA_ALPHA);
  float co2 = MQ135_ReadCO2(&hadc1, SENSOR_EMA_ALPHA);
  Data_Manager_SetSensorValues(co, co2);
}

void AppTask_Init(void)
{
  DWT_Init();
  HUB75_Init();

  Data_Manager_Init();
  MQ7_Init();
  MQ135_Init();
  Data_Manager_SetSensorValues(MQ7_ReadCO(&hadc1, SENSOR_EMA_ALPHA),
                               MQ135_ReadCO2(&hadc1, SENSOR_EMA_ALPHA));
  g_db_data.target_num = 8;
  s_sensor_tick = HAL_GetTick();

  MatrixRun_Init();

  UART_CMD_Init(&huart6);
}

void AppTask_Run(void)
{
  AppTask_UpdateGasSensors();
  MatrixRun_Run();
}

void AppTask_Loop(void)
{
  while (1)
  {
    AppTask_Run();
  }
}
