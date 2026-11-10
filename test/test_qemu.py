"""Boot the firmware in QEMU, decode its UART stream and check it.

python3 test/test_qemu.py build/fw.elf
"""
import os
import select
import subprocess
import sys
import time

SAMPLES, STATUS = 1, 2


def crc16(b):
    c = 0xFFFF
    for x in b:
        c ^= x << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def cobs_decode(b):
    out, i = bytearray(), 0
    while i < len(b):
        code = b[i]
        out += b[i + 1:i + code]
        i += code
        if code < 0xFF and i < len(b):
            out.append(0)
    return bytes(out)


def expected(k):
    return ((k * 2654435761) & 0xFFFFFFFF) >> 20


def frames(elf, seconds, command_at=None):
    q = subprocess.Popen(
        ["qemu-system-arm", "-M", "mps2-an385", "-display", "none", "-monitor", "none",
         "-icount", "shift=4,align=off", "-serial", "stdio", "-kernel", elf],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    buf, out, statuses, sent = b"", [], 0, False
    end = time.monotonic() + 120
    try:
        while statuses < seconds and time.monotonic() < end:
            if select.select([q.stdout], [], [], 1)[0]:
                buf += os.read(q.stdout.fileno(), 4096)
            *done, buf = buf.split(b"\0")
            for f in done:
                raw = cobs_decode(f)
                if crc16(raw) != 0 or raw[3] * 2 + 6 != len(raw):
                    out.append(("bad", raw))
                    continue
                vals = [int.from_bytes(raw[4 + 2 * i:6 + 2 * i], "little") for i in range(raw[3])]
                out.append((raw[0], raw[1] | raw[2] << 8, vals))
                statuses += raw[0] == STATUS
            if command_at is not None and statuses == command_at and not sent:
                q.stdin.write(b"P5\n")
                q.stdin.flush()
                sent = True
    finally:
        q.kill()
        q.wait()
    return out


def check(out, seconds):
    bad = [f for f in out if f[0] == "bad"]
    assert not bad, f"{len(bad)} frames failed CRC or length"
    samples = [f for f in out if f[0] == SAMPLES]
    status = [f for f in out if f[0] == STATUS]
    assert len(status) >= seconds, f"only {len(status)} status frames"
    for kind in (samples, status):
        seqs = [f[1] for f in kind]
        assert seqs == list(range(len(seqs))), "sequence gap"
    k = 0
    for _, _, vals in samples:
        assert vals == [expected(k + i) for i in range(len(vals))], f"sample values differ at {k}"
        k += len(vals)
    assert all(s[2][1] == 0 for s in status), "the sampler dropped samples"
    return samples, status


def main(elf):
    out = frames(elf, seconds=6, command_at=3)
    samples, status = check(out, 6)
    rates = [s[2][0] for s in status]
    # status 0 covers boot; 1 and 2 run at 10 ms, 4 and 5 after "P5" at 5 ms
    assert all(abs(r - 100) <= 1 for r in rates[1:3]), f"rates before P5: {rates}"
    assert all(abs(r - 200) <= 1 for r in rates[4:6]), f"rates after P5: {rates}"
    hw = [s[2][2:4] for s in status]
    print(f"ok qemu: {len(samples)} sample frames, {len(status)} status frames, 0 CRC errors, "
          f"rates {rates}, stack headroom words (telemetry, command) min {min(h[0] for h in hw)}, "
          f"{min(h[1] for h in hw)}, heap free {status[-1][2][4]} bytes")


if __name__ == "__main__":
    main(sys.argv[1])
