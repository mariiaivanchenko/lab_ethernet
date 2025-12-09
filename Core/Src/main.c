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
#include "usb_device.h"

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

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
volatile uint32_t dhcp_tick_cnt = 0;
uint8_t chip_ver_g;
Screen ILI9341_Screen;

uint8_t is_websocket_active = 0;
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
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */

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

  status_init();

    // Initialize USB CDC device
  MX_USB_DEVICE_Init();

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
      uint32_t last_status_ms = HAL_GetTick();
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  uint32_t now = HAL_GetTick();

	  if ((now - last_status_ms) >= 1000)
		{
			last_status_ms = now;
			status_update();
		}

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
				  build_status_json(json, sizeof(json));

				  char header[128];
				  sprintf(header,
					  "HTTP/1.1 200 OK\r\n"
					  "Content-Type: application/json\r\n"
					  "Content-Length: %d\r\n\r\n",
					  (int)strlen(json)
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

  /* Увімкнути живлення і виставити шкалу напруги */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /* Налаштовуємо осцилятори: HSE + PLL */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

  /* HSE=8MHz:
   * PLLM = 8   → VCO_IN = 1 MHz
   * PLLN = 336 → VCO_OUT = 336 MHz
   * PLLP = 4   → SYSCLK = 84 MHz
   * PLLQ = 7   → 336/7 = 48 MHz для USB
   */
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* Шини */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 |
                                RCC_CLOCKTYPE_PCLK2;

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;  // 84 MHz
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;         // HCLK = 84 MHz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;          // PCLK1 = 42 MHz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;          // PCLK2 = 84 MHz

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
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
