#ifndef KR_FM_H
#define KR_FM_H

#include <stdint.h>

#define KR_FM_AUTO 255

void krFmSetStations(const uint16_t *frequencies, uint16_t count);
uint8_t krFmRegionCount();
const char *krFmRegionLabel(uint8_t region);
uint8_t krFmManualRegion();
void krFmSetManualRegion(uint8_t region);
const char *krFmName(uint16_t frequency);

#endif
