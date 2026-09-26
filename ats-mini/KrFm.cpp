#include "Common.h"
#include "KrFm.h"

struct KrFmEntry
{
  uint16_t frequency;
  uint8_t region;
  const char *name;
};

#include "KrFmData.inc"

static uint16_t regionScores[ITEM_COUNT(krRegions)] = {};
static uint8_t homeRegion = KR_FM_AUTO;
static uint8_t manualRegion = KR_FM_AUTO;
static char identifiedName[64];

static const char *const regionLabels[] = {
  "Seoul Area", "Chuncheon", "Wonju", "Gangneung", "Donghae+",
  "Daejeon", "Cheongju", "Chungju", "Jeonju", "Namwon",
  "Gwangju", "Mokpo", "Yeosu+", "Daegu", "Andong", "Pohang",
  "Busan", "Ulsan", "Changwon", "Jinju+", "Jeju", "Seogwipo",
};

static_assert(ITEM_COUNT(regionLabels) == ITEM_COUNT(krRegions), "KR FM region labels must match the data");

uint8_t krFmRegionCount() { return ITEM_COUNT(krRegions); }
const char *krFmRegionLabel(uint8_t region)
{
  return region < ITEM_COUNT(regionLabels) ? regionLabels[region] : "Auto";
}
uint8_t krFmManualRegion() { return manualRegion; }
void krFmSetManualRegion(uint8_t region)
{
  manualRegion = region < ITEM_COUNT(krRegions) ? region : KR_FM_AUTO;
}

// Each mask includes its own area and neighboring areas. The order follows
// krRegions; boundaries are deliberately conservative for name lookup.
static const uint32_t nearbyRegions[] = {
  0x0000E7, // 수도권
  0x000007, // 춘천
  0x0000DF, // 원주
  0x00001C, // 강릉
  0x00C09C, // 동해·삼척·태백
  0x0001E1, // 대전
  0x0001E5, // 청주
  0x0060F5, // 충주
  0x083760, // 전주
  0x081700, // 남원
  0x101F00, // 광주
  0x301C00, // 목포
  0x1C1F00, // 여수·순천·광양
  0x0EE180, // 대구
  0x00E090, // 안동
  0x02E010, // 포항
  0x070000, // 부산
  0x07A000, // 울산
  0x0F3000, // 창원
  0x0C3300, // 진주·거창
  0x301C00, // 제주
  0x300800, // 서귀포
};

static_assert(ITEM_COUNT(nearbyRegions) == ITEM_COUNT(krRegions), "KR FM region masks must match the data");

static uint32_t regionMask(uint16_t frequency)
{
  uint32_t mask = 0;
  for(const KrFmEntry &entry : krFmEntries)
    if(entry.frequency == frequency) mask |= 1UL << entry.region;
  return mask;
}

void krFmSetStations(const uint16_t *frequencies, uint16_t count)
{
  memset(regionScores, 0, sizeof(regionScores));
  homeRegion = KR_FM_AUTO;
  uint16_t support[ITEM_COUNT(krRegions)] = {};
  for(uint16_t i = 0; i < count; ++i)
  {
    uint32_t mask = regionMask(frequencies[i]);
    if(!mask) continue;
    // Count each area once per frequency, even when several transmitters
    // within that area reuse it. Less widely reused frequencies weigh more.
    uint8_t weight = 24 / __builtin_popcount(mask);
    for(uint8_t region = 0; region < ITEM_COUNT(krRegions); ++region)
      if(mask & (1UL << region))
      {
        regionScores[region] += weight;
        ++support[region];
      }
  }

  uint8_t best = 0;
  uint16_t second = 0;
  for(uint8_t region = 1; region < ITEM_COUNT(krRegions); ++region)
    if(regionScores[region] > regionScores[best]) best = region;
  for(uint8_t region = 0; region < ITEM_COUNT(krRegions); ++region)
    if(region != best && regionScores[region] > second) second = regionScores[region];

  // A few shared frequencies do not establish the receiver's location.
  if(support[best] >= 3 && regionScores[best] >= second + 5)
    homeRegion = best;
}

const char *krFmName(uint16_t frequency)
{
  uint8_t activeRegion = manualRegion == KR_FM_AUTO ? homeRegion : manualRegion;
  if(activeRegion == KR_FM_AUTO) return nullptr;
  const KrFmEntry *best = nullptr;
  uint16_t bestScore = 0, secondScore = 0;
  uint8_t matches = 0;
  for(const KrFmEntry &entry : krFmEntries)
  {
    if(entry.frequency != frequency || !(nearbyRegions[activeRegion] & (1UL << entry.region))) continue;
    ++matches;
    // A manually selected area's own transmitter wins over neighboring
    // areas sharing the same frequency; scan scores decide other ties.
    uint16_t score = manualRegion != KR_FM_AUTO && entry.region == activeRegion ? UINT16_MAX : regionScores[entry.region];
    if(!best || score > bestScore)
    {
      secondScore = bestScore;
      bestScore = score;
      best = &entry;
    }
    else if(score > secondScore) secondScore = score;
  }

  // One frequency may belong to another area or to several transmitters in
  // the same area. Only a clearly better local candidate gets a name.
  if(!best || (matches > 1 && bestScore < secondScore + 5)) return nullptr;
  snprintf(identifiedName, sizeof(identifiedName), "%s (%s)", best->name, krRegions[best->region]);
  return identifiedName;
}
