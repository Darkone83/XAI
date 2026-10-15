# XAI — Xbox Activity Indicator

<div align=center>

<img src="https://github.com/Darkone83/XAI/blob/main/img/xai.png" width=400> <img src="https://github.com/Darkone83/XAI/blob/main/img/Darkone83.png" width=400>

<img src="https://github.com/Darkone83/XAI/blob/main/img/3D_XAI_MB_Front.png" width=400> <img src="https://github.com/Darkone83/XAI/blob/main/img/3D_XAI_LED_Front.png" width=400>

</div>

XAI (pronounced SAI) is a small activity-light controller for the original Xbox. It provides configurable RGB indicators for **LAN** and **HDD** activity while keeping the install simple and removable.

The main controller stays inside the Xbox and connects to the front LED board with a small FPC cable.

## Features

- Independent RGB LAN and HDD activity indicators
- 16 selectable colors per indicator
- 10 global brightness levels
- Reversible LED positioning
- Persistent settings
- One-button setup and factory reset
- Quick activity LED on/off toggle during normal operation
- 5-click LED self-diagnostic
- 15-second setup inactivity timeout
- Smoothed LAN/HDD activity indication for short pulses
- Compact two-board design with FPC connection

## Where to buy

<a href="https://www.darkonecustoms.com/store/p/xai">Darkone Customs</a>

## Default Settings

- **RGB1 / LAN:** Amber
- **RGB2 / HDD:** Red
- **Brightness:** 100%
- **LED Position Reverse:** Off

Settings are saved in flash and retained after power-off.

## Controls

XAI uses a single setup button.

### Normal Operation

While XAI is operating normally:

- **Double press** the setup button to turn the LAN/HDD activity LEDs **off**.
- **Double press** again to turn the activity LEDs **back on**.
- This on/off state is temporary and is **not saved**. XAI starts with activity indication enabled after each power-up.
- Setup, save, diagnostic, and factory-reset LED feedback will still be shown while normal activity indication is off.
- **Five rapid presses** starts the LED self-diagnostic.

### Activity Indication

LAN and HDD activity is shown immediately when detected. Very short activity pulses are held briefly so the indicators remain visible rather than appearing as extremely fast flicker.

### Enter Setup

Hold the button for **3 seconds**.

Both LEDs will blink **red** to confirm setup mode.

### Setup Order

Each **double press** moves to the next setting:

1. **LAN Color**
2. **HDD Color**
3. **Brightness**
4. **LED Position / Reverse**
5. Back to LAN Color

### Change a Setting

Use a **single press** to change the current setting.

#### LAN / HDD Color

Each press cycles through one of **16 preset colors**.

#### Brightness

Each press cycles through **10 brightness levels**:

- 10%
- 20%
- 30%
- 40%
- 50%
- 60%
- 70%
- 80%
- 90%
- 100%

#### Reverse LED Position

This setting is useful if the physical LED positions are reversed during installation.

During this setting:

- **Green** indicates the LAN LED position
- **Red** indicates the HDD LED position

Single press to swap the two positions.

### Save Settings

Hold the button for **3 seconds** while in setup mode.

Both LEDs will blink **green** and normal operation will resume.

### Setup Timeout

If no button input is detected for **15 seconds** while in setup mode, XAI will exit setup and return to normal operation.

Unsaved changes are discarded. Any valid setup button action restarts the 15-second timer.

## Self Diagnostics

While XAI is operating normally, press the setup button **five times rapidly** to start the LED self-test.

The diagnostic sequence checks:

1. **Color output** — both LEDs cycle through several RGB colors.
2. **Brightness** — both LEDs step through all 10 brightness levels.
3. **RGB1 / RGB2 output** — the two physical LEDs alternate on and off.

The diagnostic does **not** change or save any settings. XAI returns to normal activity indication when the test is complete.

## Factory Reset

Hold the setup button for **10 seconds**.

XAI will restore:

- LAN = Amber
- HDD = Red
- Brightness = 100%
- Reverse = Off

A successful reset is confirmed by both LEDs flashing:

**Green → Orange → Red**

## Installation

Basic Xbox wiring and installation instructions are available here:

[**XAI Installation Guide**](install/README.md)

The install guide covers the required **3.3V, GND, LAN, and HDD** connection points and basic board/cable installation.

## Flashing Prebuilt Firmware

A prebuilt `firmware.bin` can be programmed without compiling the project.

### Required

- WCH-LinkE programmer
- WCH-LinkUtility
- XAI `firmware.bin`

### Programmer Connections

Connect the WCH-LinkE to the XAI programming header:

| WCH-LinkE | XAI |
|---|---|
| 3.3V | 3.3V |
| GND | GND |
| SWIO | SWIO |

> TX and RX are not used for programming XAI.

### Flashing

1. Connect the WCH-LinkE to XAI.
2. Open **WCH-LinkUtility**.
3. Select the CH32V00X / CH32V003 target.
4. Load `firmware.bin`.
5. Set the flash address to:

   `0x08000000`

6. Program and verify the device.
7. Reset or power-cycle XAI.

## Building the Firmware

The firmware can be built with **Visual Studio Code + PlatformIO**.

### Required

- Visual Studio Code
- PlatformIO IDE extension
- WCH-LinkE if you want to upload directly from PlatformIO

### Build

1. Install **Visual Studio Code**.
2. Install the **PlatformIO IDE** extension.
3. Clone or download this repository.
4. Open the project folder in VS Code.
5. Open the PlatformIO panel.
6. Select **Build**.

The project uses:

- **MCU:** CH32V003A4M6
- **Platform:** Community-PIO-CH32V
- **Framework:** NoneOS SDK

The compiled binary will normally be located at:

`/.pio/build/genericCH32V003A4M6/firmware.bin`

### Upload from PlatformIO

With the WCH-LinkE connected to **3.3V, GND, and SWIO**, select **Upload** from PlatformIO.

## Where to Buy

Links will be added once hardware availability is finalized.

### A. Programmer

**WCH-LinkE**

Purchase link: <a href="https://www.amazon.com/dp/B0C6XRCS9Q?ref=ppx_yo2ov_dt_b_fed_asin_title">Amazon</a>

### B. FFC Cable

**5-pin FFC cable**

Purchase link: <a href="https://www.aliexpress.us/item/3256810089762446.html?spm=a2g0o.order_list.order_list_main.4.76431802nWzUqa&gatewayAdapt=glo2usa">AliExpress</a> 5p 200mm Forward direction.

### C. XAI Hardware

**XAI main board / LED board / kit**

Purchase link: <a href="https://www.darkonecustoms.com/store/p/xai">Darkone Customs</a>

## Notes

XAI is designed for the original Xbox and is intended to provide simple, configurable HDD and LAN activity indicators without requiring complex software or permanent front-panel modifications.

More hardware, installation, and release information will be added as the project moves through production testing.
