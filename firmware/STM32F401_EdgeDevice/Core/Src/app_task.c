#include "app_task.h"

#include "Data_Manager.h"
#include "MQ135.h"
#include "MQ7.h"
#include "Matrixrun.h"
#include "RGBMatrix_device.h"
#include "adc.h"
#include "uart_handler.h"
#include "usart.h"
#include <stdio.h>

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
  int co_i = (int)co;
  int co_f = (int)((co - (float)co_i) * 100.0f);
  int co2_i = (int)co2;
  int co2_f = (int)((co2 - (float)co2_i) * 100.0f);
  Data_Manager_SetSensorValues(co, co2);

  //uart6 debug sensor
//  if (co_f < 0) { co_f = -co_f; }
//  if (co2_f < 0) { co2_f = -co2_f; }
//
//  printf("[SENSOR] adc_co=%lu adc_co2=%lu | CO=%d.%02d ppm CO2=%d.%02d ppm\r\n",
//         (unsigned long)adc_value_co,
//         (unsigned long)adc_value_co2,
//         co_i, co_f,
//         co2_i, co2_f);
}

void AppTask_Init(void)
{
  DWT_Init();
  HUB75_Init();

  Data_Manager_Init();
  // MQ7_Init();
  // MQ135_Init();
  Data_Manager_SetSensorValues(MQ7_ReadCO(&hadc1, SENSOR_EMA_ALPHA),
                               MQ135_ReadCO2(&hadc1, SENSOR_EMA_ALPHA));
  Data_Manager_SetTempHumValues(24.5f, 51.0f);
  s_sensor_tick = HAL_GetTick();

  MatrixRun_Init();

  UART_CMD_Init(&huart6);
}

void AppTask_Run(void)
{
  AppTask_UpdateGasSensors();
  MatrixRun_Run();
}
