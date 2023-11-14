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
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef hlpuart1;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
uint8_t  tempH; //Last 8 bits of temperature data
uint8_t  tempL; //First 8 bits of temperature data
uint16_t fullTemp; // Fill temperature dats
uint8_t config = 0x01; // Address for CTRL register on temperature sensor
FATFS       FatFs;         //Fatfs handle
FIL         fil;           //File handle
FRESULT     fres;          //Result after operations
char        buf[100];      // for reading sd card data
FATFS *pfs;
DWORD fre_clust;
uint32_t totalSpace, freeSpace;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RTC_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t Rx_data[4];  //  creating a buffer of 10 bytes

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  HAL_UART_Receive_IT(&hlpuart1, Rx_data, 1);
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
  MX_SPI1_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */
  /* Blink the LED */
  //HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, GPIO_PIN_SET);
  	for (int i=0; i<5; i++)
  	{
  		HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
  		HAL_Delay(500);
  	}
    fres = f_mount(&FatFs, "", 1);    //1=mount now
  	if (fres != FR_OK)
  	{
  		char str[80];
  		sprintf(str, "No SD Card found : (%i)\r\n", fres);
  		HAL_UART_Transmit(&hlpuart1, (uint8_t *)str, strlen (str), HAL_MAX_DELAY);
  	}else{
  		char *str = "SD Card Mounted Successfully!!!\r\n";
  		HAL_UART_Transmit(&hlpuart1, (uint8_t *)str, strlen (str), HAL_MAX_DELAY);
  	}
  	if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) != RESET) //
  	{
  		__HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);  // clear the flag

  		/** display  the string **/
  		char *str = "Wakeup from the STANDBY MODE\n\n";
  		HAL_UART_Transmit(&hlpuart1, (uint8_t *)str, strlen (str), HAL_MAX_DELAY);

  		/** Disable the WWAKEUP PIN **/
  		HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1);  // disable PA0

  		/** Deactivate the RTC wakeup  **/
  		HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

  		// TEMPERATURE CODE //
  	  	// Device Address is the manufacturer set slave address shifted left + 1
  	  	if (HAL_I2C_IsDeviceReady(&hi2c1,0x79,1,100) == HAL_OK){ // Check if temp sensor detected
  	  		char *str = "I2C Device Detected\n\n";
  	  		HAL_UART_Transmit(&hlpuart1, (uint8_t *)str, strlen (str), HAL_MAX_DELAY);
  	  		// Write config to CTRL register in temp sensor
  	  		HAL_I2C_Mem_Write(&hi2c1, 0x78, 0x04, 1, &config, 1, HAL_MAX_DELAY);
  	  		//HAL_Delay(10);
  	  		// Write it again to make sure
  	  		HAL_I2C_Mem_Write(&hi2c1, 0x78, 0x04, 1, &config, 1, HAL_MAX_DELAY);
  	  		//HAL_Delay(10);
  	  		// Read data from both temperature registers
  	  		HAL_I2C_Mem_Read(&hi2c1, 0x79 | 0x01, 0x06, 1, &tempL, 1, HAL_MAX_DELAY);
  	  		HAL_I2C_Mem_Read(&hi2c1, 0x79 | 0x01, 0x07, 1, &tempH, 1, HAL_MAX_DELAY);
  	  		char str2[80];
  	  		fullTemp =((uint16_t)tempH << 8) | tempL; // concatenate temperatures
  	  		sprintf(str2, "temperature = %d \n", fullTemp/100);
  	  		HAL_UART_Transmit(&hlpuart1, (uint8_t *)str2, strlen (str2), HAL_MAX_DELAY);
  	  	}
  	  	// SD CARD CODE //
  	    f_getfree("", &fre_clust, &pfs);
  	    totalSpace = (uint32_t)((pfs->n_fatent - 2) * pfs->csize * 0.5);
  	    freeSpace = (uint32_t)(fre_clust * pfs->csize * 0.5);
  	    char str[80];
  	    sprintf(str,"TotalSpace : %lu bytes, FreeSpace = %lu bytes\n", totalSpace, freeSpace);
  	    HAL_UART_Transmit(&hlpuart1, (uint8_t *)str, strlen (str), HAL_MAX_DELAY);
  	    // WRITE DATA //
  	    fres = f_open(&fil, "tData.txt", FA_WRITE | FA_READ | FA_CREATE_ALWAYS);
  	    if(fres == FR_OK)
  	    {
  	    	//write the data
  	    	char str[2];
  	    	sprintf(str,"%d\n", fullTemp);
  	        f_puts(str, &fil);
  	        //close the file
  	        f_close(&fil);
  	        // unmount drive
  	        f_mount(NULL, "", 0);
  	        char *str = "File written and closed.\n\n";
  	        HAL_UART_Transmit(&hlpuart1, (uint8_t *)str, strlen (str), HAL_MAX_DELAY);
  	        //Check if drive can remain mounted in standby mode
  	    }else{
  	    	char *str[80];
  	    	sprintf(str,"File creation/open Error : (%i)\r\n", fres);
  	    	HAL_UART_Transmit(&hlpuart1, (uint8_t *)str, strlen (str), HAL_MAX_DELAY);
  	    }
  	}
  	/** Now enter the standby mode **/
  	/* Clear the WU FLAG */
  	__HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

  	/* clear the RTC Wake UP (WU) flag */
  	__HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);

  	/* Display the string */
  	char *str = "About to enter the STANDBY MODE\n\n";
  	HAL_UART_Transmit(&hlpuart1, (uint8_t *)str, strlen (str), HAL_MAX_DELAY);

  	/* Enable the WAKEUP PIN */
  	HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);
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

  	/* one last string to be sure */
  	char *str2 = "STANDBY MODE is ON\n\n";
  	HAL_UART_Transmit(&hlpuart1, (uint8_t *)str2, strlen (str2), HAL_MAX_DELAY);

  	/* Finally enter the standby mode */
  	HAL_PWR_EnterSTANDBYMode();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
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
  hi2c1.Init.Timing = 0x00000103;
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
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_4BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
