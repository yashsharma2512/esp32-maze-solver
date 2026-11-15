import time
from pyb import LED

try:
    import csi
except ImportError:
    csi = None

red_led = LED(1)
green_led = LED(2)
blue_led = LED(3)

red_led.on()
green_led.on()
blue_led.on()

if csi and hasattr(csi, "CSI"):
    camera = csi.CSI()
    camera.reset()
    camera.framesize(csi.QVGA)
    camera.pixformat(csi.RGB565)
    camera.vflip(True)
    camera.hmirror(True)
    camera.snapshot(time=2000)
else:
    import sensor

    camera = sensor
    camera.reset()
    camera.set_framesize(sensor.QVGA)
    camera.set_pixformat(sensor.RGB565)
    camera.set_vflip(True)
    camera.set_hmirror(True)
    camera.skip_frames(time=2000)

clock = time.clock()


# =================================================
# WHITE DENSITY
# =================================================

def field(value):
    # OpenMV versions expose blob and statistics fields as methods or attributes.
    return value() if callable(value) else value


def density(img, roi):

    stats = img.get_statistics(roi=roi)

    return field(stats.mean) / 255.0


# =================================================
# CLASSIFY LETTER
# =================================================

def classify_letter(img, blob):

    x, y, w, h = field(blob.rect)

    # -------------------------------------------------
    # OMEGA TEST
    # -------------------------------------------------
    # Bottom-center should be OPEN for Omega.
    # Phi/Psi have their vertical stem here.
    # -------------------------------------------------

    bottom_roi = (
        x + (w * 40) // 100,
        y + (h * 75) // 100,
        (w * 20) // 100,
        (h * 15) // 100
    )

    bottom = density(img, bottom_roi)

    if bottom < 0.20:
        return "OMEGA", bottom, 0, 0

    # -------------------------------------------------
    # PSI TEST
    # -------------------------------------------------
    # Look at left and right sides of the middle.
    #
    # Psi:
    #    left side  = WHITE
    #    right side = WHITE
    #
    # Phi:
    #    left side  = mostly BLACK
    #    right side = mostly BLACK
    # -------------------------------------------------

    band_y = y + (h * 45) // 100
    band_h = (h * 15) // 100

    left_roi = (
        x + (w * 10) // 100,
        band_y,
        (w * 30) // 100,
        band_h
    )

    right_roi = (
        x + (w * 60) // 100,
        band_y,
        (w * 30) // 100,
        band_h
    )

    left = density(img, left_roi)
    right = density(img, right_roi)

    # Both sides must contain substantial white
    if left > 0.25 and right > 0.25:
        return "PSI", bottom, left, right

    return "PHI", bottom, left, right

# Blob thresholds and scoring from ring.py.
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
    BLACK_THRESHOLD,
]
RING_VALUES = {"red": -1, "green": 1, "blue": 2, "black": -2, "none": 0}
VICTIM_NAMES = {2: "HARMED", 1: "STABLE", 0: "UNHARMED"}

RING_SEARCH_DIVISOR = 2
RING_SEARCH_SCALE = 1 / RING_SEARCH_DIVISOR
LOG_INTERVAL_MS = 1000
LETTER_CODES = {"PHI": "P", "PSI": "S", "OMEGA": "O"}


def open_range_sensor():
    try:
        from machine import I2C
        from vl53l1x import VL53L1X

        return VL53L1X(I2C(2))
    except (ImportError, OSError, RuntimeError) as error:
        print("COGNITIVE: ToF unavailable", error)
        return None


def read_ring_distance(tof):
    if tof is None:
        return None
    try:
        distance = tof.read()
    except OSError:
        return None
    if distance is None or distance <= 0:
        return None
    return distance


def ring_color(img, x, y):
    # Average the same 3×3 section and use the RGB percentages from ring.py.
    r = g = b = 0
    for yy in range(y - 1, y + 2):
        for xx in range(x - 1, x + 2):
            pixel = img.get_pixel((xx, yy))
            if pixel and len(pixel) >= 3:
                r += pixel[0]
                g += pixel[1]
                b += pixel[2]
    r //= 9
    g //= 9
    b //= 9

    total = r + g + b if r > 0 and g > 0 and b > 0 else 1
    r_scaled = int((r / total) * 100)
    g_scaled = int((g / total) * 100)
    b_scaled = int((b / total) * 100)

    if (r_scaled - (g_scaled + b_scaled) / 2) > 15 and abs(g_scaled - b_scaled) < 15:
        return "red"
    if (g_scaled - r_scaled >= 5) and (g_scaled > b_scaled):
        return "green"
    if (b_scaled - (g_scaled + r_scaled) / 2) > 5:
        return "blue"
    if total < 230 and max(r_scaled, g_scaled, b_scaled) - min(r_scaled, g_scaled, b_scaled) < 10:
        return "black"
    return "none"


def read_ring_bands(img, rect):
    x, y, width_px, h = rect
    cx = x + width_px // 2
    cy = y + h // 2
    # Use the detected ring width with the five sample offsets from ring.py.
    points = (
        int(cx - width_px * 4.32 / 10),
        int(cx - width_px * 3.32 / 10),
        int(cx - width_px * 2.32 / 10),
        int(cx - width_px * 1.32 / 10),
        cx,
    )
    colors = [ring_color(img, px, cy) for px in points]
    return colors, points, cy


