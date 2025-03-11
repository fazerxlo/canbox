// src/slcan.h
#ifndef SLCAN_H
#define SLCAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void slcan_process_char(uint8_t ch);

#ifdef __cplusplus
}
#endif

#endif // SLCAN_H