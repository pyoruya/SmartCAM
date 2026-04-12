# 빌드 및 테스트 가이드

## 필요 도구

| 도구 | 용도 | 비고 |
|---|---|---|
| Visual Studio 2022 Community | Windows 호스트 빌드 (C++) | C++ 데스크톱 개발 워크로드 필수 |
| Android Studio (Panda+) | Android 앱 빌드 | 또는 `gradlew.bat`으로 CLI 빌드 가능 |
| adb.exe | 폰-PC 통신 | Android SDK 설치 시 자동 포함, 또는 별도 배치 |
| GitHub CLI (gh) | 리포지토리 관리 | `winget install GitHub.cli` |

---

## 1. 리포지토리 클론

```powershell
cd C:\Works\Programming
git clone https://github.com/pyoruya/SmartCAM.git
cd SmartCAM
```

---

## 2. Windows 호스트 빌드

### 2-1. CMake 구성 및 빌드

```powershell
# VS2022에 포함된 cmake 사용
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

cd C:\Works\Programming\SmartCAM\windows

# CMake 구성 (최초 1회)
& $cmake --preset x64-debug

# 빌드 (전체)
& $cmake --build build/x64-debug --config Debug

# 또는 개별 타겟 빌드
& $cmake --build build/x64-debug --target SmartCAMHost
& $cmake --build build/x64-debug --target SmartCAMVCam
```

### 2-2. 빌드 결과물 위치

```
windows/build/x64-debug/SmartCAMHost/Debug/SmartCAMHost.exe
windows/build/x64-debug/SmartCAMVCam/Debug/SmartCAMVCam.dll
```

### 2-3. 빌드 참고사항

- cmake가 시스템 PATH에 없을 수 있음. VS2022 내장 cmake 전체 경로 사용 필요
- bash 환경(Git Bash)에서는 경로를 `/c/Program Files/...` 형식으로 사용
- `WIN32_LEAN_AND_MEAN`, `NOMINMAX`은 CMakeLists.txt에 이미 정의됨 (winsock 충돌 방지)
- Release 빌드: `--preset x64-release` + `--config Release` 사용

---

## 3. adb.exe 배치

SmartCAMHost가 폰을 감지하려면 adb.exe가 필요하다.

### 자동 탐색 순서 (SmartCAMHost 내부)

1. `SmartCAMHost.exe`와 같은 디렉터리
2. `SmartCAM/third_party/adb/` (상대 경로)
3. `%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe`

### adb만 별도 배치하는 경우

1. https://developer.android.com/tools/releases/platform-tools 에서 다운로드
2. 압축 해제 후 아래 3개 파일을 `SmartCAM/third_party/adb/`에 복사:
   - `adb.exe`
   - `AdbWinApi.dll`
   - `AdbWinUsbApi.dll`
3. 이 파일들은 `.gitignore`에 의해 git 추적 제외됨

---

## 4. Android 앱 빌드

### 방법 A: Android Studio 사용

1. Android Studio 실행
2. **Open** → `C:\Works\Programming\SmartCAM\android` 선택
3. Gradle sync 완료 대기
4. **Build → Compile All** 또는 **Run → Run 'app'** (Shift+F10)

### 방법 B: CLI 빌드

```powershell
cd C:\Works\Programming\SmartCAM\android

# 빌드
.\gradlew.bat assembleDebug

# APK 위치
# app/build/outputs/apk/debug/app-debug.apk

# 폰에 설치
adb install app\build\outputs\apk\debug\app-debug.apk
```

### Android 빌드 참고사항

- AGP 9.1.0 + Kotlin 2.2.10 (Android Studio 자동 업그레이드 반영)
- minSdk 24 (Android 7.0), targetSdk 35
- CameraX 1.4.1 사용
- `JAVA_HOME` / `ANDROID_HOME` 환경변수가 설정되어 있지 않으면 Android Studio 설치 시 자동 설정됨

