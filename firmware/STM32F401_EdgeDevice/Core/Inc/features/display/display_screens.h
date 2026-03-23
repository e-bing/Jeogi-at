#ifndef FEATURES_DISPLAY_DISPLAY_SCREENS_H
#define FEATURES_DISPLAY_DISPLAY_SCREENS_H

#include "drivers/display/gui_paint.h"
#include "drivers/display/fonts.h"
#include "common/data_manager.h"

void Screen_Show_CO2(DB_Data_t* data);
void Screen_Show_TempHum(DB_Data_t* data);
void Screen_Show_StatusRow(const uint8_t status[8], uint8_t guide_mode);
void Screen_Show_Train(uint8_t train_dest_code, int8_t x_offset);

#endif /* FEATURES_DISPLAY_DISPLAY_SCREENS_H */
