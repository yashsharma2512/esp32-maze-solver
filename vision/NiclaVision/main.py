"""
Victim color classification for OpenMV / Nicla Vision.

Finds square-ish colored patches (blobs) in the camera frame, samples five
points across the patch's diameter, classifies each sample point's color,
and sums scores across the five points to decide a "victim" status:
    +2  -> Harmed (send 2 to the ESP32)
    +1  -> Stable (send 0)
     0  -> Unharmed (send 1)
  other -> no target (send -1)

A ToF sensor (VL53L1X) provides distance, which is used together with the
camera FOV to estimate the real-world width of a fixed-size (4 cm) patch
in pixels, for laying out the five sample points.
"""

import sensor
import time
import math
from machine import I2C
import vl53l1x
from pyb import LED, UART

red_led = LED(1)
green_led = LED(2)
blue_led = LED(3)

red_led.off()
green_led.off()
blue_led.off()

red_led.on()
time.sleep(0.2)
green_led.on()
time.sleep(0.2)
blue_led.on()
time.sleep(0.2)

red_led.off()
green_led.off()
blue_led.off()

# UART4 uses the Nicla pads labeled SDA (TX) and SCL (RX).
uart = UART(4, 115200)

# --------------------------------------------------------------------------
# Distance sensor (ToF)
# --------------------------------------------------------------------------
bus = I2C(2)  # internal bus (PMIC / fuel gauge / ToF)
tof = vl53l1x.VL53L1X(bus)

# --------------------------------------------------------------------------
# Camera / geometry configuration
# --------------------------------------------------------------------------
FOV = 35.0                 # camera field of view, degrees
PATCH_WIDTH_MM = 40        # real-world width of the target color patch
IMAGE_WIDTH = 160          # camera frame width, px

# --------------------------------------------------------------------------
# Blob-detection color thresholds (LAB), used by find_blobs()
# --------------------------------------------------------------------------
RED_THRESHOLD = (15, 100, 15, 127, -128, 127)
GREEN_THRESHOLD = (20, 100, -100, -10, -10, 100)
BLUE_THRESHOLD = (10, 100, -20, 50, -100, -10)
YELLOW_THRESHOLD = (30, 100, -30, 40, 20, 100)
BLACK_THRESHOLD = (0, 26, 127, -128, 127, -128)

COLOR_THRESHOLDS = [
    RED_THRESHOLD,
    GREEN_THRESHOLD,
    # BLUE_THRESHOLD,
    YELLOW_THRESHOLD,
    BLACK_THRESHOLD
]

VICTIM_CODES = {2: 2, 1: 0, 0: 1}
VICTIM_NAMES = {2: "Harmed", 0: "Stable", 1: "Unharmed"}
previous_type = -1
last_changed = time.ticks_ms()

# --------------------------------------------------------------------------
# Camera setup
# --------------------------------------------------------------------------
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.B160X160)
sensor.skip_frames(time=2000)