---

## 5. SmartCAMHost.exe 실행

```powershell
cd C:\Works\Programming\SmartCAM\windows\build\x64-debug\SmartCAMHost\Debug
.\SmartCAMHost.exe
```

### Debug 빌드 동작

- 콘솔 창이 함께 열려 로그 출력
- 시스템 트레이에 SmartCAM 아이콘 표시
- 종료: 트레이 아이콘 **우클릭 → Quit**

### 정상 시작 로그

```
[Main] SmartCAM Host starting...
[Decoder] Initialized successfully
[Tray] Initialized
[Main] Using ADB: C:\...\adb.exe
[Main] Running. Right-click tray icon to quit.
```

### 폰 연결 시 로그

```
[ADB] Device connected: XXXXXXXXX
[Main] Device XXXXXXXXX connected, setting up...
[Receiver] Connecting to localhost:38247...
[Receiver] Connected
[Receiver] Streaming: 1920x1080 @ 30fps
[Main] Stream started: 1920x1080 @ 30fps
```

---

## 6. 통합 테스트

### 사전 준비

1. 안드로이드 폰: **설정 → 개발자 옵션 → USB 디버깅 ON**
2. USB 케이블로 PC에 연결
3. 폰 화면에서 "이 컴퓨터를 신뢰" 허용 (최초 1회)

### 테스트 순서

1. PC에서 `SmartCAMHost.exe` 실행
2. 폰을 USB로 연결
3. Host가 자동으로: 폰 감지 → ADB reverse → SmartCAM 앱 launch → 스트림 수신
4. 콘솔 로그로 상태 확인

### 현재 구현 상태 및 제한

| 기능 | 상태 |
|---|---|
| 폰 카메라 캡처 + H.264 인코딩 | ✅ 구현 완료 |
| TCP 스트리밍 (와이어 프로토콜 v1) | ✅ 구현 완료 |
| ADB 디바이스 감지 + reverse + 앱 launch | ✅ 구현 완료 |
| H.264 디코딩 (Media Foundation) | ✅ 구현 완료 |
| 공유 메모리 IPC (Host → VCam) | ✅ 구현 완료 |
| 트레이 아이콘 | ✅ 구현 완료 |
| **가상 카메라 등록 (MFCreateVirtualCamera)** | ❌ 미완성 |
| **IMFMediaSource 프레임 전달** | ❌ 미완성 |
| **Zoom/Teams에서 "SmartCAM" 카메라 인식** | ❌ 미완성 (위 2개 완성 필요) |

### 다음 구현 작업

1. `SmartCAMVCam/src/media_source.cpp` — IMFMediaSource가 SharedFrameBuffer에서 NV12 프레임을 읽어 IMFSample로 전달하도록 완성
2. `SmartCAMHost/src/main.cpp` — `MFCreateVirtualCamera` API 호출로 가상 카메라를 Windows Frame Server에 등록
3. 통합 테스트: Zoom/Teams/Windows 카메라 앱에서 "SmartCAM" 카메라 선택 확인

---

## 트러블슈팅

### cmake 명령을 찾을 수 없음
- VS2022 내장 cmake 전체 경로 사용. bash에서: `"/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"`

### adb.exe not found 로그
- 위 "3. adb.exe 배치" 참고

### Android Gradle sync 에러
- Android Studio에서 **File → Sync Project with Gradle Files** 재시도
- JDK 17 이상 필요 (Android Studio 번들 JDK 사용 권장)

### winsock 재정의 에러
- `WIN32_LEAN_AND_MEAN`이 CMakeLists.txt에 정의되어 있는지 확인

### 폰에서 "이 PC를 신뢰" 팝업이 안 나옴
- USB 케이블이 충전 전용이 아닌 데이터 케이블인지 확인
- 폰에서 개발자 옵션 → USB 디버깅 해제 후 재활성화
