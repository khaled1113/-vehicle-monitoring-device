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
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// MPU6050 I2C address and register definitions
#define MPU6050_ADDR 0xD0  // 0x68 << 1 for HAL library
#define WHO_AM_I_REG 0x75
#define PWR_MGMT_1_REG 0x6B
#define SMPLRT_DIV_REG 0x19
#define CONFIG_REG 0x1A
#define GYRO_CONFIG_REG 0x1B
#define ACCEL_CONFIG_REG 0x1C
#define ACCEL_XOUT_H_REG 0x3B
#define TEMP_OUT_H_REG 0x41
#define GYRO_XOUT_H_REG 0x43
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void I2C_Scanner(void);
void UART_Print(char* message);
void MPU6050_Init(void);
void MPU6050_Read_All(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_I2C2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  // Print startup message
  UART_Print("\r\n=== STM32 I2C2 Scanner & MPU6050 Reader ===\r\n");
  UART_Print("Scanning I2C2 bus...\r\n");
  UART_Print("SCL: PB10, SDA: PB3\r\n");
  UART_Print("UART2: TX-PA2, RX-PA3\r\n\r\n");

  // Perform I2C scan
  I2C_Scanner();

  // Initialize MPU6050 if found
  HAL_Delay(1000);
  UART_Print("\r\n--- Initializing MPU6050 ---\r\n");
  MPU6050_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // Read MPU6050 data every 2 seconds
    HAL_Delay(2000);
    UART_Print("\r\n--- Reading MPU6050 Data ---\r\n");
    MPU6050_Read_All();
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
  * @brief  Scan I2C bus for connected devices
  * @retval None
  */
void I2C_Scanner(void)
{
    char buffer[100];
    uint8_t devices_found = 0;
    HAL_StatusTypeDef result;

    UART_Print("Starting I2C scan...\r\n");
    UART_Print("Address  Status\r\n");
    UART_Print("-------  ------\r\n");

    for(uint8_t address = 1; address < 128; address++)
    {
        // HAL_I2C_IsDeviceReady checks if device responds to its address
        result = HAL_I2C_IsDeviceReady(&hi2c2, (address << 1), 1, 10);

        if(result == HAL_OK)
        {
            sprintf(buffer, "0x%02X     Found!\r\n", address);
            UART_Print(buffer);
            devices_found++;
        }
        else if(result == HAL_ERROR)
        {
            // Uncomment next line if you want to see all tested addresses
            // sprintf(buffer, "0x%02X     No response\r\n", address);
            // UART_Print(buffer);
        }
    }

    if(devices_found == 0)
    {
        UART_Print("\r\nNo I2C devices found!\r\n");
        UART_Print("Check connections:\r\n");
        UART_Print("- SCL connected to PB10\r\n");
        UART_Print("- SDA connected to PB3\r\n");
        UART_Print("- Pull-up resistors on SDA/SCL (4.7kΩ)\r\n");
        UART_Print("- Device power supply\r\n");
    }
    else
    {
        sprintf(buffer, "\r\nScan complete! Found %d device(s).\r\n", devices_found);
        UART_Print(buffer);
    }
}

/**
  * @brief  Send string via UART2
  * @param  message: string to send
  * @retval None
  */
void UART_Print(char* message)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)message, strlen(message), HAL_MAX_DELAY);
}

/**
  * @brief  Initialize MPU6050 sensor
  * @retval None
  */
void MPU6050_Init(void)
{
    uint8_t check;
    uint8_t data;
    char buffer[100];

    // Check WHO_AM_I register (should return 0x68)
    HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, WHO_AM_I_REG, 1, &check, 1, HAL_MAX_DELAY);

    if (check == 0x68) // MPU6050 WHO_AM_I value
    {
        sprintf(buffer, "MPU6050 detected! WHO_AM_I = 0x%02X\r\n", check);
        UART_Print(buffer);

        // Wake up the MPU6050 (clear sleep bit in power management register)
        data = 0x00;
        HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, PWR_MGMT_1_REG, 1, &data, 1, HAL_MAX_DELAY);

        // Set sample rate to 1kHz
        data = 0x07;
        HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, SMPLRT_DIV_REG, 1, &data, 1, HAL_MAX_DELAY);

        // Configure accelerometer (+/- 2g)
        data = 0x00;
        HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, ACCEL_CONFIG_REG, 1, &data, 1, HAL_MAX_DELAY);

        // Configure gyroscope (+/- 250 deg/s)
        data = 0x00;
        HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, GYRO_CONFIG_REG, 1, &data, 1, HAL_MAX_DELAY);

        UART_Print("MPU6050 initialized successfully!\r\n");
    }
    else
    {
        sprintf(buffer, "MPU6050 not found! WHO_AM_I = 0x%02X\r\n", check);
        UART_Print(buffer);
    }
}

/**
  * @brief  Read all MPU6050 data (accelerometer, gyroscope, temperature)
  * @retval None
  */
void MPU6050_Read_All(void)
{
    uint8_t rec_data[14];
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
    int16_t temperature;
    char buffer[200];

    // Read 14 bytes starting from accelerometer high byte register
    HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, ACCEL_XOUT_H_REG, 1, rec_data, 14, HAL_MAX_DELAY);

    // Combine high and low bytes for each reading
    accel_x = (int16_t)(rec_data[0] << 8 | rec_data[1]);
    accel_y = (int16_t)(rec_data[2] << 8 | rec_data[3]);
    accel_z = (int16_t)(rec_data[4] << 8 | rec_data[5]);

    temperature = (int16_t)(rec_data[6] << 8 | rec_data[7]);

    gyro_x = (int16_t)(rec_data[8] << 8 | rec_data[9]);
    gyro_y = (int16_t)(rec_data[10] << 8 | rec_data[11]);
    gyro_z = (int16_t)(rec_data[12] << 8 | rec_data[13]);

    // Convert raw values to meaningful units
    // Accelerometer: ±2g range, 16384 LSB/g
    float accel_x_g = accel_x / 16384.0;
    float accel_y_g = accel_y / 16384.0;
    float accel_z_g = accel_z / 16384.0;

    // Gyroscope: ±250°/s range, 131 LSB/(°/s)
    float gyro_x_dps = gyro_x / 131.0;
    float gyro_y_dps = gyro_y / 131.0;
    float gyro_z_dps = gyro_z / 131.0;

    // Temperature: (TEMP_OUT/340 + 36.53) °C
    float temp_c = (temperature / 340.0) + 36.53;

    // Print raw data
    sprintf(buffer, "Raw Data:\r\nAccel: X=%6d Y=%6d Z=%6d\r\n", accel_x, accel_y, accel_z);
    UART_Print(buffer);
    sprintf(buffer, "Gyro:  X=%6d Y=%6d Z=%6d\r\n", gyro_x, gyro_y, gyro_z);
    UART_Print(buffer);
    sprintf(buffer, "Temp:  %6d\r\n", temperature);
    UART_Print(buffer);

    // Print converted data
    sprintf(buffer, "\r\nConverted Data:\r\nAccel: X=%6.2fg Y=%6.2fg Z=%6.2fg\r\n",
            accel_x_g, accel_y_g, accel_z_g);
    UART_Print(buffer);
    sprintf(buffer, "Gyro:  X=%7.1f° Y=%7.1f° Z=%7.1f° (deg/s)\r\n",
            gyro_x_dps, gyro_y_dps, gyro_z_dps);
    UART_Print(buffer);
    sprintf(buffer, "Temp:  %.1f°C\r\n", temp_c);
    UART_Print(buffer);
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
