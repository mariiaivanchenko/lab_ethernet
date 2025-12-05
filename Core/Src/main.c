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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <w5500_custom.h>
#include "dhcp.h"
#include "net.h"
#include "socket.h"
#include "ILI9341.h"
#include "llmnr.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DMA_RX_BUFFER_SIZE 256
#define GPS_PROCESS_BUFFER_SIZE 256
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart2_rx;

/* USER CODE BEGIN PV */
volatile uint32_t dhcp_tick_cnt = 0;
uint8_t chip_ver_g;
Screen ILI9341_Screen;

uint8_t is_websocket_active = 0;

uint8_t g_dma_rx_buffer[DMA_RX_BUFFER_SIZE];
uint8_t g_gps_process_buffer[GPS_PROCESS_BUFFER_SIZE];
volatile uint8_t g_gps_data_ready = 0;
volatile uint16_t g_gps_data_size = 0;

int32_t lat_e5 = 0; // Широта * 100000
int32_t lon_e5 = 0; // Довгота * 100000
uint8_t sats_view = 0;
uint8_t fix_status = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void Timer_Callback_1s(void) {
    DHCP_time_handler();
}


// “GPS”

uint32_t tick_tlm  = 0;
uint32_t tick_hb   = 0;
uint32_t tick_cmd  = 0;

static uint8_t cmd_seq = 0;


