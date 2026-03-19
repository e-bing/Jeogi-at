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

    /* 출력 비활성 */
    RGB_OE_HIGH();

    /* 현재 row 데이터 shift */
    for (x = 0; x < LED_Width; x++)
    {
        uint8_t top = top_ptr[x];
        uint8_t bot = bot_ptr[x];

        if (top & 0x01) RGB_R1_HIGH(); else RGB_R1_LOW();
        if (top & 0x02) RGB_G1_HIGH(); else RGB_G1_LOW();
        if (top & 0x04) RGB_B1_HIGH(); else RGB_B1_LOW();

        if (bot & 0x01) RGB_R2_HIGH(); else RGB_R2_LOW();
        if (bot & 0x02) RGB_G2_HIGH(); else RGB_G2_LOW();
        if (bot & 0x04) RGB_B2_HIGH(); else RGB_B2_LOW();

        RGB_CLK_LOW();
        RGB_CLK_HIGH();
    }

    /* row address 설정 */
    if (row & 0x01) RGB_A_HIGH(); else RGB_A_LOW();
    if (row & 0x02) RGB_B_HIGH(); else RGB_B_LOW();
    if (row & 0x04) RGB_C_HIGH(); else RGB_C_LOW();
    if (row & 0x08) RGB_D_HIGH(); else RGB_D_LOW();

    /* latch */
    RGB_LAT_HIGH();
    RGB_LAT_LOW();

    /* 출력 활성 */
    RGB_OE_LOW();

    row++;
    if (row >= (LED_Height / 2))
    {
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
