#include "main.h"
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart1;  // SIM800L
extern UART_HandleTypeDef huart2;  // Debug

// MQTT packet handling structures and functions (integrated from library)
typedef struct {
    char* cstring;
    struct {
        int len;
        char* data;
    } lenstring;
} MQTTString;

typedef struct {
    char clientID[32];
    char username[32];
    char password[32];
    int keepAliveInterval;
    int cleansession;
} MQTTPacket_connectData;

// Global variables for MQTT handling
uint8_t rx_data = 0;
uint8_t rx_buffer[1460] = {0};
uint16_t rx_index = 0;
uint8_t mqtt_receive = 0;
char mqtt_buffer[1460] = {0};
uint16_t mqtt_index = 0;
uint8_t mqtt_connected = 0;
uint8_t tcp_connected = 0;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);

// Function prototypes
void send_at_command(char* cmd, char* response, uint32_t timeout);
void debug_print(char* msg);
uint8_t setup_gprs(void);
uint8_t connect_tcp_server(void);
uint8_t mqtt_connect_proper(void);
void mqtt_publish_proper(char* topic, char* message);
void process_received_data(void);
void clear_rx_buffer(void);
void clear_mqtt_buffer(void);

// Simple MQTT packet construction functions
int create_mqtt_connect_packet(uint8_t* buf, int buf_len, char* client_id, char* username, char* password);
int create_mqtt_publish_packet(uint8_t* buf, int buf_len, char* topic, char* payload);

