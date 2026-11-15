# Nicla Vision reference and cognitive-target UART

The [OpenMV script](main.py) uses the existing ring color classifier and
the onboard distance sensor to recognize Harmed, Stable, and Unharmed
markers. The ESP32 receives the classification and position over UART.

## 1. Wire the camera

Power both boards over USB for the first test. The script uses the Nicla's
LP1 UART at 115200 baud.

| Nicla Vision | ESP32 Dev Module |
| --- | --- |
| **D1 / UART_TX** (J2 pin 3) | **GPIO16** (UART1 RX) |
| **GND** (J2 pin 6) | **GND** |

Only the Nicla transmits, so leave its D2 / UART_RX (J2 pin 4) unconnected.
[Arduino Nicla Vision pinout](https://docs.arduino.cc/resources/pinouts/ABX00051-full-pinout.pdf)

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
| `Camera: no recent Nicla packet.` | Check the script is running, then check J2 pin 3 to GPIO16 and the shared ground. |

The existing `main.py` is an older H/S/U victim example. In the current robot
sketches, Nicla stays connected on UART1 but its maze victim actions are
disabled while the cognitive-target code is being prepared. Grove Vision AI V2
reports the Greek-letter targets over the separate UART2.

The older H/S/U approach routine remains in source as a reference but is not
called during maze runs. The dropper servo remains disabled.
