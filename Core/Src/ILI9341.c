/*
 * ILI9341.c
 *
 *  Created on: Oct 2, 2025
 *      Author: mariia
 */

#include "ILI9341.h"

float tCSS = 0.001;
float tCSH = 0.001;

uint32_t ILI9341_WIDTH = 240;
uint32_t ILI9341_HEIGHT = 320;

void Screen_Tiles_init(Screen *screen) {
	uint16_t start_x = 0;
	uint16_t start_y = 0;
	int idx = 0;
	for (int row = 0; row < 4; row++) {
		start_x = 0;
		for (int col = 0; col < 4; col++) {
			screen->tiles[idx].point_x = start_x;
			screen->tiles[idx].point_y = start_y;
			start_x += 60;
			idx++;
		}
		start_y += 80;
	}
}

void Screen_fill_tile(SPI_HandleTypeDef *hspi, Tile *tile, uint16_t color) {
	ILI9341_set_window(hspi, tile->point_x, tile->point_y, tile->point_x + 60 - 1, tile->point_y + 80 - 1);

	uint32_t total_pixels = 60 * 80;
	const uint32_t pixels_in_chunk = 128;
	uint8_t chunk[pixels_in_chunk * 2];
	uint8_t hi = color >> 8;
	uint8_t lo = color & 0xFF;

	for (uint32_t i = 0; i < pixels_in_chunk; ++i) {
		chunk[2*i] = hi;
		chunk[2*i + 1] = lo;
	}

	HAL_GPIO_WritePin(SPI2_DC_GPIO_Port, SPI2_DC_Pin, GPIO_PIN_SET); // High -> data
	HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_RESET); // CS -> Low
	HAL_Delay(tCSS);

	uint32_t sent_pixels = 0;
	while (sent_pixels < total_pixels) {
		uint32_t remain = total_pixels - sent_pixels;
		uint32_t to_send = (remain >= pixels_in_chunk) ? pixels_in_chunk : remain;
		HAL_SPI_Transmit(hspi, chunk, to_send * 2, HAL_MAX_DELAY);
//		while(HAL_SPI_GetState(hspi) != HAL_SPI_STATE_READY);
		sent_pixels += to_send;
	}

	HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_SET); // CS -> High
	HAL_Delay(tCSH);
}


void ILI9341_send_command(SPI_HandleTypeDef *hspi, uint8_t cmd) {
    HAL_GPIO_WritePin(SPI2_DC_GPIO_Port, SPI2_DC_Pin, GPIO_PIN_RESET); // Low -> command
    HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_RESET); // CS -> Low
    HAL_Delay(tCSS); // 1000ns -> 0.001 ms
    HAL_SPI_Transmit(hspi, &cmd, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_SET); // CS -> High
    HAL_Delay(tCSH);
}

void ILI9341_send_data(SPI_HandleTypeDef *hspi, uint8_t *buff, size_t buff_size) {
    HAL_GPIO_WritePin(SPI2_DC_GPIO_Port, SPI2_DC_Pin, GPIO_PIN_SET); // High -> data
    HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_RESET); // CS -> Low
    HAL_Delay(tCSS);
    HAL_SPI_Transmit(hspi, buff, buff_size, HAL_MAX_DELAY);
//	while(HAL_SPI_GetState(hspi) != HAL_SPI_STATE_READY);
    HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_SET); // CS -> High
    HAL_Delay(tCSH);
}

