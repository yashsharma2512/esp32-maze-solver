import time

try:
    from pyb import LED
    for color in (1, 2, 3):
        LED(color).on()
except (ImportError, OSError, ValueError):
    # Newer OpenMV builds expose LEDs through machine instead.
    try:
        from machine import LED
        for color in ("LED_RED", "LED_GREEN", "LED_BLUE"):
            LED(color).on()
    except (ImportError, OSError, ValueError):
        pass

try:
    import csi
except ImportError:
    csi = None

def open_camera():
    if csi and hasattr(csi, "CSI"):
        try:
            camera = csi.CSI()
            camera.reset()
            camera.framesize(csi.QVGA)
            camera.pixformat(csi.RGB565)
            camera.vflip(True)
            camera.hmirror(True)
            camera.snapshot(time=2000)
            print("Nicla camera: CSI ready")
            return camera
        except (OSError, RuntimeError, ValueError, AttributeError, TypeError) as error:
            print("Nicla camera: CSI failed, trying sensor:", error)
    import sensor
    camera = sensor
    camera.reset()
    camera.set_framesize(sensor.QVGA)
    camera.set_pixformat(sensor.RGB565)
    camera.set_vflip(True)
    camera.set_hmirror(True)
    camera.skip_frames(time=2000)
    print("Nicla camera: sensor ready")
    return camera



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

    def area(x0, y0, x1, y1):
        roi = (x + int(w * x0), y + int(h * y0),
               max(1, int(w * (x1 - x0))), max(1, int(h * (y1 - y0))))
        return density(img, roi)

    # The binary mask has white ink. Reject solid blocks and sparse noise.
    if not 0.10 <= area(0, 0, 1, 1) <= 0.72:
        return None
    top = area(.40, .02, .60, .22)
    middle = area(.40, .40, .60, .60)
    bottom = area(.40, .78, .60, .95)
    left = area(.08, .40, .38, .60)
    right = area(.62, .40, .92, .60)
    foot_left = area(.02, .90, .35, 1)
    foot_right = area(.65, .90, .98, 1)
    foot_center = area(.40, .90, .60, 1)

    # Omega has separated feet. An ordinary O closes across the bottom.
    if (middle < .22 and foot_left > .60 and foot_right > .60 and
            foot_center < .35 and left > .12 and right > .12):
        return "OMEGA"
    # Phi has a center stem through an oval.
    if (top > .38 and middle > .38 and bottom > .38 and
            .10 < left < .42 and .10 < right < .42 and
            min(left, right) < .34):
        return "PHI"
    # Psi has a center stem and empty lower corners.
    if (top > .30 and middle > .38 and bottom > .30 and
            left > .34 and right > .34 and
            foot_left < .18 and foot_right < .18):
        return "PSI"
    return None

# Blob thresholds and scoring from ring.py.
RED_THRESHOLD = (15, 100, 15, 127, -128, 127)
GREEN_THRESHOLD = (20, 100, -100, -10, -10, 100)
BLUE_THRESHOLD = (10, 100, -20, 50, -100, -10)
YELLOW_THRESHOLD = (30, 100, -30, 40, 20, 100)
BLACK_THRESHOLD = (0, 26, -128, 127, -128, 127)

COLOR_THRESHOLDS = [
    RED_THRESHOLD,
    GREEN_THRESHOLD,
    BLUE_THRESHOLD,
    YELLOW_THRESHOLD,
    BLACK_THRESHOLD,
]
RING_VALUES = {"red": -1, "green": 1, "blue": 2, "black": -2, "none": 0}
VICTIM_NAMES = {2: "HARMED", 1: "STABLE", 0: "UNHARMED"}

RING_SEARCH_DIVISOR = 2
RING_SEARCH_SCALE = 1 / RING_SEARCH_DIVISOR
LOG_INTERVAL_MS = 1000
LETTER_CODES = {"PHI": "P", "PSI": "S", "OMEGA": "O"}


# Keep the existing ring IDs and add the three Greek letter IDs.
RING_IDS = {2: 2, 1: 0, 0: 1}
LETTER_IDS = {"PHI": 3, "PSI": 4, "OMEGA": 5}
NO_RING = -1
NO_LETTER = -2


def open_camera_uart():
    # SNS wiring: ESLOV SDA (UART4 TX) -> LOLIN GPIO12 (UART1 RX).
    try:
        from machine import UART
        uart = UART(4, 115200)
        print("Nicla SNS UART4 TX ready on ESLOV SDA")
        return uart
    except (ImportError, OSError, RuntimeError, TypeError, ValueError) as error:
        print("Nicla SNS UART4 unavailable:", error)
        return None


