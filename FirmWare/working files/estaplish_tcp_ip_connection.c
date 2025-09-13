#include "main.h"
#include <string.h>
#include <stdio.h>

UART_HandleTypeDef huart1;  // SIM800L
UART_HandleTypeDef huart2;  // Debug

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);

// Function prototypes
void send_at_command(char* cmd, char* response, uint32_t timeout);
void debug_print(char* msg);
uint8_t setup_gprs(void);
uint8_t connect_tcp_server(void);

int main(void)
{
    char response[200];

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_USART1_UART_Init();

    debug_print("Starting SIM800L TCP Connection Test...\r\n");

    // Wait for SIM800L to boot
    HAL_Delay(3000);

    // Test AT communication
    debug_print("Testing AT communication...\r\n");
    send_at_command("AT\r\n", response, 2000);
    debug_print(response);

    // Setup GPRS connection
    if (setup_gprs() == 1) {
        debug_print("GPRS setup successful!\r\n");

        // Connect to TCP server
        if (connect_tcp_server() == 1) {
            debug_print("TCP connection successful!\r\n");
            debug_print("Connection established - test complete!\r\n");

            // Keep connection alive and monitor status
            while (1) {
                HAL_Delay(10000);  // Check every 10 seconds

                // Check connection status
                send_at_command("AT+CIPSTATUS\r\n", response, 3000);
                debug_print("Connection status: ");
                debug_print(response);
                debug_print("\r\n");
            }
        } else {
            debug_print("TCP connection failed!\r\n");
        }
    } else {
        debug_print("GPRS setup failed!\r\n");
    }

    while (1) {
        HAL_Delay(1000);
    }
}

void send_at_command(char* cmd, char* response, uint32_t timeout)
{
    memset(response, 0, 200);

    // Send command
    HAL_UART_Transmit(&huart1, (uint8_t*)cmd, strlen(cmd), 1000);

    // Receive response
    HAL_UART_Receive(&huart1, (uint8_t*)response, 199, timeout);
}

void debug_print(char* msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);
}

uint8_t setup_gprs(void)
{
    char response[200];

    // Check SIM status
    debug_print("Checking SIM status...\r\n");
    send_at_command("AT+CPIN?\r\n", response, 3000);
    debug_print(response);
    if (strstr(response, "READY") == NULL) {
        debug_print("SIM not ready\r\n");
        return 0;
    }

    // Check signal quality
    debug_print("Checking signal quality...\r\n");
    send_at_command("AT+CSQ\r\n", response, 3000);
    debug_print(response);

    // Check network registration
    debug_print("Checking network registration...\r\n");
    for (int i = 0; i < 10; i++) {
        send_at_command("AT+CREG?\r\n", response, 3000);
        debug_print(response);
        if (strstr(response, ",1") != NULL || strstr(response, ",5") != NULL) {
            debug_print("Network registered\r\n");
            break;
        }
        HAL_Delay(2000);
        if (i == 9) {
            debug_print("Network registration failed\r\n");
            return 0;
        }
    }

    // Check GPRS attachment
    debug_print("Checking GPRS attachment...\r\n");
    send_at_command("AT+CGATT?\r\n", response, 3000);
    debug_print(response);
    if (strstr(response, "1") == NULL) {
        debug_print("Not attached to GPRS\r\n");
        return 0;
    }

    // Start task and set APN
    debug_print("Setting APN...\r\n");
    send_at_command("AT+CSTT=\"internet\"\r\n", response, 3000);
    debug_print(response);

    // Bring up wireless connection
    debug_print("Bringing up wireless connection...\r\n");
    send_at_command("AT+CIICR\r\n", response, 15000);
    debug_print(response);
    if (strstr(response, "OK") == NULL) {
        debug_print("Failed to bring up connection\r\n");
        return 0;
    }

    // Get local IP address
    debug_print("Getting IP address...\r\n");
    send_at_command("AT+CIFSR\r\n", response, 5000);
    debug_print("IP Address: ");
    debug_print(response);
    debug_print("\r\n");

    // Verify we got a valid IP (not ERROR)
    if (strstr(response, "ERROR") != NULL) {
        debug_print("No valid IP address obtained\r\n");
        return 0;
    }

    return 1;
}

uint8_t connect_tcp_server(void)
{
    char response[200];

    // Set to single connection mode
    debug_print("Setting single connection mode...\r\n");
    send_at_command("AT+CIPMUX=0\r\n", response, 3000);
    debug_print(response);

    // Set to non-transparent mode
    debug_print("Setting non-transparent mode...\r\n");
    send_at_command("AT+CIPMODE=0\r\n", response, 3000);
    debug_print(response);

    // Check current connection status
    debug_print("Checking connection status...\r\n");
    send_at_command("AT+CIPSTATUS\r\n", response, 3000);
    debug_print(response);

    // Close any existing connections
    debug_print("Closing any existing connections...\r\n");
    send_at_command("AT+CIPCLOSE\r\n", response, 5000);
    HAL_Delay(2000);

    // Start TCP connection to your server
    debug_print("Starting TCP connection to 81.10.88.24:1883...\r\n");
    send_at_command("AT+CIPSTART=\"TCP\",\"81.10.88.24\",1883\r\n", response, 15000);
    debug_print("TCP Response: ");
    debug_print(response);
    debug_print("\r\n");

    // Check for successful connection
    if (strstr(response, "CONNECT OK") != NULL) {
        debug_print("TCP connection established successfully!\r\n");
        return 1;
    } else if (strstr(response, "ALREADY CONNECT") != NULL) {
        debug_print("TCP connection already established!\r\n");
        return 1;
    } else {
        debug_print("TCP connection failed\r\n");
        return 0;
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

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

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 9600;
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
}

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

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