# --------------------------------------------------------------------------
# Main loop
# --------------------------------------------------------------------------
while True:
    img = sensor.snapshot()
    try:
        dist = tof.read()
    except OSError:
        dist = None
    if dist is None or dist <= 0:
        previous_type = -1
        uart.write("-1,0,-1,-1\n")
        continue

    # Margin (px) kept clear on all sides of the frame when searching for blobs
    clip_window = 10

    blobs = img.find_blobs(
        COLOR_THRESHOLDS,
        roi=(
            clip_window + 1,
            clip_window + 1,
            img.width() - clip_window * 2 - 2,
            img.height() - clip_window * 2 - 2,
        ),
        x_stride=2,
        y_stride=2,
        area_threshold=100,
        pixels_threshold=80,
        merge=True,
        margin=2,
    )

    victim_type = -1
    victim_x = -1
    victim_y = -1
    for blob in sorted(blobs, key=lambda blob: blob.pixels(), reverse=True):
        # ------------------------------------------------------------
        # Basic geometry / rejection filters
        # ------------------------------------------------------------
        w = blob.w()
        h = blob.h()
        bx = blob.x()
        by = blob.y()
        cx = bx + w // 2
        cy = by + h // 2

        # Estimate pixel width of a 4 cm real-world patch at this distance
        width_px = (IMAGE_WIDTH * PATCH_WIDTH_MM / (2.0 * dist * math.tan(math.radians(FOV / 2.0))))

        # Reject blobs touching the ROI edges (likely clipped)
        if blob.x() <= 0 or blob.y() <= 0:
            continue
        if blob.x() + w >= img.width() - clip_window - 1:
            continue
        if blob.y() + h >= img.height() - clip_window - 1:
            continue

        # Reject non-square blobs
        ratio = w / h
        if ratio < 0.85 or ratio > 1.15:
            continue

        # ------------------------------------------------------------
        # Lay out 5 sample points across the patch, from center outward
        # ------------------------------------------------------------
        x1 = int(cx)
        x2 = int(cx - width_px * 1.32 / 10)
        x3 = int(cx - width_px * 2.32 / 10)
        x4 = int(cx - width_px * 3.32 / 10)
        x5 = int(cx - width_px * 4.32 / 10)

        points = (x5, x4, x3, x2, x1)
        y = cy
        if x5 < 1 or x1 >= img.width() - 1 or y < 1 or y >= img.height() - 1:
            continue

        # ------------------------------------------------------------
        # Classify the color at each sample point and accumulate score
        # ------------------------------------------------------------
        victim = 0

        for i, xx in enumerate(points):
            # Average a 3x3 pixel neighborhood around the sample point
            r = 0
            g = 0
            b = 0
            for y_scan in range(y - 1, y + 2):
                for x_scan in range(xx - 1, xx + 2):
                    pr, pg, pb = img.get_pixel(x_scan, y_scan)
                    r += pr
                    g += pg
                    b += pb
            r //= 9
            g //= 9
            b //= 9

            # Normalize RGB to percentages of their sum, for color comparison
            _sum = 1
            if r > 0 and g > 0 and b > 0:
                _sum = r + g + b

            r_scaled = int((r / _sum) * 100)
            g_scaled = int((g / _sum) * 100)
            b_scaled = int((b / _sum) * 100)

            # Draw the sampled color at this point (mul boosts brightness for visibility)
            mul = 1.5
            img.draw_circle(
                (xx, int(y), 2),
                color=(min(int(r * mul), 255), min(int(g * mul), 255), min(int(b * mul), 255)),
                fill=True,
            )
            img.draw_circle((xx, int(y), 2), color=(0, 0, 0), fill=False)

            _size = 10
            max_val = max(r_scaled, g_scaled, b_scaled)

            isGreen = (g_scaled - r_scaled >= 5) and (g_scaled > b_scaled)
            isRed = ((r_scaled - (g_scaled + b_scaled) / 2) > 15) and (abs(g_scaled - b_scaled) < 15)
            isBlue = ((b_scaled - (g_scaled + r_scaled) / 2) > 5)
            isBlack = (_sum < 230) and (max_val - min(r_scaled, g_scaled, b_scaled) < 10)

            if isRed:
                victim -= 1
                img.draw_rectangle((int(i * _size), 0, _size, _size), color=(180, 0, 0), fill=True)
            elif isGreen:
                victim += 1
                img.draw_rectangle((int(i * _size), 0, _size, _size), color=(0, 180, 0), fill=True)
            elif isBlue:
                victim += 2
                img.draw_rectangle((int(i * _size), 0, _size, _size), color=(100, 100, 255), fill=True)
            elif isBlack:
                victim -= 2
                img.draw_rectangle((int(i * _size), 0, _size, _size), color=(0, 0, 0), fill=True)
            else:
                img.draw_rectangle((int(i * _size), 0, _size, _size), color=(200, 200, 0), fill=True)

        if victim not in VICTIM_CODES:
            continue
        victim_type = VICTIM_CODES[victim]
        victim_x, victim_y = x1, y
        img.draw_circle((x1, y, int(width_px / 2)), color=(255, 255, 255), thickness=1)
        break

    # Send exactly one packet per frame. A blank frame clears the last target.
    now = time.ticks_ms()
    changed = victim_type != previous_type
    if changed:
        previous_type = victim_type
        last_changed = now
    elapsed = time.ticks_diff(now, last_changed) if victim_type != -1 else 0
    data = "%d,%d,%d,%d" % (victim_type, elapsed, victim_x, victim_y)
    uart.write(data + "\n")
    if changed:
        print(data)
    img.draw_string((12, 12), VICTIM_NAMES.get(victim_type, "No target"), color=(255, 255, 255))
