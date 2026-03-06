#include "RGBMatrix_device.h"

uint8_t LED_Buffer[LED_Width * LED_Height];

/**
 * Initialization routine.
 */
void DWT_Init(void)
{
    if (!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk)) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
}

void DWT_Delay(uint32_t us)
{
    uint32_t startTick = DWT->CYCCNT;
    uint32_t delayTicks = us * (SystemCoreClock / 1000000);

    while ((DWT->CYCCNT - startTick) < delayTicks);
}

void HUB75E_DelayUs(int us)
{
    DWT_Delay(us);
}

void HUB75_Init(void)
{
    Paint_NewImage(LED_Buffer);
    HUB75_Clear();
    RGB_OE(1);   // 출력 비활성
    RGB_LAT(0);
    RGB_CLK(0);
}

// ISR 전용 갱신 함수
void HUB75_RefreshStep_ISR(void)
{
    static uint8_t row = 0;
    uint16_t x;

    uint8_t *top_ptr = &LED_Buffer[row * LED_Width];
    uint8_t *bot_ptr = &LED_Buffer[(row + (LED_Height / 2)) * LED_Width];

    RGB_OE(1);

    for (x = 0; x < LED_Width; x++) {
        uint8_t top = top_ptr[x];
        uint8_t bot = bot_ptr[x];

        RGB_R1((top & 0x01) ? 1 : 0);
        RGB_G1((top & 0x02) ? 1 : 0);
        RGB_B1((top & 0x04) ? 1 : 0);

        RGB_R2((bot & 0x01) ? 1 : 0);
        RGB_G2((bot & 0x02) ? 1 : 0);
        RGB_B2((bot & 0x04) ? 1 : 0);

        RGB_CLK(0);
        RGB_CLK(1);
    }

    RGB_A((row & 0x01) ? 1 : 0);
    RGB_B((row & 0x02) ? 1 : 0);
    RGB_C((row & 0x04) ? 1 : 0);
    RGB_D((row & 0x08) ? 1 : 0);
    RGB_E((row & 0x10) ? 1 : 0);

    RGB_LAT(1);
    RGB_LAT(0);

    RGB_OE(0);

    row++;
    if (row >= (LED_Height / 2)) {
        row = 0;
    }
}


/**
 * row 1개만 출력하는 step 함수
 * main loop에서 자주 호출
 */
void HUB75_RefreshStep(void)
{
    static uint8_t row = 0;
    uint16_t x;

    /* 이전 row 출력 잠시 off */
    RGB_OE(1);

    /* 현재 row 데이터 shift */
    for (x = 0; x < LED_Width; x++) {
        uint8_t top = LED_Buffer[x + (row * LED_Width)];
        uint8_t bot = LED_Buffer[x + ((row + (LED_Height / 2)) * LED_Width)];

        RGB_R1((top & 0x01) ? 1 : 0);
        RGB_G1((top & 0x02) ? 1 : 0);
        RGB_B1((top & 0x04) ? 1 : 0);

        RGB_R2((bot & 0x01) ? 1 : 0);
        RGB_G2((bot & 0x02) ? 1 : 0);
        RGB_B2((bot & 0x04) ? 1 : 0);

        RGB_CLK(0);
        RGB_CLK(1);
    }

    /* row address 지정 */
    RGB_A((row & 0x01) ? 1 : 0);
    RGB_B((row & 0x02) ? 1 : 0);
    RGB_C((row & 0x04) ? 1 : 0);
    RGB_D((row & 0x08) ? 1 : 0);
    RGB_E((row & 0x10) ? 1 : 0);

    /* latch */
    RGB_LAT(1);
    RGB_LAT(0);

    /* 현재 row 표시 on
       busy wait 없이 다음 호출 전까지 보이게 둠 */
    RGB_OE(0);

    row++;
    if (row >= (LED_Height / 2)) {
        row = 0;
    }
}

/* 기존 함수는 남겨둬도 되지만 사용하지 않는 걸 권장 */
void RGBMatrixWriteData(void)
{
    /* no longer used */
}

void HUB75_Display(void)
{
    /* no longer used */
}

void HUB75_Clear(void)
{
    memset(LED_Buffer, 0, LED_Width * LED_Height);
}

void scrollRight(void)
{
    UBYTE tempBuffer[LED_Height];
    UWORD row, col;

    for (row = 0; row < LED_Height; row++) {
        tempBuffer[row] = LED_Buffer[(row * LED_Width) + 0];
    }

    for (col = 0; col < LED_Width - 1; col++) {
        for (row = 0; row < LED_Height; row++) {
            LED_Buffer[(row * LED_Width) + col] =
                LED_Buffer[(row * LED_Width) + (col + 1)];
        }
    }

    for (row = 0; row < LED_Height; row++) {
        LED_Buffer[(row * LED_Width) + (LED_Width - 1)] = tempBuffer[row];
    }
}

void scrollLeft(void)
{
    UBYTE tempBuffer[LED_Height];
    UWORD row, col;

    for (row = 0; row < LED_Height; row++) {
        tempBuffer[row] = LED_Buffer[(row * LED_Width) + LED_Width - 1];
    }

    for (col = LED_Width - 1; col > 0; col--) {
        for (row = 0; row < LED_Height; row++) {
            LED_Buffer[(row * LED_Width) + col] =
                LED_Buffer[(row * LED_Width) + (col - 1)];
        }
    }

    for (row = 0; row < LED_Height; row++) {
        LED_Buffer[(row * LED_Width)] = tempBuffer[row];
    }
}

void scrollDown(void)
{
    UBYTE tempBuffer[LED_Width];
    UWORD row, col;

    for (col = 0; col < LED_Width; col++) {
        tempBuffer[col] = LED_Buffer[col];
    }

    for (row = 0; row < LED_Height - 1; row++) {
        for (col = 0; col < LED_Width; col++) {
            LED_Buffer[(row * LED_Width) + col] =
                LED_Buffer[((row + 1) * LED_Width) + col];
        }
    }

    for (col = 0; col < LED_Width; col++) {
        LED_Buffer[((LED_Height - 1) * LED_Width) + col] = tempBuffer[col];
    }
}

void scrollUp(void)
{
    UBYTE tempBuffer[LED_Width];
    UWORD row, col;

    for (col = 0; col < LED_Width; col++) {
        tempBuffer[col] = LED_Buffer[((LED_Height - 1) * LED_Width) + col];
    }

    for (row = LED_Height - 1; row > 0; row--) {
        for (col = 0; col < LED_Width; col++) {
            LED_Buffer[(row * LED_Width) + col] =
                LED_Buffer[((row - 1) * LED_Width) + col];
        }
    }

    for (col = 0; col < LED_Width; col++) {
        LED_Buffer[col] = tempBuffer[col];
    }
}
