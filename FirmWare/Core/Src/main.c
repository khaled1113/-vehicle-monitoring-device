/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include "mpu6050.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
char buffer[100];
MPU6050_t MPU6050;
/* USER CODE END PV */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C2_Init();

  /* USER CODE BEGIN 2 */
  MPU6050_Init(&hi2c2); // Initialize MPU6050 on I2C2
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN WHILE */
    MPU6050_Read_All(&hi2c2, &MPU6050);

    // Format output: Accel (g), Gyro (deg/s), Temp (°C)
    sprintf(buffer, "Ax: %.2f Ay: %.2f Az: %.2f | Gx: %.2f Gy: %.2f Gz: %.2f | T: %.2f\r\n",
            MPU6050.Ax, MPU6050.Ay, MPU6050.Az,
            MPU6050.Gx, MPU6050.Gy, MPU6050.Gz,
            MPU6050.Temp);

    HAL_UART_Transmit(&huart2, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

    HAL_Delay(500);
    /* USER CODE END WHILE */
  }
}

/**
  * @brief I2C2 Initialization Function (PB10=SCL, PB3=SDA)
  */
static void MX_I2C2_Init(void)
{
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

  /** I2C2 GPIO Configuration
    PB10 ------> I2C2_SCL
    PB3  ------> I2C2_SDA
  */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/**
  * @brief USART2 Initialization Function (PA2=TX, PA3=RX)
  */
static void MX_USART2_UART_Init(void)
{
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
}
