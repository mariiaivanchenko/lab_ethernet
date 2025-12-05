#ifndef WEBSOCKET_UTILS_H_
#define WEBSOCKET_UTILS_H_

#include <stdint.h>
#include <stddef.h>

// --- SHA1 Context ---
typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[64];
} SHA1_CTX;

// Функції SHA1
void SHA1_Init(SHA1_CTX* context);
void SHA1_Update(SHA1_CTX* context, const uint8_t* data, uint32_t len);
void SHA1_Final(uint8_t digest[20], SHA1_CTX* context);

// Функція Base64
// input: вхідний масив байтів (хеш)
// input_len: довжина вхідного масиву
// output: буфер для результату (рядка). Має бути достатнього розміру!
void Base64_Encode(const uint8_t* input, int input_len, char* output);

#endif /* WEBSOCKET_UTILS_H_ */
