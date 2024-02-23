/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h> //for va_list var arg functions
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FLASH_USER_START_ADDR   ADDR_FLASH_PAGE_16   /* Start @ of user Flash area */
#define FLASH_USER_END_ADDR     ADDR_FLASH_PAGE_127 + FLASH_PAGE_SIZE - 1   /* End @ of user Flash area */

#define DATA_64                 ((uint64_t)0x1234567812345678)
#define DATA_32                 ((uint32_t)0x12345678)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef hlpuart1;

RTC_HandleTypeDef hrtc;

/* USER CODE BEGIN PV */
uint8_t  tempH, tempL; //Temperature data is read in 2 8-bit parts
uint8_t storeCounter=3; //store intermediate values in backup reg for more efficiency
uint16_t fullTemp; // Full temperature data
uint64_t writeVal; // Value to write to flash storage
uint32_t bkWrite; // Value to write to backup register
uint8_t config = 0x01; // Address for CTRL register on temperature sensor
RTC_TimeTypeDef gTime;
//FLASH MEMORY STUFF
uint32_t FirstPage = 0, NbOfPages = 0, BankNumber = 0;
uint32_t Address = 0, PAGEError = 0;
__IO uint32_t data32 = 0, MemoryProgramStatus = 0;
__IO uint64_t data64 = 0;
static FLASH_EraseInitTypeDef EraseInitStruct;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RTC_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
void UART_TRANSMIT(char *str);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == GPIO_PIN_14)
  {
	  SystemClock_Config ();
	  HAL_ResumeTick();
	  char *str = "WAKEUP FROM EXTII\n\n";
	  HAL_UART_Transmit(&hlpuart1, (uint8_t *) str, strlen (str), HAL_MAX_DELAY);
	  HAL_PWR_DisableSleepOnExit();
  }
}
void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc){
	SystemClock_Config ();
	HAL_ResumeTick();
	char *str = "WAKEUP FROM RTC\n\n";
	HAL_UART_Transmit(&hlpuart1, (uint8_t *) str, strlen (str), HAL_MAX_DELAY);
	HAL_PWR_DisableSleepOnExit();
}
void myprintf(const char *fmt, ...) {
  static char buffer[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  int len = strlen(buffer);
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)buffer, len, -1);
}
static uint32_t GetPage(uint32_t Addr)
{
  uint32_t page = 0;

  if (Addr < (FLASH_BASE + FLASH_BANK_SIZE))
  {
    /* Bank 1 */
    page = (Addr - FLASH_BASE) / FLASH_PAGE_SIZE;
  }
  else
  {
    /* Bank 2 */
    page = (Addr - (FLASH_BASE + FLASH_BANK_SIZE)) / FLASH_PAGE_SIZE;
  }

  return page;
}

static uint32_t GetBank(uint32_t Addr)
{
  return FLASH_BANK_1;
}

