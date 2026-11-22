"""Live VL53L9 depth map from the NUCLEO-N657X0-Q over the ST-LINK VCP.

    uv run tools/vl53l9_viewer.py /dev/ttyACM0
    uv run tools/vl53l9_viewer.py /dev/ttyACM0 --record capture.bin
    uv run tools/vl53l9_viewer.py capture.bin            # replay a recording

Packet layout: see common/vl53l9_stream.h. Zones with no target are drawn black.
Text the board prints between packets is echoed to the terminal.

Keys: q quit, r cycle orientation, [ ] range -/+ 500 mm, s save PNG + .npy
"""
import argparse
import os
import stat
import termios
import time

import cv2
import numpy as np

MAGIC = b"\xa5\x5aV9"
HEADER_LEN = 28
VERSION = 5
MM_PER_LSB = 2                          # distance step of the packed values
MAX_PAYLOAD = (3 * 54 * 42 + 1) // 2    # binning 2 is the largest grid
ORIENTATIONS = ["raw", "flip vertical", "flip horizontal", "rotate 180"]
BAUDS = {115200: termios.B115200, 921600: termios.B921600, 2000000: termios.B2000000,
         4000000: termios.B4000000}


def open_port(path, baud):
    """Open the serial port raw at baud 8N1 with a 0.05 s read timeout."""
    fd = os.open(path, os.O_RDONLY | os.O_NOCTTY)
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0                                             # iflag
    attrs[1] = 0                                             # oflag
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL  # cflag
    attrs[3] = 0                                             # lflag
    attrs[4] = attrs[5] = BAUDS[baud]                        # ispeed, ospeed
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0                              # non-blocking, the GUI loop polls
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIFLUSH)
    return fd


def payload_len(zones):
    """Bytes of packed 12-bit distances for this many zones."""
    return (3 * zones + 1) // 2


def unpack12(payload, zones):
    """Packed 12-bit values -> distance [mm] per zone as uint16."""
    b = np.frombuffer(payload + b"\0" * (-len(payload) % 3), dtype=np.uint8).reshape(-1, 3).astype(np.uint16)
    a = b[:, 0] | ((b[:, 1] & 0x0F) << 8)
    c = (b[:, 1] >> 4) | (b[:, 2] << 4)
    return np.stack([a, c], axis=1).reshape(-1)[:zones] * MM_PER_LSB


class PacketReader:
    """Splits a byte stream into depth packets and the text between them."""

    def __init__(self):
        self.buf = bytearray()
        self.text = bytearray()
        self.bad = 0                    # packets that failed the checksum or sanity checks

    def feed(self, data):
        """Add bytes, return (list of packets, list of complete text lines)."""
        self.buf += data
        packets = []
        while True:
            i = self.buf.find(MAGIC)
            if i < 0:
                # keep a possible partial magic at the end
                keep = len(MAGIC) - 1
                self.text += self.buf[:-keep] if len(self.buf) > keep else b""
                del self.buf[:max(0, len(self.buf) - keep)]
                break
            self.text += self.buf[:i]
            del self.buf[:i]
            if len(self.buf) < HEADER_LEN:
                break

            h = self.buf
            version, binning, width, height = h[4], h[5], h[6], h[7]
            length = int.from_bytes(h[20:22], "little")
            if version != VERSION or length != payload_len(width * height) or length == 0 or length > MAX_PAYLOAD:
                self.bad += 1
                del self.buf[:1]        # not a packet, look for the next magic
                continue
            if len(self.buf) < HEADER_LEN + length:
                break

            payload = bytes(h[HEADER_LEN:HEADER_LEN + length])
            checksum = int.from_bytes(h[26:28], "little")
            if (sum(h[4:26]) + sum(payload)) & 0xFFFF != checksum:
                self.bad += 1
                del self.buf[:1]
                continue

            packets.append({
                "binning": binning,
                "frame": int.from_bytes(h[8:12], "little"),
                "dropped": int.from_bytes(h[12:16], "little"),
                "errors": int.from_bytes(h[16:20], "little"),
                "temperature": int.from_bytes(h[22:24], "little"),
                "laser_temperature": int.from_bytes(h[24:26], "little"),
                "depth": unpack12(payload, width * height).reshape(height, width),
            })
            del self.buf[:HEADER_LEN + length]

        # return complete text lines, skip lines with binary data
        lines = []
        while b"\n" in self.text:
            line, _, rest = self.text.partition(b"\n")
            self.text = bytearray(rest)
            line = bytes(line).strip()
            if line and all(32 <= c < 127 for c in line):
                lines.append(line.decode("ascii"))
        return packets, lines


