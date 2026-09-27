# ATS Mini KR

ATS Mini 펌웨어 기능 개선 + 한국 FM 특화 기능 추가


## 변경점
- 기존 Memory -> Favorite 로 이름 변경
- Memory, ATS 기능 추가
- (TECSUN) ETM 기능 추가
- DAY, NIGHT 단파 밴드 추가
- Web UI 에서 저장된 주파수 조회, 다운로드, 복원
- 한국 FM 특화 기능 (★)
  - 수신 지역에 맞는 방송국명 표시
  - Memory ATS 주파수를 이용해 수신 지역 추론
- 기타 편의성 개선
  - Wifi 연결 도중 조작 가능
  - CPU Sleep 복귀 시 Sync Only 자동 재접속 방지
  - 짧은 Hold로 메뉴 닫기 지원
  - 메뉴 정리


# ATS Mini

![](docs/source/_static/esp32-si4732-ui-theme.jpg)

This firmware is for use on the SI4732 (ESP32-S3) Mini/Pocket Receiver

Based on the following sources:

* Volos Projects:    https://github.com/VolosR/TEmbedFMRadio
* PU2CLR, Ricardo:   https://github.com/pu2clr/SI4735
* Ralph Xavier:      https://github.com/ralphxavier/SI4735
* Goshante:          https://github.com/goshante/ats20_ats_ex
* G8PTN, Dave:       https://github.com/G8PTN/ATS_MINI

## Releases

Check out the [Releases](https://github.com/esp32-si4732/ats-mini/releases) page.

## Documentation

The hardware, software and flashing documentation is available at <https://esp32-si4732.github.io/ats-mini/>

## Discuss

* [GitHub Discussions](https://github.com/esp32-si4732/ats-mini/discussions) - the best place for feature requests, observations, sharing, etc.
* [TalkRadio Telegram Chat](https://t.me/talkradio/174172) - informal space to chat in Russian and English.
