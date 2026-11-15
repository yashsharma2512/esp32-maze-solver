# Nicla Vision for PSO and cognitive targets

Run [main.py](main.py) in OpenMV to test Φ/Ψ/Ω letters and cognitive rings with
the working camera script. It places the five sample points using the detected
ring width. The ToF reading is printed for debugging. Run
[robot_uart.py](robot_uart.py) when testing the ESP32 UART link.

## 1. Wire the camera

Power both boards over USB for the first test. The UART script uses the Nicla's
external UART at 115200 baud. In OpenMV it opens as `machine.UART(9, 115200)`.
[Arduino Nicla Vision UART guide](https://docs.arduino.cc/tutorials/nicla-vision/user-manual/#uart)

| Nicla Vision | ESP32 Dev Module |
| --- | --- |
| **D1 / UART_TX** (J2 pin 3) | **GPIO35** (UART1 RX) |
| **GND** (J2 pin 6) | **GND** |

Only the Nicla transmits, so leave its D2 / UART_RX (J2 pin 4) unconnected.
[Arduino Nicla Vision pinout](https://docs.arduino.cc/resources/pinouts/ABX00051-full-pinout.pdf)

## 2. Run the camera by itself

Open `main.py` in OpenMV IDE, connect the Nicla, and press Run. Place a
victim marker in front of the camera and check the label in the preview.
This test displays results in OpenMV.
[Getting started with OpenMV on Nicla Vision](https://docs.arduino.cc/tutorials/nicla-vision/getting-started/)

## 3. Check the ESP32 link before driving

Run `robot_uart.py` in OpenMV for the robot link. It prints a message when
UART initializes or fails. To boot into that mode, save `robot_uart.py` as
`main.py` on the Nicla's flash.

Upload your ESP32 sketch and open its console. The USB, BLE and Wi-Fi versions
show confirmed detections while waiting to start and during a run.
In the [Wi-Fi sketch](../../wifi/MAZESOLVER_WIFI/README.md), you can also send
`camera` at startup over USB Serial at 115200 baud with Newline selected,
or from the browser console.

| Output | What to check |
| --- | --- |
| `Camera UART: ... bytes received, ... packets accepted since boot.` | A running Nicla should make both counts rise. Check D1/TX, GPIO35 and ground if the byte count stays flat. If only the byte count rises, check the baud rate and packet format. |
| `Camera ring: linked, no target.` or `Camera PSO: linked, no target.` | The UART works. Present the corresponding target and send `camera` again. |
| `COGNITIVE: HARMED`, `STABLE`, or `UNHARMED` | Check the color classification and the five sample markers in the preview. |
| `PSO: PHI`, `PSI`, or `OMEGA` | Check the letter in the preview. |
| `Camera: no recent Nicla packet.` | Check the script is running, then check J2 pin 3 to GPIO35 and the shared ground. |

Each frame sends one CSV line per detector with the type, milliseconds the
type has been visible, and its x/y center in half-resolution coordinates.
Type IDs `0`, `1`, `2` mean Stable, Unharmed, Harmed. IDs `3`, `4`, `5` mean
Phi, Psi, Omega. IDs `-1` and `-2` clear the ring and letter results. The
ESP32 confirms a detection after three matching frames. Detections are
console reports only.
