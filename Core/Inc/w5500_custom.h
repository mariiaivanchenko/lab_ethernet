#ifndef INC_W5500_CUSTOM_H_
#define INC_W5500_CUSTOM_H_

#include "main.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "dhcp.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>


void W5500_Init(void);

void W5500_Select(void);
void W5500_Unselect(void);

uint8_t W5500_ReadByte(void);
void W5500_WriteByte(uint8_t wb);


#endif /* INC_W5500_CUSTOM_H_ */
