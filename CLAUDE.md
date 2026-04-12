# SmartCAM

## 프로젝트 개요

안드로이드 스마트폰을 PC의 웹캠처럼 사용할 수 있게 해주는 애플리케이션입니다. 스마트폰을 USB로 PC에 연결하면 Windows 11이 표준 카메라(웹캠) 디바이스로 인식하여 화상채팅, 인터넷 방송, 회의, 녹화 등 일반 웹캠을 요구하는 모든 응용 프로그램에서 사용할 수 있도록 하는 것이 목표입니다.

## 목표

- 별도의 외장 웹캠 없이 보유한 (구형) 스마트폰을 PC 카메라로 활용
- 화상채팅(Zoom, Teams, Google Meet 등) 및 인터넷 방송(OBS 등)에서 표준 카메라로 자동 인식
- 사용자가 매번 PC에서 수동으로 프로그램을 실행할 필요 없는 매끄러운 UX
- 별도의 드라이버 설치 절차 없음 (관리자 권한 불필요)

## 1차 범위

- **모바일 OS**: Android (Kotlin)
- **PC OS**: Windows 11 (22000+) 전용 — Windows 10 미지원
- **연결 방식**: USB only (Wi-Fi는 후순위)
- **출력**: 영상 only (오디오는 v1 제외, PC 마이크 사용 권장)

## 목표 사용자 경험 (UX)

### 최초 1회 (5분 이내)
1. PC에 SmartCAM 호스트 앱 설치 (.exe, 관리자 권한 불필요, HKCU 설치)
2. 폰에 SmartCAM Android 앱 설치 (APK)
3. 폰 개발자 옵션 → USB 디버깅 ON
4. 첫 USB 연결 시 폰에서 "이 PC를 신뢰" 1회 탭

### 이후 매번
1. 폰을 USB로 PC에 연결 (잠금 해제 상태)
2. Zoom/Teams/카메라 앱에서 카메라 = "SmartCAM" 선택. 끝.

폰 SmartCAM 앱은 호스트가 ADB로 자동 실행시킴 (이미 실행 중이면 중복 실행하지 않음). 호스트 앱은 Windows 시작 시 자동으로 시스템 트레이에 상주하며, 사용자가 매번 클릭할 필요 없음.

## 아키텍처

### 데이터 흐름

```
[폰 카메라 (CameraX)]
    ↓ Surface
[MediaCodec H.264 HW 인코더]
    ↓ NAL units (Annex-B)
[TCP socket: localhost:<port> listen on phone]
    ↓ USB (ADB reverse port forward)
[PC: localhost:<port>]
    ↓
[SmartCAM 호스트 앱 (C++, 트레이 상주)]
    ↓
[Media Foundation H.264 디코더]
    ↓ IMFSample (NV12)
[IMFMediaSource 구현 (COM)]
    ↓
[MFCreateVirtualCamera → Windows Frame Server]
    ↓
[Zoom / Teams / Windows 카메라 앱 / OBS 등]
```

### 1. Android 앱 (Sender)
- **언어**: Kotlin
- **카메라**: CameraX (구형 기기 호환성 처리 위임)
- **인코딩**: MediaCodec 하드웨어 H.264 인코더, Surface→Surface zero-copy
- **전송**: TCP 소켓 (localhost listen, ADB reverse가 USB로 터널링)
- **minSdk**: 24 (Android 7.0) 잠정
- **해상도 폴백 체인**: 4K30 → 1440p30 → 1080p30 → 720p30
  - 앱 시작 시 `MediaCodec` capability 조회 후 가능한 최고 해상도 자동 선택
- **단일 인스턴스**: 호스트가 `am start`로 launch 시 중복 실행 방지 (`singleTask` launchMode + 기존 인스턴스 재사용)

### 2. Windows 호스트 (Receiver + Virtual Camera + Tray)
- **언어**: C++ (Win32 + Media Foundation, COM)
- **가상 카메라 방식**: **Media Foundation Virtual Camera (MFVCam)**
  - `MFCreateVirtualCamera` API 사용 (Windows 11 22000+)
  - `IMFMediaSource` 를 구현한 COM 클래스
  - 첫 실행 시 HKCU에 self-register (관리자 권한 불필요)