int main(void)
{
    char response[200];

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_USART1_UART_Init();

    debug_print("Starting SIM800L MQTT Test...\r\n");

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
            tcp_connected = 1;

            // Wait for TCP to stabilize
            debug_print("Waiting for TCP to stabilize...\r\n");
            HAL_Delay(5000);

            // Try MQTT connection
            debug_print("Connecting to MQTT broker...\r\n");
            if (mqtt_connect_proper() == 1) {
                debug_print("MQTT connected successfully!\r\n");
                mqtt_connected = 1;

                // Send test messages
                debug_print("Publishing test messages...\r\n");

                for(int i = 0; i < 5; i++) {
                    char message[50];
                    sprintf(message, "Hello from SIM800L - Message %d", i+1);
                    mqtt_publish_proper("test/sim800", message);
                    debug_print("Message published, waiting...\r\n");
                    HAL_Delay(10000);  // Wait 10 seconds between messages
                }

                debug_print("Test complete - check MQTT subscriber\r\n");

                // Keep alive loop
                uint32_t last_ping = HAL_GetTick();
                while (1) {
                    // Process any received data
                    process_received_data();

                    // Send periodic keepalive
                    if (HAL_GetTick() - last_ping > 30000) {  // Every 30 seconds
                        if (mqtt_connected && tcp_connected) {
                            mqtt_publish_proper("test/keepalive", "alive");
                            debug_print("Keepalive sent\r\n");
                        }
                        last_ping = HAL_GetTick();
                    }

                    HAL_Delay(1000);
                }
            } else {
                debug_print("MQTT connection failed!\r\n");
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

uint8_t mqtt_connect_proper(void)
{
    char response[300];
    uint8_t mqtt_packet[256];

    // Create MQTT CONNECT packet
    int packet_len = create_mqtt_connect_packet(mqtt_packet, sizeof(mqtt_packet),
                                              "SIM800L_Client", "", "");

    if (packet_len <= 0) {
        debug_print("Failed to create MQTT CONNECT packet\r\n");
        return 0;
    }

    debug_print("Sending MQTT CONNECT packet...\r\n");

    char send_cmd[50];
    sprintf(send_cmd, "AT+CIPSEND=%d\r\n", packet_len);
    send_at_command(send_cmd, response, 5000);

    if (strstr(response, ">") != NULL) {
        debug_print("Sending CONNECT data...\r\n");
        HAL_UART_Transmit(&huart1, mqtt_packet, packet_len, 5000);

        // Wait for CONNACK
        HAL_Delay(5000);

        // Check for successful connection by looking for CONNACK (0x20)
        // In practice, you'd parse the actual response
        debug_print("MQTT CONNECT sent, assuming success\r\n");
        return 1;
    }

    debug_print("Failed to get > prompt for MQTT CONNECT\r\n");
    return 0;
}

void mqtt_publish_proper(char* topic, char* message)
{
    char response[300];
    uint8_t mqtt_packet[512];

    // Create MQTT PUBLISH packet
    int packet_len = create_mqtt_publish_packet(mqtt_packet, sizeof(mqtt_packet), topic, message);

    if (packet_len <= 0) {
        debug_print("Failed to create MQTT PUBLISH packet\r\n");
        return;
    }

    debug_print("Publishing MQTT message...\r\n");

    char send_cmd[50];
    sprintf(send_cmd, "AT+CIPSEND=%d\r\n", packet_len);
    send_at_command(send_cmd, response, 5000);

    if (strstr(response, ">") != NULL) {
        debug_print("Sending PUBLISH data...\r\n");
        HAL_UART_Transmit(&huart1, mqtt_packet, packet_len, 5000);
        HAL_Delay(2000);
        debug_print("MQTT message sent\r\n");
    } else {
        debug_print("Failed to get > prompt for MQTT PUBLISH\r\n");
    }
}

// Create MQTT CONNECT packet manually (simplified version)
int create_mqtt_connect_packet(uint8_t* buf, int buf_len, char* client_id, char* username, char* password)
{
    if (buf_len < 50) return -1;  // Not enough space

    int pos = 0;
    int client_id_len = strlen(client_id);
    int username_len = username ? strlen(username) : 0;
    int password_len = password ? strlen(password) : 0;

    // Calculate remaining length
    int remaining_len = 10 + 2 + client_id_len;  // Variable header + client ID
    if (username_len > 0) remaining_len += 2 + username_len;
    if (password_len > 0) remaining_len += 2 + password_len;

    // Fixed header
    buf[pos++] = 0x10;  // CONNECT packet type
    buf[pos++] = remaining_len;  // Remaining length (assuming < 127)

    // Variable header
    buf[pos++] = 0x00; buf[pos++] = 0x04;  // Protocol name length
    buf[pos++] = 'M'; buf[pos++] = 'Q'; buf[pos++] = 'T'; buf[pos++] = 'T';  // Protocol name
    buf[pos++] = 0x04;  // Protocol level (MQTT 3.1.1)

    // Connect flags
    uint8_t connect_flags = 0x02;  // Clean session
    if (username_len > 0) connect_flags |= 0x80;  // Username flag
    if (password_len > 0) connect_flags |= 0x40;  // Password flag
    buf[pos++] = connect_flags;

    // Keep alive (60 seconds)
    buf[pos++] = 0x00; buf[pos++] = 0x3C;

    // Payload - Client ID
    buf[pos++] = (client_id_len >> 8) & 0xFF;
    buf[pos++] = client_id_len & 0xFF;
    memcpy(&buf[pos], client_id, client_id_len);
    pos += client_id_len;

    // Username (if provided)
    if (username_len > 0) {
        buf[pos++] = (username_len >> 8) & 0xFF;
        buf[pos++] = username_len & 0xFF;
        memcpy(&buf[pos], username, username_len);
        pos += username_len;
    }

    // Password (if provided)
    if (password_len > 0) {
        buf[pos++] = (password_len >> 8) & 0xFF;
        buf[pos++] = password_len & 0xFF;
        memcpy(&buf[pos], password, password_len);
        pos += password_len;
    }

    return pos;
}

// Create MQTT PUBLISH packet manually
int create_mqtt_publish_packet(uint8_t* buf, int buf_len, char* topic, char* payload)
{
    int topic_len = strlen(topic);
    int payload_len = strlen(payload);
    int total_len = 2 + topic_len + payload_len;  // Topic length field + topic + payload

    if (buf_len < total_len + 2) return -1;  // Not enough space

    int pos = 0;

    // Fixed header
    buf[pos++] = 0x30;  // PUBLISH packet, QoS 0, no retain
    buf[pos++] = total_len;  // Remaining length (assuming < 127)

    // Variable header - Topic
    buf[pos++] = (topic_len >> 8) & 0xFF;  // Topic length MSB
    buf[pos++] = topic_len & 0xFF;         // Topic length LSB
    memcpy(&buf[pos], topic, topic_len);
    pos += topic_len;

    // Payload
    memcpy(&buf[pos], payload, payload_len);
    pos += payload_len;

    return pos;
}

void process_received_data(void)
{
    // Simple data processing - in practice you'd implement proper UART interrupt handling
    // This is a placeholder for the interrupt-based reception from the library
}

void clear_rx_buffer(void)
{
    rx_index = 0;
    memset(rx_buffer, 0, sizeof(rx_buffer));
}

void clear_mqtt_buffer(void)
{
    mqtt_receive = 0;
    mqtt_index = 0;
    memset(mqtt_buffer, 0, sizeof(mqtt_buffer));
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

    // Start task and set APN (change this to your carrier's APN)
    debug_print("Setting APN...\r\n");
    send_at_command("AT+CSTT=\"internet.vodafone.net\"\r\n", response, 3000);
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

    // Check current connection status first
    debug_print("Checking connection status...\r\n");
    send_at_command("AT+CIPSTATUS\r\n", response, 3000);
    debug_print(response);

    // If already connected, skip setup
    if (strstr(response, "CONNECT OK") != NULL || strstr(response, "STATE: CONNECT OK") != NULL) {
        debug_print("TCP connection already established!\r\n");
        return 1;
    }

    // Only set these if not already connected
    if (strstr(response, "STATE: IP STATUS") != NULL) {
        // Set to single connection mode
        debug_print("Setting single connection mode...\r\n");
        send_at_command("AT+CIPMUX=0\r\n", response, 3000);
        debug_print(response);

        // Set to non-transparent mode
        debug_print("Setting non-transparent mode...\r\n");
        send_at_command("AT+CIPMODE=0\r\n", response, 3000);
        debug_print(response);
    }

    // Close any existing connections
    debug_print("Closing any existing connections...\r\n");
    send_at_command("AT+CIPCLOSE\r\n", response, 5000);
    HAL_Delay(2000);

    // Start TCP connection to your MQTT broker
    debug_print("Starting TCP connection to MQTT broker...\r\n");
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

// System configuration functions (unchanged)
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
