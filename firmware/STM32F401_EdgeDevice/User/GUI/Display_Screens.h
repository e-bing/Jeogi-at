#ifndef __DISPLAY_SCREENS_H
#define __DISPLAY_SCREENS_H

#include "gui_paint.h" // User/GUI 안에 있을 기존 헤더
#include "fonts.h"     // User/Fonts 안의 헤더
#include "Data_Manager.h"

// 화면 출력 함수들
void Screen_Show_Dashboard(void);
void Screen_Show_CO2(DB_Data_t* data);
void Screen_Show_Alert(DB_Data_t* data);

#endif