def find_ring_candidates(rgb, search):
    # The combined sketch keeps its QVGA preview. Run ring.py's search and
    # five samples on the 160-pixel-wide image, then scale boxes for the preview.
    search.draw_image(rgb, 0, 0, x_scale=RING_SEARCH_SCALE, y_scale=RING_SEARCH_SCALE)
    clip_window = 10
    blobs = search.find_blobs(
        COLOR_THRESHOLDS,
        roi=(clip_window + 1, clip_window + 1,
             search.width() - clip_window * 2 - 2,
             search.height() - clip_window * 2 - 2),
        x_stride=2,
        y_stride=2,
        area_threshold=100,
        pixels_threshold=80,
        merge=True,
        margin=2,
    )
    rings = []
    for blob in blobs:
        rect = field(blob.rect)
        x, y, w, h = rect
        if x <= 0 or y <= 0:
            continue
        if x + w >= search.width() - clip_window - 1:
            continue
        if y + h >= search.height() - clip_window - 1:
            continue
        if not 0.85 <= w / h <= 1.15:
            continue
        reading = read_ring_bands(search, rect)
        preview_rect = tuple(value * RING_SEARCH_DIVISOR for value in rect)
        rings.append((preview_rect, reading))
    return rings


def detect_letters(img, preview):
    border = 50
    detected = []
    blobs = img.find_blobs([(255, 255)], pixels_threshold=100, area_threshold=100, merge=False)
    for blob in blobs:
        x, y, w, h = field(blob.rect)
        if not (50 <= w <= 200 and 50 <= h <= 200 and 0.7 <= w / h <= 1.4):
            continue

        cx, cy = field(blob.cx), field(blob.cy)
        if not (border <= cx <= img.width() - border and
                border <= cy <= img.height() - border):
            continue

        letter, _, _, _ = classify_letter(img, blob)
        detected.append((x, y, w, h, cx, cy, letter))

    # Draw after classification so the guide and labels cannot alter a reading.
    if preview is img:
        preview.draw_rectangle((border, border, img.width() - 2 * border,
                                img.height() - 2 * border), thickness=2)
    detected.sort(key=lambda letter: letter[0])
    result = ""
    for x, y, w, h, cx, cy, letter in detected:
        preview.draw_rectangle((x, y, w, h), thickness=2)
        preview.draw_circle((cx, cy, 4), fill=True)
        preview.draw_string((x, max(0, y - 18)), letter, scale=2)
        result += LETTER_CODES[letter]

    return result


tof = open_range_sensor()
print("COGNITIVE: sampling from measured blob width")
ring_search = camera.snapshot().copy(x_scale=RING_SEARCH_SCALE, y_scale=RING_SEARCH_SCALE)
gray_buffer = None
last_log = time.ticks_ms()

while True:
    clock.tick()
    now = time.ticks_ms()
    report = time.ticks_diff(now, last_log) >= LOG_INTERVAL_MS

    # Read ring colors first, while the frame is still RGB.
    rgb = camera.snapshot()
    rings = find_ring_candidates(rgb, ring_search)
    distance = read_ring_distance(tof) if report else None

    if rings:
        # Keep the live RGB preview and reuse a grayscale buffer for PSO.
        if gray_buffer is None:
            gray_buffer = rgb.to_grayscale(copy=True)
        else:
            gray_buffer.draw_image(rgb, 0, 0)
        img = gray_buffer
        preview = rgb
    else:
        # Show the PSO mask directly when there is no ring candidate.
        img = rgb.to_grayscale(copy=False)
        preview = img
    img.binary([(0, 70)])

    # In the binary image, white pixels are PSO candidates. Black out ring
    # regions before looking for letters.
    for rect, _ in rings:
        img.draw_rectangle(rect, color=0, fill=True)
    result = detect_letters(img, preview)

    for rect, reading in rings:
        colors, points, sample_y = reading
        score = sum(RING_VALUES[color] for color in colors)
        status = VICTIM_NAMES.get(score, "FAKE")
        if report:
            print("COGNITIVE:", status, "| colors edge to center:", ", ".join(colors),
                  "| sum:", score, "| sample width from blob px:", rect[2] // RING_SEARCH_DIVISOR)

        # Mark the sample positions after all detection is done.
        cy = sample_y * RING_SEARCH_DIVISOR
        for px in points:
            cx = px * RING_SEARCH_DIVISOR
            preview.draw_circle((cx, cy, 4), color=(0, 0, 0), thickness=2)
            preview.draw_circle((cx, cy, 3), color=(255, 255, 255), thickness=1)

        x, y = rect[:2]
        preview.draw_rectangle(rect, thickness=2)
        preview.draw_string((x, max(0, y - 18)), status, scale=2)

    if report:
        tof_status = distance if distance is not None else "NO VALID RANGE"
        print("RESULT:", result, "| FPS:", clock.fps(), "| ToF mm:", tof_status)
        last_log = now
