"""Print the position from a SparkFun NEO-F10N GNSS breakout over USB-C.

    uv run tools/gnss_check.py [port]      # default /dev/ttyUSB0, 38400 baud
"""
import sys
import serial

def to_degrees(value, hemi):
    # convert NMEA ddmm.mmmm / dddmm.mmmm to signed decimal degrees
    dot = value.index(".")

    # deg = dd + (mm.mmmm / 60.0)
    deg = float(value[:dot - 2]) + float(value[dot - 2:]) / 60.0

    # south/west is negative, north/east is positive
    return -deg if hemi in ("S", "W") else deg

# defaults to /dev/ttyUSB0, provide port as argument if any other port used
port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"

with serial.Serial(port, 38400, timeout=1) as ser:
    while True:
        f = ser.readline().decode("ascii", "replace").strip().split(",")

        # the list of substrings in f is in the wrong order
        if not f[0].endswith("GGA") or len(f) < 10:
            continue

        # we have no position yet
        if f[6] in ("", "0"):
            print(f"no fix yet ({f[7] or 0} satellites)")
            continue

        # calculate latitude and longitude
        lat, lon = to_degrees(f[2], f[3]), to_degrees(f[4], f[5])
        print(f"lat. {lat:.7f}  lon. {lon:.7f}  alt. {f[9]}m  ({f[7]} satellites)")