def orient(a, orientation):
    """Flip or rotate a zone grid for display."""
    if orientation == 1:
        return a[::-1, :]
    if orientation == 2:
        return a[:, ::-1]
    if orientation == 3:
        return a[::-1, ::-1]
    return a


def render(depth_mm, max_mm, scale, orientation):
    """Colour-map depth (0 = no target, black) and scale up with square zones."""
    d = orient(depth_mm, orientation)

    norm = np.clip(d.astype(np.float32) / max_mm, 0, 1)
    img = cv2.applyColorMap((norm * 255).astype(np.uint8), cv2.COLORMAP_TURBO)
    img[d == 0] = 0
    img = cv2.resize(img, (d.shape[1] * scale, d.shape[0] * scale), interpolation=cv2.INTER_NEAREST)
    return img, d


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("source", help="serial port (e.g. /dev/ttyACM0) or a recorded .bin file")
    p.add_argument("--baud", type=int, default=4000000, choices=sorted(BAUDS))
    p.add_argument("--max", type=int, default=4000, help="distance shown as the hottest colour [mm]")
    p.add_argument("--scale", type=int, default=14, help="screen pixels per zone")
    p.add_argument("--record", help="also write the raw byte stream to this file")
    args = p.parse_args()

    replay = stat.S_ISREG(os.stat(args.source).st_mode)
    if replay:
        fd = os.open(args.source, os.O_RDONLY)
    else:
        fd = open_port(args.source, args.baud)
    record = open(args.record, "wb") if args.record else None

    reader = PacketReader()
    max_mm, orientation = args.max, 0
    depth = None
    shown = None
    mouse = [-1, -1]
    fps, frames_in_window, window_start = 0.0, 0, time.monotonic()

    cv2.namedWindow("VL53L9", cv2.WINDOW_AUTOSIZE)
    cv2.setMouseCallback("VL53L9", lambda ev, x, y, flags, param: mouse.__setitem__(slice(None), [x, y]))
    print(f"reading {args.source}" + ("" if replay else f" at {args.baud} baud"))

    while True:
        # replay at ~30 fps by reading one packet's worth at a time
        data = os.read(fd, HEADER_LEN + MAX_PAYLOAD if replay else 65536)
        if replay and not data:
            replay = False              # end of file, keep the last frame on screen
        if record and data:
            record.write(data)

        packets, lines = reader.feed(data)
        for line in lines:
            print(f"board: {line}")
        if packets:
            pkt = packets[-1]           # draw only the newest
            depth = pkt["depth"]
            frames_in_window += len(packets)
            now = time.monotonic()
            if now - window_start >= 1.0:
                fps = frames_in_window / (now - window_start)
                frames_in_window, window_start = 0, now

        if depth is not None:
            img, shown = render(depth, max_mm, args.scale, orientation)
            info = (f"{fps:5.1f} fps  frame {pkt['frame']}  drop {pkt['dropped']}  err {pkt['errors']}  "
                    f"bad {reader.bad}  {pkt['temperature']}/{pkt['laser_temperature']} C  0-{max_mm} mm")
            zx, zy = mouse[0] // args.scale, mouse[1] // args.scale
            if 0 <= zy < shown.shape[0] and 0 <= zx < shown.shape[1]:
                info += f"  [{zx},{zy}] {shown[zy, zx]} mm"
            cv2.rectangle(img, (0, 0), (img.shape[1], 22), (0, 0, 0), -1)
            cv2.putText(img, info, (6, 16), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (255, 255, 255), 1, cv2.LINE_AA)
            cv2.imshow("VL53L9", img)

        key = cv2.waitKey(33 if replay else 1) & 0xFF
        if key in (ord("q"), 27):
            break
        if key == ord("r"):
            orientation = (orientation + 1) % len(ORIENTATIONS)
            print(f"orientation: {ORIENTATIONS[orientation]}")
        if key == ord("]"):
            max_mm += 500
        if key == ord("[") and max_mm > 500:
            max_mm -= 500
        if key == ord("s") and shown is not None:
            stamp = time.strftime("%Y%m%d_%H%M%S")
            cv2.imwrite(f"vl53l9_{stamp}.png", img)
            np.save(f"vl53l9_{stamp}.npy", shown)
            print(f"saved vl53l9_{stamp}.png / .npy")

    os.close(fd)
    if record:
        record.close()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
