/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "adc.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdbool.h"
#include "stdlib.h"
#include "analog.h"
#include "rgb.h"
#include "keyboard.h"
#include "keyboard_conf.h"
#include "math.h"
#include "lfs.h"
#include "command.h"
#include "keyboard_def.h"
#include "usbd_user.h"
#include "command.h"
#include "packet.h"
#include "sfud.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define rgb_start() HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_2, (uint32_t *)g_rgb_buffer, RGB_BUFFER_LENGTH);
#define rgb_stop() HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_2);

#define DMA_BUF_LEN             8
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
sfud_flash sfud_norflash0 = {
  .name = "norflash0",
  .spi.name = "SPI1",
  .chip = {"W25Q128JV", SFUD_MF_ID_WINBOND, 0x40, 0x18, 16L * 1024L * 1024L, SFUD_WM_PAGE_256B, 4096, 0x20},
};

uint32_t ADC_Buffer[4*DMA_BUF_LEN];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// 重定向print start
#if defined (__ARMCC_VERSION) /* ARM Compiler */
int fputc(int ch, FILE *f)
{
  while ((USART1->ISR & USART_ISR_TXE) == 0)
      ;
  USART1->TDR = ch;
  return ch;
}
#elif defined ( __GNUC__ ) && !defined (__CC_ARM) /* GNU Compiler */
//_write函數在syscalls.c中， 使用__weak定义以可以直接在其他文件中定义_write函數
__attribute__((weak)) int _write(int file, char *ptr, int len)
{
  UNUSED(file);
  if (HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, 0xffff) != HAL_OK)
  {
    Error_Handler();
  }
  return HAL_OK;
}
#endif
// 重定向print end

/**
 * @brief  初始化时间戳
 * @note   使用延时函数前，必须调用本函数
 */
int DWT_Init(void)
{
  /* Disable TRC */
  CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk; // ~0x01000000;
  /* Enable TRC */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // 0x01000000;

  /* Disable clock cycle counter */
  DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk; //~0x00000001;
  /* Enable  clock cycle counter */
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; // 0x00000001;

  /* Reset the clock cycle counter value */
  DWT->CYCCNT = 0;

  /* 3 NO OPERATION instructions */
  __ASM volatile("NOP");
  __ASM volatile("NOP");
  __ASM volatile("NOP");

  /* Check if clock cycle counter has started */
  if (DWT->CYCCNT)
  {
    return 0; /*clock cycle counter started*/
  }
  else
  {
    return 1; /*clock cycle counter not started*/
  }
}
// This Function Provides Delay In Microseconds Using DWT

void DWT_Delay_us(volatile uint32_t au32_microseconds)
{
  uint32_t au32_initial_ticks = DWT->CYCCNT;
  uint32_t au32_ticks = (HAL_RCC_GetHCLKFreq() / 1000000);
  au32_microseconds *= au32_ticks;
  while ((DWT->CYCCNT - au32_initial_ticks) < au32_microseconds - au32_ticks)
    ;
}

void (*SysMemBootJump)(void);
__IO uint32_t BootAddr = 0x1FFFD800; /* BootLoader 地址 */

void JumpToBootloader(void)
{
  uint32_t i = 0;

  /* 关闭全局中断 */
  __set_PRIMASK(1);

  /* 关闭滴答定时器，复位到默认值 */
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;

  /* 设置所有时钟到默认状态，使用HSI时钟 */
  HAL_RCC_DeInit();

  /* 关闭所有中断，清除所有中断挂起标志 */
  for (i = 0; i < 8; i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFF;
    NVIC->ICPR[i] = 0xFFFFFFFF;
  }

  /* 使能全局中断 */
  __set_PRIMASK(0);

  /* 跳转到系统BootLoader，首地址是MSP，地址+4是复位中断服务程序地址 */
  SysMemBootJump = (void (*)(void))(*((uint32_t *)(BootAddr + 4)));

  /* 设置主堆栈指针 */
  __set_MSP(*(uint32_t *)BootAddr);

  /* 跳转到系统BootLoader */
  SysMemBootJump();

  /* 跳转成功的话，不会执行到这里，用户可以在这里添加代码 */
  while (1)
  {
  }
}

