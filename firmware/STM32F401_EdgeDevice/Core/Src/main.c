/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "fatfs.h"
#include "i2s.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "spi_sdcard.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
	char riff[4];
	uint32_t size;
	char wave[4];

	char fmt[4];
	uint32_t fmt_size;
	uint16_t format;
	uint16_t channels;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t align;
	uint16_t bits;

	char data[4];
	uint32_t data_size;
} WAV_Header;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// printf <-> UART
int _write(int file, char *ptr, int len) {
	HAL_UART_Transmit(&huart2, (uint8_t*) ptr, len, 200);
	return len;
}

#define BUF_SAMPLES 8192

static int16_t mono_buf[BUF_SAMPLES];
static int16_t stereo_buf[BUF_SAMPLES * 2]; // L,R interleaved

void PlayWav(const char *name) {
	FIL file;
	WAV_Header header;
	UINT br;

	if (f_open(&file, name, FA_READ) != FR_OK) {
		printf("open fail\r\n");
		return;
	}

	// ?��?�� 44바이?�� ?���????????????(PCM ?���???????????? WAV �?????????????��)
	f_read(&file, &header, 44, &br);
	printf("fmt=%d ch=%d bits=%d rate=%ld size=%ld\n", header.format,
			header.channels, header.bits, header.sample_rate, header.data_size);

	// 최소 �????????????�???????????? (?��?��?���???????????? ?�� 추�?)
	if (header.format != 1 || header.bits != 16) {
		printf("wav fmt error\r\n");
		f_close(&file);
		return;
	}

	// data ?��?��?���???????????? ?��?��
	f_lseek(&file, 44);

	while (1) {
		// WAV 채널?�� ?��?�� ?���???????????? ?���???????????? 결정
		if (header.channels == 1) {
			// mono: 16bit * BUF
		    uint32_t t0 = HAL_GetTick();
		    f_read(&file, mono_buf, BUF_SAMPLES * 2, &br);
		    uint32_t dt = HAL_GetTick() - t0;

		    printf("4096B read: %ld ms\r\n", dt);

			if (br == 0)
				break;

			int n = br / 2; // mono samples

			// mono -> stereo 복제
			for (int i = 0; i < n; i++) {
				int16_t v = mono_buf[i];
				stereo_buf[2 * i] = v; // L
				stereo_buf[2 * i + 1] = v; // R
			}
			// ?��?�� �????????????�???????????? 0 ?��?��
			for (int i = n; i < BUF_SAMPLES; i++) {
				stereo_buf[2 * i] = 0;
				stereo_buf[2 * i + 1] = 0;
			}

		} else {
			printf("unsupported channels=%d\r\n", header.channels);
			break;
		}

		// I2S 16on16: Size?�� 16-bit halfword 개수
		// stereo_buf?�� 16-bit * (BUF_SAMPLES*2)
		HAL_I2S_Transmit(&hi2s3, (uint16_t*) stereo_buf,
		BUF_SAMPLES * 2, 200);
	}

	f_close(&file);
	printf("play end\r\n");
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_SPI2_Init();
  MX_FATFS_Init();
  MX_I2S3_Init();
  /* USER CODE BEGIN 2 */
	uint8_t buf[512];

	if (sd_init() == 0) {
		printf("SD init OK\r\n");

		if (sd_read_block(0, buf) == 0){
			printf("Read OK\r\n");

			hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4; // or _4
			HAL_SPI_Init(&hspi2);
		}
	}

	FRESULT res;

	res = f_mount(&USERFatFS, USERPath, 1);
	printf("mount = %d\r\n", res);

	sd_files();

	printf("voice start\r\n");

	PlayWav("voice_1.wav");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
		HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);

		HAL_Delay(500);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
