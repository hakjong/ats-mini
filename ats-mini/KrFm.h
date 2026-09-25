#ifndef KR_FM_H
#define KR_FM_H

#include <stdint.h>

void krFmSetStations(const uint16_t *frequencies, uint8_t count);
const char *krFmName(uint16_t frequency);

#endif