void usb_dc_low_level_init(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  /* USER CODE BEGIN USB_MspInit 0 */

  /* USER CODE END USB_MspInit 0 */

  /** Initializes the peripherals clocks
  */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
  /* USB clock enable */
  __HAL_RCC_USB_CLK_ENABLE();
  /* USB interrupt Init */
  HAL_NVIC_SetPriority(USB_LP_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USB_LP_IRQn);
  /* USER CODE BEGIN USB_MspInit 1 */

  /* USER CODE END USB_MspInit 1 */
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
  MX_SPI1_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_ADC4_Init();
  MX_TIM3_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  MX_TIM7_Init();
  /* USER CODE BEGIN 2 */
  DWT_Init();
  sfud_device_init(&sfud_norflash0);
  usb_init();
  keyboard_init();

  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
  HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
  HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED);
  HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED);
  rgb_start();
  rgb_init_flash();

  HAL_ADC_Start_DMA(&hadc1, ADC_Buffer + DMA_BUF_LEN*0, DMA_BUF_LEN);
  HAL_ADC_Start_DMA(&hadc2, ADC_Buffer + DMA_BUF_LEN*1, DMA_BUF_LEN);
  HAL_ADC_Start_DMA(&hadc3, ADC_Buffer + DMA_BUF_LEN*2, DMA_BUF_LEN);
  HAL_ADC_Start_DMA(&hadc4, ADC_Buffer + DMA_BUF_LEN*3, DMA_BUF_LEN);
  
  HAL_TIM_Base_Start_IT(&htim2);

  HAL_Delay(100);

  analog_reset_range();
  
  HAL_TIM_Base_Start_IT(&htim7);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    rgb_update();
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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV4;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM7)
  {
    keyboard_scan();
    for (uint8_t i = 0; i < ANALOG_BUFFER_LENGTH; i++)
    {
        g_ADC_Averages[i] = ringbuf_avg(&adc_ringbuf[i]);
#ifdef ENABLE_FILTER
        g_ADC_Averages[i] = adaptive_schimidt_filter(g_analog_filters+i,g_ADC_Averages[i]);
#endif
      if ((uint16_t)~g_analog_map[i])
      {
        AdvancedKey* key = &g_keyboard_advanced_keys[g_analog_map[i]];
        if (key->config.mode != KEY_DIGITAL_MODE)
        {
            advanced_key_update_raw(key, g_ADC_Averages[i]);
        }
      }
    }
    switch (g_keyboard_state)
    {
    case KEYBOARD_STATE_DEBUG:
      send_debug_info();
      break;
    case KEYBOARD_STATE_UPLOAD_CONFIG:
      if (!load_cargo())
      {
        g_keyboard_state = KEYBOARD_STATE_IDLE;
      }
      break;
    default:
      keyboard_send_report();
      break;
    }
  }
  if (htim->Instance == TIM2)
  {
    static uint32_t adc[8] = {0};
    memset(adc, 0 ,sizeof(adc));
    for (int i = 0; i < DMA_BUF_LEN/2; i++)
    {
      adc[0] += ADC_Buffer[DMA_BUF_LEN * 0 + i * 2];
      adc[1] += ADC_Buffer[DMA_BUF_LEN * 0 + i * 2 + 1];
      adc[2] += ADC_Buffer[DMA_BUF_LEN * 1 + i * 2];
      adc[3] += ADC_Buffer[DMA_BUF_LEN * 1 + i * 2 + 1];
      adc[4] += ADC_Buffer[DMA_BUF_LEN * 2 + i * 2];
      adc[5] += ADC_Buffer[DMA_BUF_LEN * 2 + i * 2 + 1];
      adc[6] += ADC_Buffer[DMA_BUF_LEN * 3 + i * 2];
      adc[7] += ADC_Buffer[DMA_BUF_LEN * 3 + i * 2 + 1];
    }

    ringbuf_push(&adc_ringbuf[0 + ADDRESS * 8], (float)adc[0] * (1/((float)DMA_BUF_LEN/2)));
    ringbuf_push(&adc_ringbuf[1 + ADDRESS * 8], (float)adc[1] * (1/((float)DMA_BUF_LEN/2)));
    ringbuf_push(&adc_ringbuf[2 + ADDRESS * 8], (float)adc[2] * (1/((float)DMA_BUF_LEN/2)));
    ringbuf_push(&adc_ringbuf[3 + ADDRESS * 8], (float)adc[3] * (1/((float)DMA_BUF_LEN/2)));
    ringbuf_push(&adc_ringbuf[4 + ADDRESS * 8], (float)adc[4] * (1/((float)DMA_BUF_LEN/2)));
    ringbuf_push(&adc_ringbuf[5 + ADDRESS * 8], (float)adc[5] * (1/((float)DMA_BUF_LEN/2)));
    ringbuf_push(&adc_ringbuf[6 + ADDRESS * 8], (float)adc[6] * (1/((float)DMA_BUF_LEN/2)));
    ringbuf_push(&adc_ringbuf[7 + ADDRESS * 8], (float)adc[7] * (1/((float)DMA_BUF_LEN/2)));

    if (htim->Instance->CNT < 1000)
    {
      g_analog_active_channel++;
      if (g_analog_active_channel >= ANALOG_CHANNEL_MAX)
      {
        g_analog_active_channel = 0;
      }
      analog_channel_select(g_analog_active_channel);
    }
  }
}


void rgb_update_callback()
{
	if(g_keyboard_led_state&BIT(1))
  {
	  g_rgb_colors[g_rgb_mapping[51]].r = 0xff;
	  g_rgb_colors[g_rgb_mapping[51]].g = 0xff;
	  g_rgb_colors[g_rgb_mapping[51]].b = 0xff;//cap lock
	}
	if(g_keyboard_led_state&BIT(2))
  {
	  g_rgb_colors[g_rgb_mapping[15]].r = 0xff;
	  g_rgb_colors[g_rgb_mapping[15]].g = 0xff;
	  g_rgb_colors[g_rgb_mapping[15]].b = 0xff;//cap lock
	}
}

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
  while (1)
  {
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
