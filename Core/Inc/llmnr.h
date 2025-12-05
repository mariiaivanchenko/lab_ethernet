#ifndef INC_LLMNR_H_
#define INC_LLMNR_H_


#include <stdint.h>
#include "main.h"

void LLMNR_Init(uint8_t sock);
void LLMNR_Process(uint8_t sock);
void LLMNR_SendResponse(uint8_t sock, uint8_t id0, uint8_t id1,uint8_t *rip, uint16_t rport);

#endif /* INC_LLMNR_H_ */
