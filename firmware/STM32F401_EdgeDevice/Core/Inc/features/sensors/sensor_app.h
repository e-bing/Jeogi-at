/* sensor_app.h */
#ifndef FEATURES_SENSORS_SENSOR_APP_H
#define FEATURES_SENSORS_SENSOR_APP_H

#include "adc.h"

// ADC 핸들 공유
extern ADC_HandleTypeDef hadc1;

// 센서 필터 공통 계수
#define SENSOR_ALPHA 0.1f

#endif /* FEATURES_SENSORS_SENSOR_APP_H */
