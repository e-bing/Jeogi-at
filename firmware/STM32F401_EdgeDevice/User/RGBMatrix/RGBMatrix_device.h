#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include "stm32f4xx_hal.h"
#include "main.h"
#include "usart.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "fonts.h"

#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

#include "GUI_Paint.h"

#define LED_chain 2
#define LED_Width  (64 * LED_chain)
#define LED_Height 32

#define SCROLL_SPEED 5
#define HAL_DELAY_NEWBIE 0

void DWT_Init(void);
void DWT_Delay(uint32_t us);

/* =========================================================
 * Fast GPIO control using BSRR
 * ========================================================= */
#define GPIO_SET(port, pin)      ((port)->BSRR = (pin))
#define GPIO_RESET(port, pin)    ((port)->BSRR = ((uint32_t)(pin) << 16U))

/* ---------------- RGB data pins ---------------- */
#define RGB_R1_HIGH()    GPIO_SET(R1_GPIO_Port, R1_Pin)
#define RGB_R1_LOW()     GPIO_RESET(R1_GPIO_Port, R1_Pin)

#define RGB_G1_HIGH()    GPIO_SET(G1_GPIO_Port, G1_Pin)
#define RGB_G1_LOW()     GPIO_RESET(G1_GPIO_Port, G1_Pin)

#define RGB_B1_HIGH()    GPIO_SET(B1_GPIO_Port, B1_Pin)
#define RGB_B1_LOW()     GPIO_RESET(B1_GPIO_Port, B1_Pin)

#define RGB_R2_HIGH()    GPIO_SET(R2_GPIO_Port, R2_Pin)
#define RGB_R2_LOW()     GPIO_RESET(R2_GPIO_Port, R2_Pin)

#define RGB_G2_HIGH()    GPIO_SET(G2_GPIO_Port, G2_Pin)
#define RGB_G2_LOW()     GPIO_RESET(G2_GPIO_Port, G2_Pin)

#define RGB_B2_HIGH()    GPIO_SET(B2_GPIO_Port, B2_Pin)
#define RGB_B2_LOW()     GPIO_RESET(B2_GPIO_Port, B2_Pin)

/* ---------------- Row select pins ---------------- */
#define RGB_A_HIGH()     GPIO_SET(A_GPIO_Port, A_Pin)
#define RGB_A_LOW()      GPIO_RESET(A_GPIO_Port, A_Pin)

#define RGB_B_HIGH()     GPIO_SET(B_GPIO_Port, B_Pin)
#define RGB_B_LOW()      GPIO_RESET(B_GPIO_Port, B_Pin)

#define RGB_C_HIGH()     GPIO_SET(C_GPIO_Port, C_Pin)
#define RGB_C_LOW()      GPIO_RESET(C_GPIO_Port, C_Pin)

#define RGB_D_HIGH()     GPIO_SET(D_GPIO_Port, D_Pin)
#define RGB_D_LOW()      GPIO_RESET(D_GPIO_Port, D_Pin)

/* ---------------- Control pins ---------------- */
#define RGB_CLK_HIGH()   GPIO_SET(CLK_GPIO_Port, CLK_Pin)
#define RGB_CLK_LOW()    GPIO_RESET(CLK_GPIO_Port, CLK_Pin)

#define RGB_LAT_HIGH()   GPIO_SET(LAT_GPIO_Port, LAT_Pin)
#define RGB_LAT_LOW()    GPIO_RESET(LAT_GPIO_Port, LAT_Pin)

#define RGB_OE_HIGH()    GPIO_SET(OE_GPIO_Port, OE_Pin)
#define RGB_OE_LOW()     GPIO_RESET(OE_GPIO_Port, OE_Pin)

/* -------------------------------------------------
 * Optional compatibility macros
 * 기존 코드 많이 건드리기 싫으면 임시로 유지 가능
 * 성능상으론 HIGH/LOW 직접 호출이 더 낫다
 * ------------------------------------------------- */
#define RGB_R1(value)    do { if (value) RGB_R1_HIGH(); else RGB_R1_LOW(); } while (0)
#define RGB_G1(value)    do { if (value) RGB_G1_HIGH(); else RGB_G1_LOW(); } while (0)
#define RGB_B1(value)    do { if (value) RGB_B1_HIGH(); else RGB_B1_LOW(); } while (0)

#define RGB_R2(value)    do { if (value) RGB_R2_HIGH(); else RGB_R2_LOW(); } while (0)
#define RGB_G2(value)    do { if (value) RGB_G2_HIGH(); else RGB_G2_LOW(); } while (0)
#define RGB_B2(value)    do { if (value) RGB_B2_HIGH(); else RGB_B2_LOW(); } while (0)

#define RGB_A(value)     do { if (value) RGB_A_HIGH(); else RGB_A_LOW(); } while (0)
#define RGB_B(value)     do { if (value) RGB_B_HIGH(); else RGB_B_LOW(); } while (0)
#define RGB_C(value)     do { if (value) RGB_C_HIGH(); else RGB_C_LOW(); } while (0)
#define RGB_D(value)     do { if (value) RGB_D_HIGH(); else RGB_D_LOW(); } while (0)

#define RGB_CLK(value)   do { if (value) RGB_CLK_HIGH(); else RGB_CLK_LOW(); } while (0)
#define RGB_LAT(value)   do { if (value) RGB_LAT_HIGH(); else RGB_LAT_LOW(); } while (0)
#define RGB_OE(value)    do { if (value) RGB_OE_HIGH(); else RGB_OE_LOW(); } while (0)

void HUB75_Init(void);
void RGBMatrixWriteData(void);
void HUB75_Display(void);
void HUB75_Clear(void);

void scrollLeft(void);
void scrollRight(void);
void scrollUp(void);
void scrollDown(void);

void HUB75_RefreshStep(void);
void HUB75_RefreshStep_ISR(void);

#endif
