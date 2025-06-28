# 🕉️ ESP32 Motion-Triggered MP3 Audio Playback

This project uses an **ESP32**, **PIR motion sensor**, **SD card**, and a **MAX98357A I2S amplifier** to play MP3 audio files when motion is detected. It plays a greeting sound, followed by a random short audio clip and a random dhun from SD card folders.

---

## 🎯 Features

- 🏃‍♂️ Motion detection via PIR sensor
- 🎵 Plays greeting: `/jay-swaminarayan.mp3`
- 🔀 Random MP3 playback from `/short` and `/dhun` folders
- 🎧 I2S audio output via MAX98357A
- ⏱️ Auto timeout after 5 seconds of inactivity
- 🧪 Serial logs for debugging and monitoring

---

## 🔌 Hardware Connections

### MAX98357A I2S Amplifier
| ESP32 Pin  | MAX98357A Pin     |
|------------|-------------------|
| GPIO26     | BCLK (Bit Clock)  |
| GPIO25     | LRCLK (Word Select) |
| GPIO22     | DIN (Data In)     |
| GND        | GND               |
| 3.3V       | VIN               |

### SD Card Module
| ESP32 Pin  | SD Card Module Pin |
|------------|--------------------|
| 5V or 3.3V | VCC                |
| GND        | GND                |
| GPIO5      | CS                 |
| GPIO18     | SCK                |
| GPIO19     | MISO               |
| GPIO23     | MOSI               |

### PIR Motion Sensor
| ESP32 Pin  | PIR Sensor Pin |
|------------|----------------|
| 3.3V or 5V | VCC            |
| GND        | GND            |
| GPIO4      | OUT            |

> ⚠️ Ensure proper voltage levels for your modules. Most SD and PIR modules support 3.3V and 5V.

---

## 💾 SD Card File Structure

Place your MP3 files as follows on the root of the SD card:

