This repository provides:

* A very minimal firmware targeting bluepill boards (f103c8_128k)[^1] which,
  paired with a 433Mhz receiver (e.g. cheap Superheterodyne from AE) on pin
  `PB1`, decodes all Ambient Weather F007TH (OOK pulse manchester) frames and
  spits them out as JSON lines on USB serial.
* A very minimal Go binary that reads back said JSON lines and exposes them as
  Prometheus gauges: device id, channel, temperature, humidity, battery and an
  optional location (matched using channel).

[^1]: Should work for any Arduino-ready board.

### Arduino (bluepill) firmware flashing

See [platformio.ini](platformio.ini) for flashing instructions for bluepill
boards.

[`generic_boot20_pc13.bin`](https://github.com/rogerclarkmelbourne/STM32duino-bootloader/blob/master/binaries/generic_boot20_pc13.bin)
is one possible source for the DFU bootloader.

```shell
$ nix build '.#upload-stm32'
$ ./result/bin/upload-stm32
```

### Serial to Prometheus Go exporter

```shell
$ nix build '.#rtl_433_f007th_prometheus'
$ ./result/bin/rtl_433_f007th_prometheus -serial /dev/ttyACM0
```
