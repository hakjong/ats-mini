#ifndef ETM_PLUS_H
#define ETM_PLUS_H

#include <stdint.h>

enum class EtmPlusScanResult : uint8_t
{
  COMPLETED,
  CANCELLED,
  SAVE_FAILED,
  NO_MEMORY,
  NO_CLOCK,
  UNSUPPORTED,
};

bool etmPlusSupported();
bool etmPlusCurrentHour(uint8_t *hour);
EtmPlusScanResult etmPlusScan();
uint16_t etmPlusNextFrequency(uint16_t current, int16_t direction);
bool etmPlusScanning();
uint16_t etmPlusScanFoundCount();
uint8_t etmPlusScanListCount();
uint16_t etmPlusScanFrequency(uint8_t index);
uint8_t etmPlusScanHour();

#endif // ETM_PLUS_H
