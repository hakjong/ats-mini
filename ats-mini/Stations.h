#ifndef STATIONS_H
#define STATIONS_H

#include <stdint.h>

void stationsLoad(uint8_t band);
bool stationsScan();
void stationsSelect(int16_t direction);
uint8_t stationsCount();
uint8_t stationsSelected();
uint16_t stationsFrequency(uint8_t index);
bool stationsScanning();
uint16_t stationsScanFoundCount();
uint8_t stationsScanListCount();
uint16_t stationsScanFrequency(uint8_t index);

#endif // STATIONS_H
