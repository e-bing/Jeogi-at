#include "dev_ctrl.h"
#include "main.h"
#include <string.h>

void Sensor_Init(void)
{
    // 필요하면 초기화 코드
	printf("sensor_init\r\n");
}

void Sensor_SetState(char *name, char *state)
{
	printf("called, name: %s, state: %s\r\n", name, state);

    if(strcmp(name,"SEN1")==0)
    {
        if(strcmp(state,"ON")==0)
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);

        else if(strcmp(state,"OFF")==0)
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    }
}
