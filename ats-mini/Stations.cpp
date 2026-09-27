#include "Common.h"
#include "Draw.h"
#include "Menu.h"
#include "Storage.h"
#include "Stations.h"
#include "Utils.h"
#include "KrFm.h"

#define STATION_LIMIT 256
#define STATION_VERSION 2

struct SavedStations
{
  uint8_t version;
  uint8_t mode;
  uint16_t count;
  uint16_t minimumFreq;
  uint16_t maximumFreq;
  uint16_t frequencies[STATION_LIMIT];
};

static SavedStations stations = {};
static uint8_t loadedBand = 255;
static uint16_t selected = 0;
static bool scanAborted = false;
static const SavedStations *activeScan = nullptr;
static uint16_t scanFoundCount = 0;
static uint16_t recentFound[5] = {};
static uint8_t recentCount = 0;

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
    for(uint16_t i = 0; i < stations.count; ++i)
      if(stations.frequencies[i] < current->minimumFreq ||
         stations.frequencies[i] > current->maximumFreq ||
         (i && stations.frequencies[i] <= stations.frequencies[i - 1]))
      {
        stations = {};
        break;
      }
  if(stations.version != STATION_VERSION)
  {
    stations.version = STATION_VERSION;
    stations.mode = current->bandMode;
    stations.minimumFreq = current->minimumFreq;
    stations.maximumFreq = current->maximumFreq;
  }
  krFmSetStations(stations.frequencies, current->bandMode == FM ? stations.count : 0);
}

uint16_t stationsCount() { return stations.count; }
uint16_t stationsSelected() { return selected; }
uint16_t stationsFrequency(uint16_t index)
{
  return index < stations.count ? stations.frequencies[index] : 0;
}

uint16_t stationsNextFrequency(uint16_t current, int16_t direction)
{
  stationsLoad(bandIdx);
  if(!stations.count || !direction) return 0;

  int32_t index = direction > 0 ? 0 : stations.count - 1;
  if(direction > 0)
  {
    for(uint16_t i = 0; i < stations.count; ++i)
      if(stations.frequencies[i] > current)
      {
        index = i;
        break;
      }
  }
  else
  {
    for(int16_t i = stations.count - 1; i >= 0; --i)
      if(stations.frequencies[i] < current)
      {
        index = i;
        break;
      }
  }

  int32_t steps = direction > 0 ? direction : -(int32_t)direction;
  int32_t offset = (steps - 1) % stations.count;
  if(direction < 0) offset = stations.count - offset;
  index = (index + offset) % stations.count;
  return stations.frequencies[index];
}

static bool saveStations(const SavedStations &updated)
{
  char key[16];
  stationKey(key, bandIdx);
  prefs.begin("stations", false, STORAGE_PARTITION);
  bool saved = prefs.putBytes(key, &updated, sizeof(updated)) == sizeof(updated);
  prefs.end();
  if(saved)
  {
    stations = updated;
    krFmSetStations(stations.frequencies, currentMode == FM ? stations.count : 0);
    clearStationInfo();
    identifyFrequency(currentFrequency);
  }
  return saved;
}

bool stationsClear()
{
  stationsLoad(bandIdx);
  SavedStations cleared = stations;
  cleared.count = 0;
  if(!saveStations(cleared)) return false;
  if(currentMode == FM)
  {
    krFmSetManualRegion(KR_FM_AUTO);
    prefsRequestSave(SAVE_SETTINGS, true);
    clearStationInfo();
    identifyFrequency(currentFrequency);
  }
  selected = STATION_CLEAR; // Keep Clear selected after removing the list.
  return true;
}

bool stationsAddCurrent()
{
  stationsLoad(bandIdx);
  uint16_t index = 0;
  while(index < stations.count && stations.frequencies[index] < currentFrequency) ++index;
  if(index < stations.count && stations.frequencies[index] == currentFrequency) return true;
  if(stations.count == STATION_LIMIT) return false;

  SavedStations updated = stations;
  for(uint16_t i = updated.count; i > index; --i)
    updated.frequencies[i] = updated.frequencies[i - 1];
  updated.frequencies[index] = currentFrequency;
  ++updated.count;
  return saveStations(updated);
}

