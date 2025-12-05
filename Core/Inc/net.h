#ifndef INC_NET_H_
#define INC_NET_H_


#include <stdio.h>
#include <stdint.h>

#include "dhcp.h"
#include "wizchip_conf.h"
#include "stm32f4xx_hal.h"



void Callback_IPConflict(void);
void Net_Init(void);


extern uint8_t mac_addr[6];
extern uint8_t dhcp_buffer[1024];



#endif /* INC_NET_H_ */
