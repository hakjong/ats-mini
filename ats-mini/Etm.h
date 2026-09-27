#ifndef ETM_H
#define ETM_H

#include <stdint.h>

enum class EtmScanResult : uint8_t
{
  COMPLETED,
  CANCELLED,
  SAVE_FAILED,
  NO_MEMORY,
  UNSUPPORTED,
};

bool etmLoad(uint8_t band);
EtmScanResult etmScan();
uint16_t etmCount();
uint16_t etmNextFrequency(uint16_t current, int16_t direction);
bool etmScanning();
uint16_t etmScanFoundCount();
uint8_t etmScanListCount();
uint16_t etmScanFrequency(uint8_t index);

#endif // ETM_H
