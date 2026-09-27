#ifndef STATIONS_H
#define STATIONS_H

#include <stdint.h>

#define STATION_ADD_CURRENT  0
#define STATION_ATS_SCAN     1
#define STATION_CLEAR        2
#define STATION_ACTION_COUNT 3

enum class StationAddResult : uint8_t
{
  ADDED,
  ALREADY_SAVED,
  LIST_FULL,
  SAVE_FAILED,
};

enum class StationScanResult : uint8_t
{
  COMPLETED,
  CANCELLED,
  SAVE_FAILED,
  UNSUPPORTED,
};

void stationsLoad(uint8_t band);
StationScanResult stationsScan();
void stationsSelect(int16_t direction);
StationAddResult stationsAddCurrent();
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
