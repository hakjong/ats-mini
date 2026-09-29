#include "Common.h"
#include "Draw.h"
#include "Etm.h"
#include "KrFm.h"
#include "Menu.h"
#include "Storage.h"
#include "Utils.h"

#include <stdlib.h>

#define ETM_LIMIT   256
#define ETM_VERSION 1

struct SavedEtm
{
  uint8_t version;
  uint8_t mode;
  uint16_t count;
  uint16_t minimumFreq;
  uint16_t maximumFreq;
  uint16_t frequencies[ETM_LIMIT];
};

static SavedEtm *etm = nullptr;
static uint8_t loadedBand = 255;
static bool scanAborted = false;
static const SavedEtm *activeScan = nullptr;
static uint16_t scanFoundCount = 0;
static uint16_t recentFound[5] = {};
static uint8_t recentCount = 0;

static bool ensureEtm()
{
  if(etm) return true;
  etm = static_cast<SavedEtm *>(ps_malloc(sizeof(*etm)));
  if(etm) memset(etm, 0, sizeof(*etm));
  return etm != nullptr;
}

static void etmKey(char *key, uint8_t band)
{
  snprintf(key, 16, "Band-%u", band);
}

bool etmLoad(uint8_t band)
{
  if(!ensureEtm()) return false;
  const Band *current = &bands[band];
  if(loadedBand == band && etm->mode == current->bandMode &&
     etm->minimumFreq == current->minimumFreq && etm->maximumFreq == current->maximumFreq)
    return true;

  char key[16];
  etmKey(key, band);
  loadedBand = band;
  memset(etm, 0, sizeof(*etm));

  prefs.begin("etm", true, STORAGE_PARTITION);
  if(prefs.getBytesLength(key) == sizeof(*etm))
    prefs.getBytes(key, etm, sizeof(*etm));
  prefs.end();

  if(etm->version != ETM_VERSION || etm->mode != current->bandMode ||
     etm->minimumFreq != current->minimumFreq || etm->maximumFreq != current->maximumFreq ||
     etm->count > ETM_LIMIT)
    memset(etm, 0, sizeof(*etm));
  else
    for(uint16_t i = 0; i < etm->count; ++i)
      if(etm->frequencies[i] < current->minimumFreq ||
         etm->frequencies[i] > current->maximumFreq ||
         (i && etm->frequencies[i] <= etm->frequencies[i - 1]))
      {
        memset(etm, 0, sizeof(*etm));
        break;
      }

  if(etm->version != ETM_VERSION)
  {
    etm->version = ETM_VERSION;
    etm->mode = current->bandMode;
    etm->minimumFreq = current->minimumFreq;
    etm->maximumFreq = current->maximumFreq;
  }
  return true;
}

uint16_t etmCount()
{
  return etmLoad(bandIdx) ? etm->count : 0;
}

uint16_t etmNextFrequency(uint16_t current, int16_t direction)
{
  if(!etmLoad(bandIdx) || !etm->count || !direction) return 0;

  int32_t index = direction > 0 ? 0 : etm->count - 1;
  if(direction > 0)
  {
    for(uint16_t i = 0; i < etm->count; ++i)
      if(etm->frequencies[i] > current)
      {
        index = i;
        break;
      }
  }
  else
  {
    for(int16_t i = etm->count - 1; i >= 0; --i)
      if(etm->frequencies[i] < current)
      {
        index = i;
        break;
      }
  }

  int32_t steps = direction > 0 ? direction : -(int32_t)direction;
  int32_t offset = (steps - 1) % etm->count;
  if(direction < 0) offset = etm->count - offset;
  index = (index + offset) % etm->count;
  return etm->frequencies[index];
}

uint16_t etmFrequencyPosition(uint16_t current, uint16_t *total)
{
  if(!etmLoad(bandIdx))
  {
    if(total) *total = 0;
    return 0;
  }

  uint16_t position = 0;
  for(uint16_t i = 0; i < etm->count; ++i)
    if(etm->frequencies[i] == current)
    {
      position = i + 1;
      break;
    }
  if(total) *total = etm->count;
  return position;
}

static bool saveEtm(const SavedEtm &updated)
{
  char key[16];
  etmKey(key, bandIdx);
  prefs.begin("etm", false, STORAGE_PARTITION);
  bool saved = prefs.putBytes(key, &updated, sizeof(updated)) == sizeof(updated);
  prefs.end();
  if(saved) *etm = updated;
  return saved;
}

bool etmScanning() { return activeScan != nullptr; }
uint16_t etmScanFoundCount() { return scanFoundCount; }
uint8_t etmScanListCount() { return activeScan ? recentCount : 0; }
uint16_t etmScanFrequency(uint8_t index)
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

static void rememberFrequency(SavedEtm &found, uint16_t freq)
{
  uint16_t index = 0;
  while(index < found.count && found.frequencies[index] < freq) ++index;
  if((index < found.count && found.frequencies[index] == freq) || found.count == ETM_LIMIT) return;
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

EtmScanResult etmScan()
{
  if(isSSB()) return EtmScanResult::UNSUPPORTED;
  if(!etmLoad(bandIdx)) return EtmScanResult::NO_MEMORY;

  SavedEtm *found = static_cast<SavedEtm *>(ps_malloc(sizeof(*found)));
  if(!found) return EtmScanResult::NO_MEMORY;
  *found = *etm;
  found->count = 0;

  const Band *band = getCurrentBand();
  const uint16_t originalFreq = currentFrequency;
  scanAborted = false;
  scanFoundCount = 0;
  recentCount = 0;
  activeScan = found;
  seekStop = false;
  clearStationInfo();
  muteOn(MUTE_TEMP, true);
  rx.setFrequency(band->minimumFreq);
  scanProgress(band->minimumFreq);
  rx.getCurrentReceivedSignalQuality();
  if(rx.getCurrentRSSI() >= (currentMode == FM ? 5 : 10) &&
     rx.getCurrentSNR() >= (currentMode == FM ? 2 : 3))
  {
    rememberFrequency(*found, band->minimumFreq);
    drawScreen();
  }

  uint16_t previous = band->minimumFreq;
  while(!scanShouldStop())
  {
    rx.seekStationProgress(scanProgress, scanShouldStop, 1);
    if(scanAborted || rx.getBandLimit()) break;

    const uint16_t freq = rx.getFrequency();
    if(freq > band->maximumFreq) break;
    if(freq <= previous)
    {
      uint32_t next = (uint32_t)previous + getCurrentStep()->spacing;
      if(next > band->maximumFreq) break;
      rx.setFrequency(next);
      previous = next;
      continue;
    }
    previous = freq;
    if(!rx.getStatusValid()) continue;

    rx.getCurrentReceivedSignalQuality();
    if(rx.getCurrentRSSI() < (currentMode == FM ? 5 : 10) ||
       rx.getCurrentSNR() < (currentMode == FM ? 2 : 3)) continue;
    rememberFrequency(*found, freq);
    drawScreen();
    if(freq == band->maximumFreq) break;
  }

  activeScan = nullptr;
  rx.setFrequency(originalFreq);
  currentFrequency = originalFreq;
  muteOn(MUTE_TEMP, false);
  clearStationInfo();
  identifyFrequency(currentFrequency);

  EtmScanResult result = EtmScanResult::CANCELLED;
  if(!scanAborted)
    result = saveEtm(*found) ? EtmScanResult::COMPLETED : EtmScanResult::SAVE_FAILED;
  free(found);
  return result;
}