bool stationsDeleteSelected()
{
  stationsLoad(bandIdx);
  if(selected < STATION_ACTION_COUNT || selected >= stations.count + STATION_ACTION_COUNT) return false;
  SavedStations updated = stations;
  uint16_t index = selected - STATION_ACTION_COUNT;
  for(uint16_t i = index; i + 1 < updated.count; ++i)
    updated.frequencies[i] = updated.frequencies[i + 1];
  updated.frequencies[--updated.count] = 0;
  if(!saveStations(updated)) return false;
  if(selected >= stations.count + STATION_ACTION_COUNT) --selected;
  return true;
}

bool stationsScanning() { return activeScan != nullptr; }
uint16_t stationsScanFoundCount() { return scanFoundCount; }
uint8_t stationsScanListCount() { return activeScan ? recentCount : 0; }
uint16_t stationsScanFrequency(uint8_t index)
{
  return activeScan && index < recentCount ? recentFound[index] : 0;
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

static void rememberStation(SavedStations &found, uint16_t freq)
{
  uint16_t index = 0;
  while(index < found.count && found.frequencies[index] < freq) ++index;
  if((index < found.count && found.frequencies[index] == freq) || found.count == STATION_LIMIT) return;
  for(uint16_t i = found.count; i > index; --i)
    found.frequencies[i] = found.frequencies[i - 1];
  found.frequencies[index] = freq;
  ++found.count;
  ++scanFoundCount;
  if(recentCount < ITEM_COUNT(recentFound))
    recentFound[recentCount++] = freq;
  else
  {
    for(uint8_t i = 1; i < recentCount; ++i)
      recentFound[i - 1] = recentFound[i];
    recentFound[recentCount - 1] = freq;
  }
}

bool stationsScan(bool append)
{
  if(isSSB()) return false; // The SI4732 cannot seek in SSB mode.

  stationsLoad(bandIdx);
  const Band *band = getCurrentBand();
  const uint16_t originalFreq = currentFrequency;
  SavedStations found = stations;
  if(!append) found.count = 0;

  scanAborted = false;
  scanFoundCount = 0;
  recentCount = 0;
  activeScan = &found;
  seekStop = false;
  clearStationInfo();
  muteOn(MUTE_TEMP, true);
  rx.setFrequency(band->minimumFreq);
  scanProgress(band->minimumFreq);
  rx.getCurrentReceivedSignalQuality();
  if(rx.getCurrentRSSI() >= (currentMode == FM ? 5 : 10) &&
     rx.getCurrentSNR() >= (currentMode == FM ? 2 : 3))
  {
    rememberStation(found, band->minimumFreq);
    drawScreen();
  }

  uint16_t previous = band->minimumFreq;
  while(!scanShouldStop())
  {
    rx.seekStationProgress(scanProgress, scanShouldStop, 1);
    if(scanAborted) break;
    if(rx.getBandLimit()) break;

    const uint16_t freq = rx.getFrequency();
    if(freq > band->maximumFreq) break;
    if(freq <= previous)
    {
      // A seek may time out without moving. Advance before trying again.
      uint32_t next = (uint32_t)previous + getCurrentStep()->spacing;
      if(next > band->maximumFreq) break;
      rx.setFrequency(next);
      previous = next;
      continue;
    }
    previous = freq;
    if(!rx.getStatusValid()) continue; // Seek timed out; resume from here.

    rx.getCurrentReceivedSignalQuality();
    if(rx.getCurrentRSSI() < (currentMode == FM ? 5 : 10) ||
       rx.getCurrentSNR() < (currentMode == FM ? 2 : 3)) continue;
    rememberStation(found, freq);
    drawScreen();
    if(freq == band->maximumFreq) break;
  }

  activeScan = nullptr;
  rx.setFrequency(originalFreq);
  currentFrequency = originalFreq;
  muteOn(MUTE_TEMP, false);
  clearStationInfo();
  identifyFrequency(currentFrequency);

  // A user stop still commits the stations found so far.
  if(!saveStations(found)) return false;
  if(!append && currentMode == FM)
  {
    krFmSetManualRegion(KR_FM_AUTO);
    prefsRequestSave(SAVE_SETTINGS, true);
    clearStationInfo();
    identifyFrequency(currentFrequency);
  }
  return true;
}

void stationsSelect(int16_t direction)
{
  stationsLoad(bandIdx);
  if(!direction) return;
  int16_t total = stations.count + STATION_ACTION_COUNT;
  selected = (selected + total + direction % total) % total;
  if(selected < STATION_ACTION_COUNT) return;
  updateFrequency(stations.frequencies[selected - STATION_ACTION_COUNT], false);
  clearStationInfo();
  identifyFrequency(currentFrequency);
  prefsRequestSave(SAVE_CUR_BAND);
}
