#ifndef STATUS_H
#define STATUS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void status_init(void);
void status_update(void);
void build_status_json(char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif // STATUS_H
