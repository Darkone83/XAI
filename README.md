# XAI — Xbox Activity Indicator

XAI is a small activity-light controller for the original Xbox. It provides configurable RGB indicators for **LAN** and **HDD** activity while keeping the install simple and removable.

The main controller stays inside the Xbox and connects to the front LED board with a small FPC cable.

## Default Settings

- **RGB1 / LAN:** Amber
- **RGB2 / HDD:** Red
- **Brightness:** 100%
- **LED Position Reverse:** Off

Settings are saved in flash and retained after power-off.

## Controls

XAI uses a single setup button.

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

## Factory Reset

Hold the setup button for **10 seconds**.

XAI will restore:

- LAN = Amber
- HDD = Red
- Brightness = 100%
- Reverse = Off

A successful reset is confirmed by both LEDs flashing:

**Green → Orange → Red**

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

Purchase link: _TBD_

### B. FPC Cable

**5-pin FPC cable**

Purchase link: _TBD_

### C. XAI Hardware

**XAI main board / LED board / kit**

Purchase link: _TBD_

## Notes

XAI is designed for the original Xbox and is intended to provide simple, configurable HDD and LAN activity indicators without requiring complex software or permanent front-panel modifications.

More hardware, installation, and release information will be added as the project moves through production testing.