uint8_t crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00;
    const uint8_t poly = 0x07; // x^8 + x^2 + x + 1

    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ poly;
            else
                crc <<= 1;
        }
    }
    return crc;
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
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  MX_SPI2_Init();


  /* USER CODE BEGIN 2 */
  if(HAL_UARTEx_ReceiveToIdle_DMA(&huart2, g_dma_rx_buffer, DMA_RX_BUFFER_SIZE) != HAL_OK) {
	  Error_Handler();
  }

  ILI9341_init(&hspi2);

  Screen_Tiles_init(&ILI9341_Screen);

  ILI9341_fill_screen(&hspi2, &ILI9341_Screen, 0x0000); // 0x0000
  HAL_Delay(3000);

  printf("Resetting W5500...\r\n");

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(100);

  W5500_Init();
  Net_Init();

  // --- Start HTTP Server on Socket 1 ---
  uint8_t sock = 1;

    if(socket(sock, Sn_MR_TCP, 80, 0) == sock) {
        listen(sock);
        printf("Server started on port 80\r\n");
    } else {
        printf("Failed to open socket\r\n");
    }

    // --- Start LLMNR on Socket 2 ---
      uint8_t sock_llmnr = 2; // <--- 2. Виділяємо окремий сокет для LLMNR (UDP)
      LLMNR_Init(sock_llmnr); // <--- 3. Ініціалізуємо LLMNR
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if (g_gps_data_ready) {
	  	          char* line = strtok((char*)g_gps_process_buffer, "\r\n");

	  	          while (line != NULL)
	  	          {
	  	              // Шукаємо рядки GNGGA або GPGGA
	  	              if (strncmp(line, "$GNGGA", 6) == 0 || strncmp(line, "$GPGGA", 6) == 0) {
	  	                  float nmea_time, nmea_lat, nmea_lon, altitude;
	  	                  char lat_dir, lon_dir;
	  	                  int fix_quality, num_sats;

	  	                  // Знаходимо початок даних (після коми)
	  	                  const char* data_start = strchr(line, ',');
	  	                  if (data_start) {
	  	                      // Парсимо рядок
	  	                      int items = sscanf(data_start + 1, "%f,%f,%c,%f,%c,%d,%d,%*f,%f,",
	  	                                         &nmea_time, &nmea_lat, &lat_dir, &nmea_lon, &lon_dir,
	  	                                         &fix_quality, &num_sats, &altitude);

	  	                      if (items >= 7 && fix_quality > 0) {
	  	                          // 1. Конвертуємо координати з формату NMEA (ddmm.mmmm) в десяткові градуси (dd.ddddd)
	  	                          int lat_deg_int = (int)(nmea_lat / 100);
	  	                          float lat_min = nmea_lat - (lat_deg_int * 100);
	  	                          float lat_decimal = lat_deg_int + (lat_min / 60.0f);
	  	                          if (lat_dir == 'S') lat_decimal = -lat_decimal;

	  	                          int lon_deg_int = (int)(nmea_lon / 100);
	  	                          float lon_min = nmea_lon - (lon_deg_int * 100);
	  	                          float lon_decimal = lon_deg_int + (lon_min / 60.0f);
	  	                          if (lon_dir == 'W') lon_decimal = -lon_decimal;

	  	                          // 2. Оновлюємо глобальні змінні для CAN (формат: * 100000)
	  	                          lat_e5 = (int32_t)(lat_decimal * 100000.0f);
	  	                          lon_e5 = (int32_t)(lon_decimal * 100000.0f);
	  	                          sats_view = (uint8_t)num_sats;
	  	                          fix_status = (uint8_t)fix_quality; // 1 = GPS fix, 2 = DGPS fix
	  	                      } else {
	  	                           // Якщо фіксу немає, можна слати нулі або статус помилки
	  	                           fix_status = 0;
	  	                      }
	  	                  }
	  	              }
	  	              line = strtok(NULL, "\r\n"); // Наступний рядок
	  	          }
	  	          g_gps_data_ready = 0;
	  	      }
	  	  uint32_t now = HAL_GetTick();


	  wiz_NetInfo cur_net_info;

	  wizchip_getnetinfo(&cur_net_info);

	  if (cur_net_info.dhcp == NETINFO_DHCP) {
	       static uint32_t sec_timer = 0;
	       if (HAL_GetTick() - sec_timer >= 1000) {
	          sec_timer = HAL_GetTick();
	          DHCP_time_handler();
	       }
	       DHCP_run();
	  }



	  // ========== HTTP SERVER LOOP ==========
	  uint8_t status = getSn_SR(sock);

	  static uint8_t buffer[1024];

	  if (status == SOCK_CLOSED) {
		  if(socket(sock, Sn_MR_TCP, 80, 0) == sock) {
			  listen(sock);
			  is_websocket_active = 0; // Reset flag
			  printf("Socket Re-opened\r\n");
		  }
	  }

	  if (status == SOCK_ESTABLISHED) {
		if (getSn_RX_RSR(sock) > 0) {
		  int32_t recv_len = recv(sock, buffer, sizeof(buffer));

		  if (recv_len > 0) {
			  if (!is_websocket_active && strstr((char*)buffer, "Upgrade: websocket")) {
					  printf("WebSocket Upgrade Request Received\r\n");
					  if (WS_PerformHandshake(sock, (char*)buffer)) { //
						  is_websocket_active = 1;
						  printf("WebSocket Handshake Success!\r\n");
						  // IMPORTANT: DO NOT DISCONNECT HERE
				  }
			  }

			  // --- GET /status ---
			  else if (strstr((char*)buffer, "GET /status") != NULL) {

				  char json[256];
				  sprintf(json,
					  "{ \"proto_ver\":1, \"device_id\":\"STM32-411\", "
					  "\"gps\":{\"lat\":49.0,\"lon\":24.0}, "
					  "\"env\":{\"t_c\":23.0,\"lux\":400} }"
				  );

				  char header[128];
				  sprintf(header,
					  "HTTP/1.1 200 OK\r\n"
					  "Content-Type: application/json\r\n"
					  "Content-Length: %d\r\n\r\n",
					  strlen(json)
				  );

				  send(sock, (uint8_t *)header, strlen(header));
				  send(sock, (uint8_t *)json, strlen(json));
			  }

			  // --- GET / ---
			  else if (!is_websocket_active && strstr((char*)buffer, "GET / ")) {
			                  // Updated HTML to use WebSocket Client (ws://)
			                  const char *html =
			                      "<html><head><title>STM32 WS</title></head><body>"
			                      "<h1>STM32 Monitor</h1>"
			                      "<div id='d' style='font-size:20px;font-family:monospace;'>Waiting...</div>"
			                      "<script>"
			                      "var ws = new WebSocket('ws://' + location.host);"
			                      "ws.onmessage = function(e){ document.getElementById('d').innerText = e.data; };"
			                      "ws.onclose = function(){ document.getElementById('d').innerText += ' (Closed)'; };"
			                      "</script>"
			                      "</body></html>";

			                  char header[128];
			                  sprintf(header,
			                      "HTTP/1.1 200 OK\r\n"
			                      "Content-Type: text/html\r\n"
			                      "Content-Length: %d\r\n\r\n",
			                      strlen(html)
			                  );
			                  send(sock, (uint8_t *)header, strlen(header));
			                  send(sock, (uint8_t *)html, strlen(html));

			                  // For standard HTTP, we MUST disconnect to finish the load
			                  disconnect(sock);
			  }

		  }



		}
	  }

	  if (status == SOCK_CLOSE_WAIT) {
	          disconnect(sock);
	          is_websocket_active = 0;
	          printf("Client Disconnected\r\n");
	  }
	  if (is_websocket_active) {
	              static uint32_t ws_timer = 0;
	              if (HAL_GetTick() - ws_timer >= 500) { // Send every 500ms
	                  ws_timer += 500;

	                  char json[128];
	                  // Simulate changing data
	                  sprintf(json, "{\"tick\":%lu, \"adc\":%d}", HAL_GetTick(), rand()%4096);

	                  WS_SendText(sock, json); //
	              }
	  }
	  // --- LLMNR Process ---
	  LLMNR_Process(sock_llmnr);
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
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
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

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
  huart1.Init.BaudRate = 115200;
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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, SPI2_RST_Pin|SPI2_DC_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, SPI1_CS_Pin|SPI1_RST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : SPI2_RST_Pin SPI2_DC_Pin */
  GPIO_InitStruct.Pin = SPI2_RST_Pin|SPI2_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI1_CS_Pin SPI1_RST_Pin */
  GPIO_InitStruct.Pin = SPI1_CS_Pin|SPI1_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : SPI2_CS_Pin */
  GPIO_InitStruct.Pin = SPI2_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SPI2_CS_GPIO_Port, &GPIO_InitStruct);

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
#ifdef USE_FULL_ASSERT
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
