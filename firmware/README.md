## STM32F401 Edge Device

### MCU:
STM32F401RE

### Tool:
STM32CubeIDE(2.0.0)

### Features:
- UART Debug
- Multi Sensor
- SDCard Read(SPI)
- MAX98357A AMP Class D Amplifier(I2S)
- LED Matrix(HUB75)

---

### Project File Structure

| Category        | Path / File                                               | Description                                                        |
| --------------- | --------------------------------------------------------- | ------------------------------------------------------------------ |
| IDE Config      | `.project`<br>`.cproject`<br>`.mxproject`<br>`.settings/` | STM32CubeIDE / Eclipse project configuration files                 |
| Hardware Config | `*.ioc`                                                   | Pin mapping, clock configuration, and peripheral settings (CubeMX) |
| User Code       | `Core/`<br>`Startup/`                                     | Application source code and startup files                          |
| Drivers         | `Drivers/`                                                | STM32 CMSIS and HAL driver libraries                               |
| Linker Script   | `*.ld`                                                    | Flash/RAM memory layout configuration                              |
| Git Config      | `.gitignore`                                              | Excludes build artifacts and temporary files                       |

---

| 구분 | 경로 / 파일 | 설명 |
|------|-------------|------|
| IDE 설정 |`.project`<br>`.cproject`<br>`.mxproject`<br>`.settings/` | CubeIDE 프로젝트 설정 |
| 하드웨어 설정 | `*.ioc` | 핀맵, 클럭, 주변장치 설정 |
| 사용자 코드 | `Core/`<br>`Startup/` | 펌웨어 메인 소스 코드 |
| 드라이버 | `Drivers/` | CMSIS / HAL 라이브러리 |
| 메모리 설정 | `*.ld` | Flash / RAM 메모리 배치 |
| Git 설정 | `.gitignore` | 빌드 파일 제외 |
---