class TargetReporter:
    def __init__(self, clear_id):
        self.clear_id = clear_id
        self.previous_type = clear_id
        self.changed_at = time.ticks_ms()

    def packet(self, target, now):
        if target is None:
            target_type, x, y = self.clear_id, -1, -1
        else:
            target_type, x, y = target
            x //= RING_SEARCH_DIVISOR
            y //= RING_SEARCH_DIVISOR
        if target_type != self.previous_type:
            self.previous_type = target_type
            self.changed_at = now
        age = time.ticks_diff(now, self.changed_at) if target is not None else 0
        return "%d,%d,%d,%d\n" % (target_type, age, x, y)


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

    total = max(1, r + g + b)
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
    colors = []
    for px in points:
        samples = [ring_color(img, px, yy) for yy in
                   (cy - max(2, h // 12), cy, cy + max(2, h // 12))]
        if samples[0] == samples[1] or samples[0] == samples[2]:
            colors.append(samples[0])
        elif samples[1] == samples[2]:
            colors.append(samples[1])
        else:
            colors.append(samples[1])
    return colors, points, cy


def classify_ring(colors):
    # Require multiple colored bands before allowing a candidate to mask PSO.
    if sum(color in ("red", "green", "blue") for color in colors) < 2:
        return None
    if len(set(colors)) < 2:
        return None
    score = sum(RING_VALUES[color] for color in colors)
    return score if score in VICTIM_NAMES else None


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
        if not 0.75 <= w / h <= 1.25:
            continue
        reading = read_ring_bands(search, rect)
        if classify_ring(reading[0]) is None:
            continue
        preview_rect = tuple(value * RING_SEARCH_DIVISOR for value in rect)
        rings.append((preview_rect, reading))
    return rings


def detect_letters(img, preview):
    border = 35
    detected = []
    blobs = img.find_blobs([(255, 255)], pixels_threshold=100, area_threshold=100, merge=False)
    for blob in blobs:
        x, y, w, h = field(blob.rect)
        if not (35 <= w <= 200 and 35 <= h <= 200 and 0.65 <= w / h <= 1.45):
            continue

        cx, cy = field(blob.cx), field(blob.cy)
        if not (border <= cx <= img.width() - border and
                border <= cy <= img.height() - border):
            continue

        letter = classify_letter(img, blob)
        if letter is None:
            continue
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

    primary = None
    if detected:
        _, _, _, _, cx, cy, letter = max(detected, key=lambda target: target[2] * target[3])
        primary = LETTER_IDS[letter], cx, cy
    return result, primary


uart = open_camera_uart()
if uart is None:
    print("Nicla: no UART opened; image preview will continue")
sent_bytes = 0
camera = open_camera()
clock = time.clock()
tof = open_range_sensor()
print("COGNITIVE: sampling from measured blob width")
ring_search = camera.snapshot().copy(x_scale=RING_SEARCH_SCALE, y_scale=RING_SEARCH_SCALE)
gray_buffer = None
last_log = time.ticks_ms()

ring_reporter = TargetReporter(NO_RING)
letter_reporter = TargetReporter(NO_LETTER)

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
    result, primary_letter = detect_letters(img, preview)
    primary_ring = None
    largest_ring_area = 0

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
        w, h = rect[2:]
        if score in RING_IDS and w * h > largest_ring_area:
            largest_ring_area = w * h
            primary_ring = RING_IDS[score], x + w // 2, y + h // 2
        preview.draw_rectangle(rect, thickness=2)
        preview.draw_string((x, max(0, y - 18)), status, scale=2)

    if report:
        tof_status = distance if distance is not None else "NO VALID RANGE"
        print("RESULT:", result, "| FPS:", clock.fps(), "| ToF mm:", tof_status)
        last_log = now

    # Send the finished frame's results after drawing and OpenMV diagnostics.
    if uart is not None:
        packets = ring_reporter.packet(primary_ring, now) + letter_reporter.packet(primary_letter, now)
        try:
            written = uart.write(packets.encode())
            if written:
                sent_bytes += written
            if report:
                print("Nicla SNS UART4 TX bytes:", sent_bytes)
        except (OSError, RuntimeError) as error:
            print("Nicla SNS UART4 write failed:", error)
            uart = None
