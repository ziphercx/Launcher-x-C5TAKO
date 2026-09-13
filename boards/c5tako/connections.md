# C5TAKO (XIAO ESP32-C5)

Environment: `c5tako` (same name as the Bruce port). Hardware testing is still required.

Pins, battery conversion and power-off logic come from Bruce's `boards/c5tako` port and the XC5 reference in `ZipherDeauthC5`. Inputs and brightness use Launcher's existing HAL; the display uses its Arduino_GFX ST7789 backend.

| Hardware | GPIO / settings |
| --- | --- |
| Memory | 8MB flash, 8MB QSPI PSRAM, qio_qspi |
| Shared SPI | SCK 8, MOSI 10, MISO 9 |
| ST7789 IPS | 240x240, CS 7, DC 1, RST -1, BL 25 active HIGH |
| Display | Rotation 0, RGB, normal colors (no extra inversion), SPI mode 3; rotation 2 row offset 80, rotation 3 column offset 80 |
| SD | CS 5, initially mounted at 4MHz before display |
| Buttons | Up 24, down 28, left 23, right 0, OK 4; active LOW, internal pull-ups |
| CC1101 | CS 2 held HIGH; GDO0 12 and GDO2 11 reserved; Launcher does not need an RF driver |
| Buzzer / ordinary LED | Buzzer 3 initialized LOW; LED 27 active LOW, initialized off |
| Battery | ADC 6, enable 26 HIGH while sampling; milliVolts x 2, same cached 16-sample LiPo curve as Bruce |
| USB | Hardware Serial/JTAG CDC, USB mode 1, CDC on boot |

Left/right select previous/next, up/down navigate the keyboard, OK selects, and left+right together goes back using the existing five-button HAL. Directions follow the current display rotation. Hold OK while resetting to return to Launcher through its patched key-boot SDK. Power off sends ST7789 sleep-in then enters deep sleep; OK wakes it. No power latch or charging-status GPIO is available.

All SPI chip selects are deselected before initialization. SD mounts before TFT and its mount result is restored in `_post_setup_gpio()` because Launcher clears `sdcardMounted` during startup. Default XIAO UART/I2C pins overlap the buttons or CC1101; this port does not initialize those peripherals.

Keep Launcher's inherited patched SDK. This board uses `boards/c5tako/partitions.csv`: NVS 0x9000/0x5000, OTA data 0xE000/0x2000, Launcher test app 0x10000/0x180000 (1.5MB), coredump 0x190000/0x10000. The original 0x150000 test partition was too small for the C5 build. Remaining flash space is managed by Launcher. Do not copy Bruce's factory partitions or NimBLE patch. Launcher's existing merge script already handles the ESP32-C5 bootloader offset 0x2000.

When requested, build with `pio run -e c5tako`. Verify PSRAM, all four display rotations, SD read/write, buttons, backlight, battery, deep sleep and OK key boot on real hardware before enabling this environment in the default/CI lists.
