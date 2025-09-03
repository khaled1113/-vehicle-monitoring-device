/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * STM32F401RCT6 with SIM800L V2 - Phone Call Example
  * SIM800L connected to UART1 (PA9-TX, PA10-RX) at 9600 baud
  * FTDI connected to UART2 (PA2-TX, PA3-RX) at 115200 baud for debugging
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <string.h>
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PHONE_NUMBER "+201010423508"
#define RX_BUFFER_SIZE 256
#define TIMEOUT_MS 10000
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;  // SIM800L at 9600 baud
UART_HandleTypeDef huart2;  // FTDI Debug at 115200 baud

/* USER CODE BEGIN PV */
char rx_buffer[RX_BUFFER_SIZE];
char debug_msg[256];
uint8_t rx_data;
volatile uint16_t rx_index = 0;
volatile uint8_t rx_complete = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void send_at_command(char* command);
void debug_print(char* message);
void clear_rx_buffer(void);
uint8_t wait_for_response(char* expected_response, uint32_t timeout_ms);
void make_phone_call(char* number);
void sim800l_init(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// UART1 Receive Interrupt Callback
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1)
    {
        if(rx_index < RX_BUFFER_SIZE - 1)
        {
            rx_buffer[rx_index] = rx_data;
            rx_index++;

            // Check for end of response
            if(rx_data == '\n' || rx_index >= RX_BUFFER_SIZE - 1)
            {
                rx_buffer[rx_index] = '\0';
                rx_complete = 1;
            }
        }

        // Continue receiving
        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    }
}

void send_at_command(char* command)
{
    clear_rx_buffer();
    HAL_UART_Transmit(&huart1, (uint8_t*)command, strlen(command), 1000);
    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 1000);

    sprintf(debug_msg, "Sent: %s\r\n", command);
    debug_print(debug_msg);
}

void debug_print(char* message)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)message, strlen(message), 1000);
}

void clear_rx_buffer(void)
{
    memset(rx_buffer, 0, RX_BUFFER_SIZE);
    rx_index = 0;
    rx_complete = 0;
}

uint8_t wait_for_response(char* expected_response, uint32_t timeout_ms)
{
    uint32_t start_time = HAL_GetTick();

    while((HAL_GetTick() - start_time) < timeout_ms)
    {
        if(rx_complete)
        {
            sprintf(debug_msg, "Received: %s", rx_buffer);
            debug_print(debug_msg);

            if(strstr(rx_buffer, expected_response) != NULL)
            {
                clear_rx_buffer();
                return 1; // Success
            }
            clear_rx_buffer();
        }
        HAL_Delay(10);
    }

    debug_print("Timeout waiting for response\r\n");
    return 0; // Timeout
}

void sim800l_init(void)
{
    debug_print("=== SIM800L Initialization ===\r\n");
    HAL_Delay(2000); // Wait for SIM800L to boot up

    // Test AT communication
    debug_print("Testing AT communication...\r\n");
    send_at_command("AT");
    if(!wait_for_response("OK", 5000))
    {
        debug_print("ERROR: No response from SIM800L\r\n");
        return;
    }

    // Disable echo
    debug_print("Disabling echo...\r\n");
    send_at_command("ATE0");
    wait_for_response("OK", 3000);

    // Check SIM card
    debug_print("Checking SIM card...\r\n");
    send_at_command("AT+CPIN?");
    if(!wait_for_response("READY", 10000))
    {
        debug_print("ERROR: SIM card not ready\r\n");
        return;
    }

    // Check network registration
    debug_print("Checking network registration...\r\n");
    send_at_command("AT+CREG?");
    wait_for_response("OK", 5000);

    // Wait for network registration
    debug_print("Waiting for network registration...\r\n");
    uint8_t network_ready = 0;
    for(int i = 0; i < 30; i++) // Wait up to 30 seconds
    {
        send_at_command("AT+CREG?");
        if(wait_for_response("+CREG: 0,1", 3000) || wait_for_response("+CREG: 0,5", 3000))
        {
            network_ready = 1;
            break;
        }
        HAL_Delay(1000);
    }

    if(!network_ready)
    {
        debug_print("ERROR: Network registration failed\r\n");
        return;
    }

    // Check signal quality
    debug_print("Checking signal quality...\r\n");
    send_at_command("AT+CSQ");
    wait_for_response("OK", 3000);

    debug_print("SIM800L initialization complete!\r\n");
}

void make_phone_call(char* number)
{
    char call_command[64];

    sprintf(debug_msg, "=== Making call to %s ===\r\n", number);
    debug_print(debug_msg);

    // Prepare call command
    sprintf(call_command, "ATD%s;", number);

    // Make the call
    send_at_command(call_command);

    if(wait_for_response("OK", 10000))
    {
        debug_print("Call initiated successfully!\r\n");
        debug_print("Call in progress... Press reset to end call\r\n");

        // Keep the call active and monitor status
        while(1)
        {
            HAL_Delay(5000);
            send_at_command("AT+CPAS"); // Check phone activity status
            wait_for_response("OK", 3000);

            // You can add call status monitoring here
            // For example, check for call end with AT+CLCC
        }
    }
    else
    {
        debug_print("ERROR: Failed to initiate call\r\n");
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
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  // Start UART1 interrupt for receiving data from SIM800L
  HAL_UART_Receive_IT(&huart1, &rx_data, 1);

  debug_print("STM32F401 + SIM800L Phone Call Example\r\n");
  debug_print("======================================\r\n");
  debug_print("UART1 (SIM800L): 9600 baud\r\n");
  debug_print("UART2 (Debug): 115200 baud\r\n");
  debug_print("======================================\r\n");

  // Initialize SIM800L
  sim800l_init();

  // Wait a bit before making the call
  HAL_Delay(3000);

  // Make the phone call
  make_phone_call(PHONE_NUMBER);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    HAL_Delay(1000);
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
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */
  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */
  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;          // SIM800L at 9600 baud
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  /* USER CODE END USART1_Init 2 */

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
  huart2.Init.BaudRate = 115200;        // FTDI Debug at 115200 baud
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
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