void Flash_Read_Data (uint32_t StartPageAddress, uint32_t *RxBuf, uint16_t numberofwords)
{
	while (1)
	{

		*RxBuf = *(__IO uint32_t *)StartPageAddress;
		StartPageAddress += 4;
		RxBuf++;
		if (!(numberofwords--)) break;
	}
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
  MX_RTC_Init();
  MX_LPUART1_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(1000);
  for (int i=0; i<11; i++)
  {
	  HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
	  HAL_Delay(200);
  }
  myprintf("BEGIN MAIN LOOP\n\n");
   /* enable the RTC Wakeup */
     /*  RTC Wake-up Interrupt Generation:
       Wake-up Time Base = (RTC_WAKEUPCLOCK_RTCCLK_DIV /(LSI))
       ==> WakeUpCounter = Wake-up Time / Wake-up Time Base

       To configure the wake up timer to 5s the WakeUpCounter is set to 0x2710:
       RTC_WAKEUPCLOCK_RTCCLK_DIV = RTCCLK_Div16 = 16
       Wake-up Time Base = 16 /(32KHz) = 0.0005 seconds
       ==> WakeUpCounter = ~5s/0.0005s = 20000 = 0x2710
     */
  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0x2710, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
  {
	  Error_Handler();
  }
  //Now Erase the Flash memory we will use//
  /* Unlock the Flash to enable the flash control register access *************/
  HAL_FLASH_Unlock();

  /* Erase the user Flash area
        (area defined by FLASH_USER_START_ADDR and FLASH_USER_END_ADDR) ***********/

  /* Clear OPTVERR bit set on virgin samples */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);
  /* Get the 1st page to erase */
  FirstPage = GetPage(FLASH_USER_START_ADDR);
  /* Get the number of pages to erase from 1st page */
  NbOfPages = GetPage(FLASH_USER_END_ADDR) - FirstPage + 1;
  /* Get the bank */
  BankNumber = GetBank(FLASH_USER_START_ADDR);
  /* Fill EraseInit structure*/
  EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
  EraseInitStruct.Banks       = BankNumber;
  EraseInitStruct.Page        = FirstPage;
  EraseInitStruct.NbPages     = NbOfPages;
  myprintf("Starting Program\n");
  /* Note: If an erase operation in Flash memory also concerns data in the data or instruction cache,
         you have to make sure that these data are rewritten before they are accessed during code
         execution. If this cannot be done safely, it is recommended to flush the caches by setting the
         DCRST and ICRST bits in the FLASH_CR register. */
  if (HAL_FLASHEx_Erase(&EraseInitStruct, &PAGEError) != HAL_OK)
  {
  /*
  Error occurred while page erase.
  User can add here some code to deal with this error.
  PAGEError will contain the faulty page and then to know the code error on this page,
  user can call function 'HAL_FLASH_GetError()'
  */
	  while (1)
	  {
		  /* Make LED3 blink (100ms on, 2s off) to indicate error in Erase operation */
		  HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
		  HAL_Delay(100);
		  HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
		  HAL_Delay(2000);
		  myprintf("ERROR while erasing\n");
	  }
  }
  Address = FLASH_USER_START_ADDR;
  //Write current Address up backup register
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, Address);
  myprintf("Successful Erase! Ready to write...\n");
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, '*'); //Write to RTC backup register
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	if (storeCounter >= 3){
		storeCounter=0;
	}else{storeCounter++;}
	HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3); // Toggle LED on
	static uint32_t secondsPassed = 0; // keep track of time, assume RTC value is exact
	/* Get the RTC current Time */
	HAL_RTC_GetTime(&hrtc, &gTime, RTC_FORMAT_BIN);
	// Get current Address to write to //
	Address = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1);

	secondsPassed +=5;
	// Send time over serial for debugging
	myprintf("Time Passed: %u Seconds\nLast temp: %u\n", (unsigned int)secondsPassed, (unsigned int)5);
	// TEMPERATURE CODE //
	HAL_I2C_Mem_Write(&hi2c1, 0x79, 0x04, 1, &config, 1, HAL_MAX_DELAY);
	writeVal = 0;
	// Device Address is the manufacturer set slave address shifted left + 1
	if (HAL_I2C_IsDeviceReady(&hi2c1,0x79,1,1000) == HAL_OK){ // Check if temp sensor detected
		myprintf("I2C Device Detected\n\n");

	 	// Write config to CTRL register in temp sensor
	 	//HAL_Delay(10);
	 	HAL_I2C_Mem_Write(&hi2c1, 0x78, 0x04, 1, &config, 1, HAL_MAX_DELAY);

	 	// Read data from both temperature registers
	 	HAL_I2C_Mem_Read(&hi2c1, 0x79 | 0x01, 0x06, 1, &tempL, 1, HAL_MAX_DELAY);
	 	HAL_I2C_Mem_Read(&hi2c1, 0x79 | 0x01, 0x07, 1, &tempH, 1, HAL_MAX_DELAY);

	 	//Concatenate and Print temperature data
	 	fullTemp =((uint16_t)tempH << 8) | tempL;
	 	myprintf("temperature = %d \n", fullTemp/100);
	 	bkWrite |= ((uint32_t)((uint8_t)(fullTemp >> 4)) << storeCounter*8);
	 	bkWrite = (bkWrite | HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR2));
	}
	if (storeCounter!=3){
		writeVal = (uint64_t)HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR2);
	}else{
		if (Address < FLASH_USER_END_ADDR)
		{
			if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, Address, writeVal) == HAL_OK)
			{
				myprintf("%08" PRIx32 "\n",bkWrite);
				HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR2,0x00000000);
				Address = Address + 8;
			}
			else
			{
				myprintf("WRITING ERROR ERROR ERROR\n\n");
			}
		}
	}
	//Write current Address up backup register
	HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, Address);

/*    data64 = *(__IO uint64_t *)Address - 8;
    if (data64 != (uint64_t)fullTemp)
    {
    	myprintf("%" PRIx64 "read... Wrong thing written???\n\n", data64);
    } */
	myprintf("GOING INTO STOP MODE\n\n");
	/* Suspend tick before entering stop mode */
	HAL_SuspendTick();
	/* Sleep on Exit? */
	//HAL_PWR_EnableSleepOnExit();
	HAL_PWR_DisableSleepOnExit();
	HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
	/* Enter STOP mode */
	HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
	/* Wake from stop mode */
	SystemClock_Config();
	HAL_ResumeTick();
	/** Deactivate the RTC wakeup, we would do this when connected to PC**/
	//HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
	/*for (int j=0; j<6; j++)
	{
		HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
		HAL_Delay (200);
	}*/

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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV8;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00303D5B;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief LPUART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 38400;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */
  if(HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == '*') {
      //Time is OK. Set Alarms later.
      myprintf("RTC backup available\n");
      return;
  }
  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_SET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
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
