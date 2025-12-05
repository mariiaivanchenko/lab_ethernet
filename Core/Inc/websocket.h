#ifndef WEBSOCKET_H_
#define WEBSOCKET_H_

#include <stdint.h>
#include "socket.h" // W5500 socket library

// Магічний рядок з RFC6455 (з вашого файлу WebSocketProtocol.hpp)
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// Функція для перевірки handshake і відправки відповіді
uint8_t WS_PerformHandshake(uint8_t sock, char* buffer);

// Функція відправки текстового повідомлення
void WS_SendText(uint8_t sock, char* msg);

#endif /* WEBSOCKET_H_ */
