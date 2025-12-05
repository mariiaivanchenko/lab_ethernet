#include <w5500_custom.h>



char msg[64];



void W5500_Select(void) {

    HAL_GPIO_WritePin(GPIOC, SPI1_CS_Pin, GPIO_PIN_RESET);
}


void W5500_Unselect(void) {

    HAL_GPIO_WritePin(GPIOC, SPI1_CS_Pin, GPIO_PIN_SET);
}



uint8_t W5500_ReadByte(void) {
    uint8_t rb = 0;
    uint8_t wb = 0xFF; // Dummy byte (usually 0xFF)

    // Use TransmitReceive to ensure clock generation and MOSI state
    HAL_SPI_TransmitReceive(&hspi1, &wb, &rb, 1, HAL_MAX_DELAY);

    return rb;
}

void W5500_WriteByte(uint8_t wb) {

    HAL_SPI_Transmit(&hspi1, &wb, 1, HAL_MAX_DELAY);
}



void W5500_Init(void) {

	reg_wizchip_cs_cbfunc(W5500_Select, W5500_Unselect);
	reg_wizchip_spi_cbfunc(W5500_ReadByte, W5500_WriteByte);

	uint8_t rx_tx_buff_sizes[] = {2, 2, 2, 2, 2, 2, 2, 2};

	if (wizchip_init(rx_tx_buff_sizes, rx_tx_buff_sizes) == -1) {
		  sprintf(msg, "W5500 Init FAILED! Check wiring.\r\n");
		  HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
		  while (1);
	} else {
		  sprintf(msg, "W5500 Init Success!\r\n");
		  HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
	}

	uint8_t chip_ver = getVERSIONR();

	chip_ver_g = chip_ver;
	printf("W5500 Version: 0x%02X\r\n", chip_ver);

	if (chip_ver != 0x04) { // Для W5500 це має бути 0x04
		  printf("ERROR: SPI Failure! Read: 0x%02X instead of 0x04\r\n", chip_ver);
		  printf("Check MISO, MOSI, SCK and CS connections.\r\n");
		  while(1); // Зупиняємось тут, далі йти немає сенсу
	  }

	return;

}
