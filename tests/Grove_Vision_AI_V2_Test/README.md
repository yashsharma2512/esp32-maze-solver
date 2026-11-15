# Grove Vision AI V2 Greek victim test

This separate ESP32 DevKit sketch reads a **trained model already deployed on
the Grove Vision AI V2**. It recognizes the model's labels `phi`/`harmed`,
`psi`/`stable`, `omega`/`unharmed`, and `nothing`/`none`/`background`. It prints
the numeric ID and score of every class or box, then confirms a Greek target
after three successive results with a score of at least 75/100. The TB6612
motor driver stays in standby throughout the test.

## Wiring

Use the V2 module's **XIAO-compatible UART pads**, labelled `D6/TX` and
`D7/RX`. The four-pin Grove socket on the Vision V2 is the I2C connection;
do not connect its I2C signal pins to GPIO16/17. Its `VCC` and `GND` pins
can provide the power connection below.

| Grove Vision AI V2 | ESP32-WROOM-32 DevKit | Purpose |
| --- | --- | --- |
| `D6/TX` | GPIO34 (`UART2 RX`) | Vision result data to ESP32 |
| `D7/RX` | GPIO14 (`UART2 TX`) | ESP32 commands to Vision V2 |
| Grove `VCC` / 5 V input | Regulated 5 V rail | Module power |
| `GND` | GND | Shared ground |

```text
Grove Vision AI V2                    ESP32 DevKit / robot
  XIAO D6/TX  ----------------------> GPIO34 / Serial2 RX
  XIAO D7/RX  <---------------------- GPIO14 / Serial2 TX
  Grove VCC   <---------------------- regulated 5 V
  Grove GND   ----------------------- GND
  CSI camera  <---- camera ribbon
```

The UART signals on the XIAO pads are 3.3 V logic; the module's Grove power
input is 5 V. Use a stable regulated 5 V supply shared with the robot's logic
ground. The Nicla Vision stays on GPIO16/17. GPIO14 must be free of the
optional wall-select switch. The TCA9548A and its APDS/AS5600/AS7341 channels
do not change.

## Model and test

The previous dataset is at
`/home/yash/Documents/Codex/2026-11-13/di/outputs/robocup_victims_harmed_stable_unharmed_nothing_240px.zip`.
It has 300 synthetic images each of Φ, Ψ, Ω, and `nothing`. It is **not a
trained model**. Add photos captured by the actual Grove camera under maze
lighting before relying on recognition. Train a four-class classification
model in SenseCraft and deploy the resulting Grove V2-compatible model to
the module. Keep the class names `harmed`, `stable`, `unharmed`, `nothing`
(or `phi`, `psi`, `omega`, `nothing`). The sketch reads the class names from
the deployed model, so it does not assume numeric IDs.

Install **Seeed Arduino SSCMA** and **ArduinoJson** in Arduino IDE. Open
`Grove_Vision_AI_V2_Test.ino`, select **ESP32 Dev Module**, and upload by USB.
Open Serial Monitor at **115200 baud** with motor power off. The sketch first
prints the model's class ID table. Show each letter to the camera and check
the class, score, and `confirmed=3/3` output. An `UNMAPPED CLASS` means the
model has a different label; change `targetForId()` to match that label.

See [Seeed's Grove Vision AI V2 Arduino guide](https://wiki.seeedstudio.com/grove_vision_ai_v2a/),
[SSCMA library](https://github.com/Seeed-Studio/Seeed_Arduino_SSCMA), and
[custom-model deployment guide](https://wiki.seeedstudio.com/grove_vision_ai_v2_sscma/).
