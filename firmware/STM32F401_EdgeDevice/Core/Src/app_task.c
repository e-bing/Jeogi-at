#include "app_task.h"

#include "Data_Manager.h"
#include "Matrixrun.h"
#include "RGBMatrix_device.h"
#include "uart_handler.h"
#include "usart.h"

void AppTask_Init(void)
{
  DWT_Init();
  HUB75_Init();

  Data_Manager_Init();
  g_db_data.co2_val = 330.57;
  g_db_data.target_num = 8;

  MatrixRun_Init();

  UART_CMD_Init(&huart6);
}

void AppTask_Run(void)
{
  MatrixRun_Run();
}

void AppTask_Loop(void)
{
  while (1)
  {
    AppTask_Run();
  }
}