void ILI9341_reset(void) {
    HAL_GPIO_WritePin(SPI2_RST_GPIO_Port, SPI2_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(SPI2_RST_GPIO_Port, SPI2_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(20);
}


void ILI9341_init(SPI_HandleTypeDef *hspi) {
    ILI9341_reset();

    // 1. Software Reset
    ILI9341_send_command(hspi, 0x01); // Software Reset
    HAL_Delay(5);

    // 2. Power Control A
    ILI9341_send_command(hspi, 0xCB);
    uint8_t data_cba[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    ILI9341_send_data(hspi, data_cba, 5);

    // 3. Power Control B
    ILI9341_send_command(hspi, 0xCF);
    uint8_t data_cf[] = {0x00, 0xC1, 0x30};
    ILI9341_send_data(hspi, data_cf, 3);

    // 4. Driver timing control A
    ILI9341_send_command(hspi, 0xE8);
    uint8_t data_e8[] = {0x85, 0x00, 0x78};
    ILI9341_send_data(hspi, data_e8, 3);

    // 5. Driver timing control B
    ILI9341_send_command(hspi, 0xEA);
    uint8_t data_ea[] = {0x00, 0x00};
    ILI9341_send_data(hspi, data_ea, 2);

    // 6. Power on sequence control
    ILI9341_send_command(hspi, 0xED);
    uint8_t data_ed[] = {0x64, 0x03, 0x12, 0x81};
    ILI9341_send_data(hspi, data_ed, 4);

    // 7. Pump ratio control
    ILI9341_send_command(hspi, 0xF7);
    uint8_t data_f7[] = {0x20};
    ILI9341_send_data(hspi, data_f7, 1);

    // 8. Power Control 1 (Vcore)
    ILI9341_send_command(hspi, 0xC0);
    uint8_t data_c0[] = {0x23};
    ILI9341_send_data(hspi, data_c0, 1);

    // 9. Power Control 2 (VGH, VGL)
    ILI9341_send_command(hspi, 0xC1);
    uint8_t data_c1[] = {0x10};
    ILI9341_send_data(hspi, data_c1, 1);

    // 10. VCOM Control 1
    ILI9341_send_command(hspi, 0xC5);
    uint8_t data_c5[] = {0x3E, 0x28};
    ILI9341_send_data(hspi, data_c5, 2);

    // 11. VCOM Control 2
    ILI9341_send_command(hspi, 0xC7);
    uint8_t data_c7[] = {0x86};
    ILI9341_send_data(hspi, data_c7, 1);

    // 12. Memory Access Control (Orientation)
    ILI9341_send_command(hspi, 0x36);
    // Setting 0x48 for Portrait Mode (MX=0, MY=1, MV=0, ML=0, BGR=1)
    uint8_t data_36[] = {0x48};
    ILI9341_send_data(hspi, data_36, 1);

    // 13. Setting Pixel Format
    ILI9341_send_command(hspi, 0x3A);
    uint8_t data_3a[] = {0x55}; // 16-bit per pixel (65K colors)
    ILI9341_send_data(hspi, data_3a, 1);

    // 14. Frame Rate Control
    ILI9341_send_command(hspi, 0xB1);
    uint8_t data_b1[] = {0x00, 0x18};
    ILI9341_send_data(hspi, data_b1, 2);

    // 15. Display Function Control
    ILI9341_send_command(hspi, 0xB6);
    uint8_t data_b6[] = {0x08, 0xA2, 0x27, 0x00};
    ILI9341_send_data(hspi, data_b6, 4);

    // 16. Enable 3G
    ILI9341_send_command(hspi, 0xF2);
    uint8_t data_f2[] = {0x00};
    ILI9341_send_data(hspi, data_f2, 1);

    // 17. Gamma Function Disable
    ILI9341_send_command(hspi, 0x26);
    uint8_t data_26[] = {0x01};
    ILI9341_send_data(hspi, data_26, 1);

    // 18. Wake up the display
    ILI9341_send_command(hspi, 0x11); // Exit Sleep
    HAL_Delay(120);

    // 19. Display ON
    ILI9341_send_command(hspi, 0x29); // Display ON
    HAL_Delay(20);

    // Optional: Gamma Curve Configuration commands (not strictly necessary for a basic test)
}

void ILI9341_set_window(SPI_HandleTypeDef *hspi, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];

    // setting column address
    ILI9341_send_command(hspi, 0x2A);
    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;
    ILI9341_send_data(hspi, data, 4);

    // setting page address
    ILI9341_send_command(hspi, 0x2B);
    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;
    ILI9341_send_data(hspi, data, 4);

    // Memory write
    ILI9341_send_command(hspi, 0x2C);
}

void ILI9341_fill_screen(SPI_HandleTypeDef *hspi, Screen *screen, uint16_t color) {
	 for (int i = 0; i < 16; i++) {
		 Screen_fill_tile(hspi, &screen->tiles[i], color);
	 }
	 HAL_Delay(1000);
}

uint8_t get_char_index(char c) {
    switch(c) {
        case 'A': return 0;
        case 'L': return 1;
        case 'I': return 2;
        case 'D': return 3;
        case ':': return 4;
        case 'T': return 5;
        case 'O': return 6;
        case 'N': return 7;
        case 'U': return 8;
        case 'C': return 9;
        case ' ': return 10;
        case '0': return 11;
        case '1': return 12;
        case '2': return 13;
        case '3': return 14;
        case '4': return 15;
        case '5': return 16;
        case '6': return 17;
        case '7': return 18;
        case '8': return 19;
        case '9': return 20;
        case '.': return 21;
        case '-': return 22;
        case 'x': return 23;
        case 'M': return 24;
        case 'P': return 25;
        case 'G': return 26;
        case 'W': return 27;
        case 'S': return 28;
        case 'K': return 29;
        default:  return 0;
    }
}



void ILI9341_draw_char(SPI_HandleTypeDef *hspi, char c, uint16_t x, uint16_t y, uint16_t color) {

	const uint8_t *bitmap = font6x8[get_char_index(c)];
	const uint8_t width = 6;
	const uint8_t height = 8;

	uint16_t buf[width * height];

	    for (uint8_t col = 0; col < height; col++) {
	        for (uint8_t row = 0; row < width; row++) {
	            if (bitmap[col] & (1 << row)) {
	                buf[col * width + row] = color; // pixel ON
	            } else {
	                buf[col * width + row] = 0x0000; // pixel OFF (black)
	            }
	        }
	    }

	ILI9341_set_window(hspi, x, y, x + width - 1, y + height - 1);
	ILI9341_send_data(hspi, buf, width * height * 2);
}

void ILI9341_draw_text(SPI_HandleTypeDef *hspi, Screen *screen, uint16_t x, uint16_t y, const char *str, uint16_t color) {
    uint16_t cursor_x = x;
    while (*str) {
    	ILI9341_draw_char(hspi, *str, cursor_x, y, color);
    	cursor_x -= 7;
        str++;
    }
}


