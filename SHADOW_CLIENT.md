# Shadow Client — ESP32 DevKit + 1.8-inch TFT

This source-tree build profile targets an ESP32 DevKit/WROOM-32 and a 128×160 ST7735 TFT with the **red-tab** initialization. It keeps the normal device display/menu but adds an opt-in, local web page with **display-only** controls: show a short text message, clear the display, and read access-point status. The web page cannot launch Marauder scans, attacks, credential capture, or arbitrary serial commands.

## TFT wiring used by this profile

| ST7735 module | ESP32 DevKit |
| --- | ---: |
| VCC | 3V3 |
| GND | GND |
| SCL / SCK | GPIO 18 |
| SDA / MOSI | GPIO 23 |
| CS | GPIO 5 |
| DC / A0 | GPIO 2 |
| RES / RESET | GPIO 4 |
| LED / BL | 3V3 |
| SDO / MISO | Leave disconnected |

The backlight is assumed to be connected directly to 3.3 V. Do not connect the display power to 5 V unless the specific module explicitly supports it. If your module's pins are already wired differently, change both `User_Setup_shadow_client.h` and the Shadow Client display pin definitions in `esp32_marauder/configs.h` before compiling.

## Build

Use the repository's Arduino CLI setup/build workflow, select the `Shadow Client ESP32 DevKit` matrix target, and use the generated `shadow_client` firmware artifact. The target uses `SHADOW_CLIENT_ESP32` and `User_Setup_shadow_client.h`; that setup selects `ST7735_REDTAB`, 128×160 dimensions, and a conservative 20 MHz SPI clock. The red-tab driver profile and compact, unscaled text splash avoid using the scaled legacy logo on this small panel.

For local Arduino CLI builds, configure the same ESP32 core and libraries as the `.github/workflows/build_parallel.yml` workflow, ensure `User_Setup_shadow_client.h` is selected in TFT_eSPI's `User_Setup_Select.h`, then compile `esp32_marauder/esp32_marauder.ino` with `-DSHADOW_CLIENT_ESP32` for an ESP32 Dev Module with the `min_spiffs` partition scheme.

## Start the web display console

1. Flash the Shadow Client firmware and open the serial monitor at 115200 baud.
2. Enter `shadowweb` and press Enter.
3. The ESP32 starts a WPA2-protected access point with a unique `ShadowClient-XXXX` name and a newly generated password. The SSID, password, and URL (`http://192.168.4.1`) are printed to the serial monitor. The TFT shows that the console is ready.
4. Connect a phone to that access point and open the displayed URL. Use **Show on display**, **Clear**, or **Refresh status**.
5. Enter `shadowweb stop` over serial to stop the page and return the TFT to the on-device interface.

The access point password is regenerated on each start and is intentionally not shown on the web page or TFT. Keep the serial output private while the console is running. The page is local to the ESP32 access point; it does not connect to a cloud service.
