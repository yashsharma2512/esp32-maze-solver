# Nicla Vision for PSO and cognitive targets

The [OpenMV script](main.py) recognizes Φ/Ψ/Ω letters and cognitive rings on
the Nicla. It uses the width of each detected ring in the camera image to
place the five sample points. The ESP32 receives classifications and positions
over UART1.

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

Upload your ESP32 sketch and open its console. The USB, BLE and Wi-Fi
versions show confirmed detections while waiting to start and during a run.
In the [Wi-Fi sketch](../../wifi/MAZESOLVER_WIFI/README.md), you can also send
`camera` at startup over USB Serial at 115200 baud with Newline selected,
or from the browser console.

| Output | What to check |
| --- | --- |
| `Camera ring: linked, no target.` or `Camera PSO: linked, no target.` | The UART works. Present the corresponding target and send `camera` again. |
| `COGNITIVE: HARMED`, `STABLE`, or `UNHARMED` | Check the color classification and the five sample markers in the preview. |
| `PSO: PHI`, `PSI`, or `OMEGA` | Check the letter in the preview. |
| `Camera: no recent Nicla packet.` | Check the script is running, then check J2 pin 3 to GPIO16 and the shared ground. |

Each frame sends one CSV line per detector with the type, milliseconds the
type has been visible, and its x/y center in half-resolution coordinates.
Type IDs `0`, `1`, `2` mean Stable, Unharmed, Harmed. IDs `3`, `4`, `5` mean
Phi, Psi, Omega. IDs `-1` and `-2` clear the ring and letter results. The
ESP32 confirms a detection after three matching frames. Detections are
console reports only.