- **트레이 상주**:
  - HKCU `Run` 레지스트리 키에 등록하여 Windows 시작 시 자동 실행
  - 시스템 트레이 아이콘으로 상태 표시 (연결됨/대기/오류)
- **ADB 통합**:
  - `adb.exe`를 호스트 앱과 함께 번들 (사용자가 SDK 설치 불필요)
  - 폰 연결 감지 → `adb reverse tcp:<port> tcp:<port>`
  - `adb shell am start` 로 폰 SmartCAM 앱 자동 launch
  - 앱이 이미 실행 중인지 `adb shell pidof` 또는 `dumpsys` 로 확인 후 중복 실행 방지
- **디코딩**: Media Foundation 내장 H.264 디코더 → NV12 IMFSample
- **해상도 협상**: 폰이 보내오는 SPS/PPS 파싱하여 가상 카메라가 노출하는 미디어 타입 결정

### 3. 명시적으로 v1에서 제외된 항목
- **오디오 / 가상 마이크**: Windows 11에 공식 가상 마이크 API가 없어 커널 드라이버 없이 구현 불가. v1은 영상만 지원, 사용자는 PC 마이크 사용. v2에서 재검토.
- **Wi-Fi 연결 모드**
- **iOS / macOS / Linux 지원**

## 핵심 기술 과제 / 리스크

- **MFVCam Frame Server 통합**: COM 클래스 self-registration (HKCU 범위)이 모든 카메라 앱에서 정상 인식되는지 검증 필요
- **구형폰의 4K30 미지원**: 폴백 체인으로 대응하되, 최저 720p30은 보장
- **첫 USB 디버깅 승인**: 사용자가 폰 화면에서 1회 탭해야 함 — 회피 불가
- **저지연**: 목표 end-to-end < 150ms (캡처 → PC 가상 캠 출력)
- **가상 카메라 lifecycle**: 호스트 앱 종료 시 가상 캠도 깔끔하게 사라져야 함

## 디렉터리 구조 (예정)

```
SmartCAM/
├── android/                    # Android Studio 프로젝트 (Kotlin + CameraX)
│   └── app/
├── windows/                    # Visual Studio 솔루션 (C++)
│   ├── SmartCAMHost/           # 트레이 앱 + ADB 통합 + 수신/디코딩
│   └── SmartCAMVCam/           # IMFMediaSource COM 컴포넌트
├── third_party/
│   └── adb/                    # 번들된 adb.exe (Android Platform Tools)
├── protocol/                   # 양 컴포넌트가 공유하는 와이어 포맷 정의
├── docs/                       # 설계 문서
├── README.md
└── CLAUDE.md
```

## 와이어 프로토콜 (초안)

TCP 한 개 채널 위에 다음 순서로:

1. **헤드셰이크** (PC → 폰): 매직 4바이트 + 프로토콜 버전
2. **폰 capability** (폰 → PC): 지원 해상도 목록, 코덱 프로파일
3. **선택된 포맷** (PC → 폰): 해상도, FPS, 비트레이트
4. **스트림 시작** 후: `[4바이트 length BE][NAL unit (Annex-B)]` 반복
5. **종료**: TCP FIN

추후 양방향 컨트롤 메시지(노출/포커스 제어 등) 추가 가능성 → 채널 분리 검토.

## 빌드 및 테스트

**→ [docs/BUILD_AND_TEST.md](docs/BUILD_AND_TEST.md) 참고**

빌드 환경 세팅, 빌드 명령, 실행 방법, 통합 테스트 절차, 현재 구현 상태 및 다음 작업 항목이 모두 해당 문서에 정리되어 있다. 새 환경에서 클론 후 이 문서를 먼저 참조한다.

## 개발 가이드라인

- 신규 작업 시 먼저 영향 범위(android / windows / protocol / 양쪽)를 명확히 한다.
- 영상 포맷, 프로토콜, 가상 카메라 인터페이스 등 양 컴포넌트가 공유하는 계약은 변경 시 양쪽을 함께 수정한다.
- 외부 라이브러리 도입 시 라이선스를 확인한다 (특히 GPL 계열은 폐쇄 배포 시 문제 가능).
- Windows 호스트는 **관리자 권한을 절대 요구하지 않는다**. 모든 등록은 HKCU 범위로.
- ADB 동작은 백그라운드에서 사용자에게 보이지 않게 처리하되, 실패 시 트레이 알림으로 명확히 안내.
