# Drivers Build and Run Guide

This project builds the Raspberry Pi application and the kernel modules used for motor and SHT20 control.

## 1. Environment setup

Install the required packages and enable the device interfaces first.

```bash
chmod +x setup_drivers.sh
./setup_drivers.sh
```

The setup script installs:

- `build-essential`
- Linux kernel headers
- `libi2c-dev`
- `nlohmann-json3-dev`
- Eclipse Paho MQTT C / C++ libraries

Required Raspberry Pi interfaces:

- Enable `I2C`
- Enable `SPI`
- Keep the MCU UART available at `/dev/ttyS0`

If needed, use `sudo raspi-config` to enable `I2C` and `SPI`.

## 2. Configure `config/config.json`

Set the broker address, topics, and subway API key for your environment.

```json
{
  "mqtt_broker": "tcp://<server-ip>:1883",
  "subway_api_key": "<seoul-open-api-key>",
  "sensor_topic": "sensor/air_quality",
  "sht20_topic": "sensor/temp_humi"
}
```

## 3. Build

Build the kernel modules and the application.

```bash
make all
```

The application binary is created at `build/drivers`.

## 4. Load kernel modules

`build.sh` still handles the module load and device permission setup.

```bash
chmod +x build.sh
./build.sh
```

It performs:

1. `make all`
2. `insmod src/motor_driver.ko`
3. `insmod src/sht20_driver.ko`
4. Permission setup for `/dev/motor`, `/dev/sht20`, `/dev/ttyS0`

## 5. Run

Start the integrated service:

```bash
./build/drivers
```

At startup, the process now runs these features together:

- MQTT communication for motor, sensor, display, and WAV control
- UART communication with the STM32 on `/dev/ttyS0`
- SHT20 monitoring
- System status publishing
- Qt -> Raspberry Pi -> MCU audio streaming server

Audio streaming details:

- TCP listen port: `5000`
- Expected frame payload: `1920` bytes
- SPI output device: `/dev/spidev0.0`
- Optional PCM dump file: `received_48k_mono.pcm`

The audio stream server accepts a TCP client from Qt and forwards each received audio frame to the MCU over SPI while the rest of the application continues running.

## 6. Notes

- If shell scripts are not executable, run `chmod +x *.sh`
- Check loaded modules with `lsmod | grep -E "motor_driver|sht20_driver"`
- Check device nodes with `ls /dev/motor /dev/sht20 /dev/ttyS0 /dev/spidev0.0`
- Unload modules with `sudo rmmod motor_driver sht20_driver`
- `config/config.json` may contain broker and API credentials, so avoid committing real secrets
