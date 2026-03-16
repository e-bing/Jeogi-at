#ifndef __DISPLAY_SCREENS_H
#define __DISPLAY_SCREENS_H

#include "gui_paint.h"
#include "fonts.h"
#include "Data_Manager.h"

void Screen_Show_CO2(DB_Data_t* data);
void Screen_Show_TempHum(DB_Data_t* data);
void Screen_Show_StatusRow(const uint8_t status[8], uint8_t guide_mode);
void Screen_Show_Train(uint8_t train_dest_code, int8_t x_offset);

#endif
