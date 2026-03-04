# STM32F401_EdgeDevice Build Guide

## 1) 준비 사항

아래 도구가 PATH에 잡혀 있어야 합니다.

- `cmake`
- `make` (Windows에서는 `GnuWin32 make` 또는 동등 도구)
- `arm-none-eabi-gcc`

확인 명령:

```powershell
cmake --version
make --version
arm-none-eabi-gcc --version
```

## 2) 빌드 방법 (CMake)

프로젝트 루트(이 파일이 있는 폴더)에서 실행:

```powershell
cmake -S . -B build_cmake -G "Unix Makefiles"
cmake --build build_cmake -j 8
```

성공 시 생성 파일:

- `build_cmake/STM32F401_EdgeDevice` (ELF)
- `build_cmake/STM32F401_EdgeDevice.hex`
- `build_cmake/STM32F401_EdgeDevice.bin`

## 3) 보드에 플래시 (stlink)

ST-Link 연결 후:

```powershell
st-info --probe
st-flash --reset write .\build_cmake\STM32F401_EdgeDevice.bin 0x08000000
```

## 4) 참고

- 이 프로젝트 산출물은 STM32(ARM)용 펌웨어라서 PC에서 직접 실행(run)하지 않습니다.
- `run`은 디버거(OpenOCD/st-util/ST-Link GDB Server)로 보드에 붙어서 진행해야 합니다.
