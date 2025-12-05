#include "websocket.h"
#include "websocket_utils.h" // <--- Тепер підключаємо наш новий заголовок
#include <string.h>
#include <stdio.h>

// Функція відправки (залишається без змін, як у попередній відповіді)
void WS_SendText(uint8_t sock, char *msg) {
    uint8_t frame[256];
    int msg_len = strlen(msg);
    int idx = 0;

    frame[idx++] = 0x81; // FIN + Text

    if (msg_len <= 125) {
        frame[idx++] = (uint8_t)msg_len;
    } else if (msg_len <= 65535) {
        frame[idx++] = 126;
        frame[idx++] = (msg_len >> 8) & 0xFF;
        frame[idx++] = msg_len & 0xFF;
    } else {
        return;
    }

    memcpy(&frame[idx], msg, msg_len);
    send(sock, frame, idx + msg_len);
}

// Оновлена функція Handshake
uint8_t WS_PerformHandshake(uint8_t sock, char* buffer) {
    char *key_start = strstr(buffer, "Sec-WebSocket-Key: ");
    if (!key_start) return 0;

    key_start += 19;
    char *key_end = strstr(key_start, "\r\n");
    if (!key_end) return 0;

    char client_key[64];
    int key_len = key_end - key_start;
    if (key_len > 60) key_len = 60;
    strncpy(client_key, key_start, key_len);
    client_key[key_len] = '\0';

    // Додаємо GUID
    strcat(client_key, WS_GUID);

    // --- SHA1 ---
    uint8_t sha_hash[20];
    SHA1_CTX sha;
    SHA1_Init(&sha); // Ініціалізація
    SHA1_Update(&sha, (uint8_t*)client_key, strlen(client_key)); // Хешування
    SHA1_Final(sha_hash, &sha); // Отримання результату

    // --- Base64 ---
    char accept_key[32];
    Base64_Encode(sha_hash, 20, accept_key); // Кодування

    // Відповідь
    char response[256];
    sprintf(response,
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n\r\n",
            accept_key);

    send(sock, (uint8_t*)response, strlen(response));
    return 1;
}
