# SNS maze Nicla Vision

This Nicla script is specific to the SNS maze wiring. The WiFi sketch's
`vision/NiclaVision/robot_uart.py` remains separate.

Open `main.py` in OpenMV IDE, connect the Nicla Vision, and save it to the
Nicla as `main.py`. Reset the Nicla. OpenMV's Serial Terminal should print
`Nicla SNS UART4 TX ready on ESLOV SDA` and then a rising
`Nicla SNS UART4 TX bytes` count. The script sends two CSV lines per frame,
including clear packets when it sees no target.

| Nicla ESLOV pin | LOLIN S2 Mini pin |
| --- | --- |
| J5-4 SDA_EXT (UART4 TX) | GPIO12 (`NICLA_RX_PIN`) |
| J5-5 GND | GND |

ESLOV SCL_EXT and INT are unused for this one-way UART link. Power the Nicla
appropriately. The LOLIN's GPIO16/17 are the TCA9548A I²C bus and are not
the camera UART input. The SNS LOLIN sketch begins reading UART only after
the second hand gesture and prints `NICLA: UART1 RX on GPIO12 active.`

If OpenMV reports that UART4 is unavailable, this ESLOV script cannot send
packets with that firmware. Current OpenMV Nicla documentation maps UART4 TX
to SDA; Arduino's older Nicla guide instead shows UART9 on D1, which is a
different physical pin. Check the firmware's UART mapping before changing
wiring.

The script validates colored ring bands before masking them from the Greek
letter detector. It rejects unrecognized letter shapes rather than assigning
them a target, and averages three scan rows for ring colors. The LOLIN still
requires three matching recent packets before acting.

On a development PC, install Pillow and run
`python3 tests/test_simulated_targets.py` to check simulated Φ/Ψ/Ω glyphs,
non-target glyphs, ring patterns, UART selection, and packet formatting.
These tests do not verify the camera optics or physical wiring.
