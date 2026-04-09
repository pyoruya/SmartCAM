# SmartCAM

## 프로젝트 개요

안드로이드 스마트폰을 PC의 웹캠처럼 사용할 수 있게 해주는 애플리케이션입니다. 스마트폰의 카메라 영상을 USB 또는 Wi-Fi를 통해 PC로 전송하고, Windows에서는 이를 표준 카메라(웹캠) 디바이스로 인식하여 화상채팅, 인터넷 방송, 회의 등 일반 웹캠을 요구하는 모든 응용 프로그램에서 사용할 수 있도록 하는 것이 목표입니다.

## 목표

- 별도의 외장 웹캠 없이 보유한 스마트폰을 고품질 PC 카메라로 활용
- 화상채팅(Zoom, Teams, Google Meet 등) 및 인터넷 방송(OBS 등)에서 표준 카메라로 인식
- 안정적인 저지연 영상 스트리밍

## 범위

### 1차 타겟
- **모바일 OS**: Android (1차)
- **PC OS**: Windows
- **연결 방식**: USB, Wi-Fi

### 추후 확장 고려
- iOS 지원
- macOS / Linux 지원

## 아키텍처 (예정)

본 프로젝트는 두 개의 주요 컴포넌트로 구성됩니다.

### 1. Android 앱 (Sender)
- 카메라 캡처 (CameraX / Camera2 API)
- 영상 인코딩 (H.264 / MJPEG 등)
- USB(ADB/AOA) 또는 Wi-Fi(TCP/UDP/WebRTC) 전송

### 2. Windows 호스트 (Receiver + Virtual Camera)
- 스마트폰으로부터 영상 스트림 수신
- 디코딩 및 프레임 처리
- **가상 카메라 디바이스 등록**: Windows 응용 프로그램이 표준 캠으로 인식하도록 함
  - DirectShow Filter, Media Foundation Virtual Camera, 또는 third-party 가상 카메라 드라이버 활용 검토

## 핵심 기술 과제

- 저지연 영상 전송 (목표: < 100ms)
- Windows 가상 카메라 디바이스 구현 (드라이버 서명 이슈 포함)
- USB tethering 모드에서 안정적 데이터 채널 확보
- Wi-Fi 모드에서의 자동 PC 발견(mDNS 등) 및 페어링

## 디렉터리 구조 (예정)

```
SmartCAM/
├── android/         # Android 앱 (Kotlin)
├── windows/         # Windows 호스트 + 가상 카메라
├── docs/            # 설계 문서
└── CLAUDE.md
```

## 개발 가이드라인

- 신규 작업 시 먼저 영향 범위(android / windows / 양쪽)를 명확히 한다.
- 영상 포맷, 프로토콜, 가상 카메라 인터페이스 등 양 컴포넌트가 공유하는 계약은 변경 시 반드시 양쪽을 함께 수정한다.
- 외부 라이브러리 도입 시 라이선스(특히 Windows 드라이버 관련)를 확인한다.
