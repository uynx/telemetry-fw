# telemetry-fw

Telemetry firmware for an ARM Cortex-M3, written in C on FreeRTOS. I built it because a lot of the new grad postings I see are embedded software roles, and I wanted firmware I could build, run and test instead of only pointing at C and C++ on a resume.

It runs on the MPS2 AN385 board (Cortex-M3) as QEMU emulates it, so everything here was tested in an emulator and not on a physical board. The drivers talk to the board's registers directly with no vendor HAL: the CMSDK UART, the CMSDK timer, the NVIC and SysTick through FreeRTOS's ARM_CM3 port.

## What it does

A hardware timer interrupt fires every 10 ms and reads a sample from a fake sensor, which is a fixed formula of the sample index so the host can check every value. The interrupt pushes the sample into a FreeRTOS queue and counts a drop if the queue is full. A telemetry task takes 16 samples at a time and sends them out the UART as a frame, and once a second it sends a status frame with the samples taken in that second, the drop count, each task's stack headroom and the free heap.

Every frame is a type byte, a sequence number, a count, the values and a CRC-16/CCITT, then COBS encoded so the only zero byte on the wire is the delimiter at the end. If a byte is lost the receiver just waits for the next zero and starts again.

The UART receive interrupt passes bytes to a command task through a second queue. Sending `P5` and a newline changes the timer period to 5 ms, which doubles the sample rate. Anything outside 1 to 1000 ms is ignored.

```
src/startup.c     vector table, reset handler (.data copy, .bss zero)
src/main.c        UART and timer drivers, the two ISRs, the two tasks
src/frame.c       CRC-16, COBS and frame building, shared with the host tests
src/mps2.ld       linker script
test/test_frame.c host unit tests (ASan and UBSan)
test/test_qemu.py boots the firmware in QEMU and checks the stream
```

## Run it

The ARM GNU toolchain, QEMU, Python and the FreeRTOS kernel (V11.1.0) are pinned by `devenv.lock`.

```
devenv shell
make test        # host tests, cross compile, then the QEMU test
make build/fw.elf
```

`make test` runs the host tests first. They check the CRC against the standard check value for "123456789" (0x29B1), COBS against the published vectors including a 254 byte run, a decoder that has to reject a length byte past the end of the input, and a full frame round trip.

The QEMU test then boots `build/fw.elf` on `mps2-an385` with `-icount` so the emulated clock is the same from run to run, reads the UART for six status frames, sends `P5` after the third one, and decodes everything. It fails if any frame has a bad CRC or length, if a sequence number is skipped, if any sample value differs from the formula, if the sampler dropped anything, or if the rate is not 100 per second before the command and 200 after it. I checked that it catches a wrong sample formula and a missing command by breaking each on purpose.

## What I measured

All of this is from one run on 2026-11-10 with QEMU 11.1.2 and arm-none-eabi-gcc 15.3.1 at `-O2`. Since the numbers come from an emulator, sizes and counts are real but nothing here is a timing result on hardware.

* The image is 7,848 bytes of code, 84 bytes of initialized data and 8,448 bytes of zeroed RAM, and 8,192 of those are the FreeRTOS heap.
* At runtime the two tasks, their stacks and both queues leave 5,288 of the 8,192 heap bytes free.
* The telemetry task never used more than 79 of its 256 stack words and the command task never more than 38 of its 192, so both stacks could be cut down.
* Over six seconds the firmware sent 56 sample frames and 6 status frames with 0 CRC errors and 0 dropped samples, and the rate went 100, 100, 100, 199, 200, 200 per second with the command sent after the third status frame.

## What it does not do

The UART transmit side polls the status register instead of using an interrupt or DMA. It has not run on a real board, so there is no interrupt latency or power number, and I did not test it against a real sensor.
