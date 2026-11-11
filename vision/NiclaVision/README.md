# Nicla Vision victim camera

The [OpenMV script](main.py) uses the existing ring color classifier and
the onboard distance sensor to recognize Harmed, Stable, and Unharmed
markers. The ESP32 receives the classification and position over UART.

## 1. Wire the camera

Power both boards over USB for the first test. Use the Nicla Vision header
with the pads labeled SDA and SCL. The script uses UART4 at 115200 baud.

| Nicla Vision | ESP32 Dev Module |
| --- | --- |
| **SDA** (UART4 TX) | **GPIO16** (UART RX) |
| **GND** | **GND** |

Only the Nicla transmits, so leave its SCL (UART4 RX) unconnected.
[OpenMV UART pinout](https://docs.openmv.io/dev/openmvcam/quickref/arduino-nicla-vision.html#uart)

## 2. Run the camera by itself

Open `main.py` in OpenMV IDE, connect the Nicla, and press Run. Place a
victim marker in front of the camera and check the label in the preview.
To run it at power-on, save the script as `main.py` on the Nicla's flash.
[Getting started with OpenMV on Nicla Vision](https://docs.arduino.cc/tutorials/nicla-vision/getting-started/)

## 3. Check the ESP32 link before driving

Upload the [Wi-Fi sketch](https://github.com/yashsharma2512/esp32-maze-solver/tree/main/wifi/MAZESOLVER_WIFI).
At the startup prompt, send `camera` over USB Serial at 115200 baud
with Newline selected, or from the Wi-Fi browser console.

| Output | What to check |
| --- | --- |
| `Camera: linked, no target.` | The UART works. Present a marker and send `camera` again. |
| `Camera: HARMED`, `STABLE`, or `UNHARMED` | Check the label and the reported x/y position against the preview. |
| `Camera: no recent Nicla packet.` | Check the script is running, then check SDA to GPIO16 and the shared ground. |

When running the maze, the original victim approach and five NeoPixel
flashes use red for Harmed, green for Stable, and amber for Unharmed.
The dropper servo remains disabled. To test normal maze movement with
victim actions disabled, set `CAMERA_VICTIM_ACTIONS` to `false` in the
[ESP32 config](https://github.com/yashsharma2512/esp32-maze-solver/blob/main/wifi/MAZESOLVER_WIFI/config/config_esp32_dev.h).
