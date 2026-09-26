#ifndef STATIONS_H
#define STATIONS_H

#include <stdint.h>

#define STATION_CLEAR_SCAN   0
#define STATION_APPEND_SCAN  1
#define STATION_CLEAR        2
#define STATION_ACTION_COUNT 3

void stationsLoad(uint8_t band);
bool stationsScan(bool append);
void stationsSelect(int16_t direction);
bool stationsClear();
bool stationsDeleteSelected();
uint16_t stationsCount();
uint16_t stationsSelected();
uint16_t stationsFrequency(uint16_t index);
uint16_t stationsNextFrequency(uint16_t current, int16_t direction);
bool stationsScanning();
uint16_t stationsScanFoundCount();
uint8_t stationsScanListCount();
uint16_t stationsScanFrequency(uint8_t index);

#endif // STATIONS_H
