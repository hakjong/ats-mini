#include "Common.h"
#include "Draw.h"
#include "Menu.h"
#include "Storage.h"
#include "Stations.h"
#include "Utils.h"

#define STATION_LIMIT 128
#define STATION_VERSION 1

struct SavedStations
{
  uint8_t version;
  uint8_t mode;
  uint8_t count;
  uint8_t reserved;
  uint16_t minimumFreq;
  uint16_t maximumFreq;
  uint16_t frequencies[STATION_LIMIT];
};

static SavedStations stations = {};
static uint8_t loadedBand = 255;
static uint8_t selected = 0;
static bool scanAborted = false;

static void stationKey(char *key, uint8_t band)
{
  snprintf(key, 16, "Band-%u", band);
}

void stationsLoad(uint8_t band)
{
  const Band *current = &bands[band];
  if(loadedBand == band && stations.mode == current->bandMode &&
     stations.minimumFreq == current->minimumFreq &&
     stations.maximumFreq == current->maximumFreq) return;

  char key[16];
  stationKey(key, band);
  loadedBand = band;
  selected = 0;
  stations = {};

  prefs.begin("stations", true, STORAGE_PARTITION);
  if(prefs.getBytesLength(key) == sizeof(stations))
    prefs.getBytes(key, &stations, sizeof(stations));
  prefs.end();

  if(stations.version != STATION_VERSION || stations.mode != current->bandMode ||
     stations.minimumFreq != current->minimumFreq ||
     stations.maximumFreq != current->maximumFreq || stations.count > STATION_LIMIT)
    stations = {};
  else
    for(uint8_t i = 0; i < stations.count; ++i)
      if(stations.frequencies[i] < current->minimumFreq ||
         stations.frequencies[i] > current->maximumFreq ||
         (i && stations.frequencies[i] <= stations.frequencies[i - 1]))
      {
        stations = {};
        break;
      }
}

uint8_t stationsCount() { return stations.count; }
uint8_t stationsSelected() { return selected; }
uint16_t stationsFrequency(uint8_t index)
{
  return index < stations.count ? stations.frequencies[index] : 0;
}

static void scanProgress(uint16_t freq)
{
  currentFrequency = freq;
  drawScreen();
}

static bool scanShouldStop()
{
  if(consumeAbortPending()) scanAborted = true;
  return scanAborted;
}

bool stationsScan()
{
  if(isSSB()) return false; // The SI4732 cannot seek in SSB mode.

  stationsLoad(bandIdx);
  const Band *band = getCurrentBand();
  const uint16_t originalFreq = currentFrequency;
  SavedStations found = {};
  found.version = STATION_VERSION;
  found.mode = currentMode;
  found.minimumFreq = band->minimumFreq;
  found.maximumFreq = band->maximumFreq;

  scanAborted = false;
  seekStop = false;
  muteOn(MUTE_TEMP, true);
  rx.setFrequency(band->minimumFreq);
  rx.getCurrentReceivedSignalQuality();
  if(rx.getCurrentRSSI() >= (currentMode == FM ? 5 : 10) &&
     rx.getCurrentSNR() >= (currentMode == FM ? 2 : 3))
    found.frequencies[found.count++] = band->minimumFreq;

  uint16_t previous = band->minimumFreq;
  while(found.count < STATION_LIMIT && !scanShouldStop())
  {
    rx.seekStationProgress(scanProgress, scanShouldStop, 1);
    if(scanAborted) break;
    if(rx.getBandLimit() || !rx.getStatusValid()) break;

    const uint16_t freq = rx.getFrequency();
    if(freq <= previous || freq > band->maximumFreq) break;
    rx.getCurrentReceivedSignalQuality();
    if(rx.getCurrentRSSI() < (currentMode == FM ? 5 : 10) ||
       rx.getCurrentSNR() < (currentMode == FM ? 2 : 3)) break;
    found.frequencies[found.count++] = freq;
    previous = freq;
    if(freq == band->maximumFreq) break;
  }

  rx.setFrequency(originalFreq);
  currentFrequency = originalFreq;
  muteOn(MUTE_TEMP, false);
  clearStationInfo();
  identifyFrequency(currentFrequency);

  if(scanAborted) return false;

  char key[16];
  stationKey(key, bandIdx);
  prefs.begin("stations", false, STORAGE_PARTITION);
  bool saved = prefs.putBytes(key, &found, sizeof(found)) == sizeof(found);
  prefs.end();
  if(saved)
  {
    stations = found;
    selected = 0;
  }
  return saved;
}

void stationsSelect(int16_t direction)
{
  stationsLoad(bandIdx);
  if(!stations.count || !direction) return;
  selected = (selected + stations.count + direction % stations.count) % stations.count;
  updateFrequency(stations.frequencies[selected], false);
  clearStationInfo();
  identifyFrequency(currentFrequency);
  prefsRequestSave(SAVE_CUR_BAND);
}
