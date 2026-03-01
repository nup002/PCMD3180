# PCMD3180 Arduino Library

Arduino library for the Texas Instruments PCMD3180 octa-channel PDM-to-PCM converter with I2C control interface.

## Overview

The PCMD3180 is a 4-port Pulse Density Modulation (PDM) input, 8-channel Pulse Code Modulation (PCM) output audio codec. It accepts up to four PDM microphone pairs (8 physical microphones) and outputs PCM audio over Inter-Integrated Circuit Sound (I2S) or Time-Division Multiplexing (TDM). This library provides an Arduino interface built on the `Wire` library.

## Features

- Utility methods for common setups: `configureAsSlave()`, `configureAsMaster()`, `configurePDMInput()`
- I2S, Left-Justified, and TDM (Time-Division Multiplexed) serial formats
- Configurable word lengths: 16, 20, 24, 32-bit
- Master and slave clock modes, with auto clock detection in slave mode
- Per-channel digital volume control (channels 1–8)
- PDM clock output configuration
- GPIO, GPO, and GPI port configuration
- Granular power control: `powerMICBIAS()`, `powerPDM()`, `powerPLL()`, `wakeUp()`, `sleep()`
- Hardware shutdown via SHDNZ pin
- Low-level register access for advanced use

## Hardware Setup

### I2C Connections
| PCMD3180 | Arduino |
|----------|---------|
| SDA | SDA |
| SCL | SCL |
| GND | GND |
| AVDD, DVDD | 3.3 V (check datasheet for all supply pins) |

Add 4.7 kΩ pull-up resistors on SDA and SCL if not already present on your board.

### I2C Address
Default address is `0x4C`. This can be changed via hardware address pins. Check your schematic.

### PDM Microphone Connections
Each PDMIN port carries two microphone channels on one data line, differentiated by clock edge:

| Port | Rising edge | Falling edge |
|------|------------|--------------|
| PDMIN1 | CH1 | CH2 |
| PDMIN2 | CH3 | CH4 |
| PDMIN3 | CH5 | CH6 |
| PDMIN4 | CH7 | CH8 |

The default edge assignment (`EDGE_MODE_EVEN_NEGATIVE`) samples the even-numbered channel on the falling edge. Most PDM microphones specify which edge their data is valid on. Check your mic's datasheet and use `setPortEdgeLatchMode()` if you need to change the default.

### ASI Output
Connect BCLK, FSYNC, and SDOUT to your MCU or DSP's I2S/TDM input. In slave mode the PCMD3180 follows the clocks your host provides; in master mode it drives them.

## Installation

1. Create a folder named `PCMD3180` inside your Arduino `libraries` folder
2. Copy `PCMD3180.h` and `PCMD3180.cpp` into it
3. Restart the Arduino IDE

## Quick Start

Most setups follow the same three-step pattern after `begin()`:

1. `configureAsSlave()` or `configureAsMaster()` — sets the ASI format and channel count
2. `configurePDMInput()` — sets the PDM clock and enables the right ports
3. `powerPDM(true)` — powers up the PDM block

```cpp
#include <Wire.h>
#include "PCMD3180.h"

PCMD3180 mic;

void setup() {
  Wire.begin();

  if (!mic.begin()) {
    while (1);  // halt on failure
  }

  mic.configureAsSlave(FORMAT_I2S, WORD_32_BIT, 2);
  mic.configurePDMInput(PDMCLK_2822_KHZ, 2);
  mic.powerPDM(true);
  mic.powerPLL(true);
}
```

## Examples

### Stereo I2S slave (2 channels)

The most common setup: two PDM mics on port 1, output over I2S, clocked by the host.

```cpp
#include <Wire.h>
#include "PCMD3180.h"

PCMD3180 mic;

void setup() {
  Wire.begin();

  if (!mic.begin()) {
    while (1);
  }

  // I2S, 32-bit words, slave mode, 2 channels
  mic.configureAsSlave(FORMAT_I2S, WORD_32_BIT, 2);

  // Enable port 1 (CH1 + CH2) at 2.8224 MHz PDM clock
  mic.configurePDMInput(PDMCLK_2822_KHZ, 2);

  mic.powerPDM(true);
  mic.powerPLL(true);
}

void loop() {}
```

---

### Quad TDM slave (4 channels)

Four PDM mics across two ports, output over TDM. Useful when connecting to a DSP that
consumes multiple channels on one serial line.

```cpp
#include <Wire.h>
#include "PCMD3180.h"

PCMD3180 mic;

void setup() {
  Wire.begin();

  if (!mic.begin()) {
    while (1);
  }

  // TDM, 32-bit, slave mode, 4 channels (ports 1 and 2 enabled automatically)
  mic.configureAsSlave(FORMAT_TDM, WORD_32_BIT, 4);
  mic.configurePDMInput(PDMCLK_2822_KHZ, 4);
  mic.powerPDM(true);
  mic.powerPLL(true);
}

void loop() {}
```

---

### I2S master (device drives clocks)

Use this when the PCMD3180 is the clock source on the bus, for example when connecting
directly to an MCU I2S peripheral in slave mode.

```cpp
#include <Wire.h>
#include "PCMD3180.h"

PCMD3180 mic;

void setup() {
  Wire.begin();

  if (!mic.begin()) {
    while (1);
  }

  // I2S master at 48 kHz, BCLK = 64 × FSYNC = 3.072 MHz
  mic.configureAsMaster(FORMAT_I2S, WORD_32_BIT, FSRATE_48, BCLKRATIO_64, 2);
  mic.configurePDMInput(PDMCLK_2822_KHZ, 2);
  mic.powerPDM(true);
  mic.powerPLL(true);
}

void loop() {}
```

---

### Adjusting digital volume

`setDigitalVolume()` accepts values 0x00–0xFF. `0x00` is approximately mute;
`0x7F` is unity gain (0 dB); `0xFF` is +24 dB.

```cpp
// Set all channels to unity gain
mic.setAllChannelVolumes(0x7F);

// Or set individual channels
mic.setDigitalVolume(1, 0x7F);  // CH1 unity gain
mic.setDigitalVolume(2, 0x60);  // CH2 slightly lower
```

---

### Hardware shutdown pin

If your board connects a GPIO to the SHDNZ pin, pass it to the constructor.
The library will assert it low at construction and release it in `begin()`.

```cpp
#define SHDNZ_PIN 5

PCMD3180 mic(SHDNZ_PIN);

void setup() {
  Wire.begin();
  mic.begin();  // releases SHDNZ internally
  // ...
}
```

### Using MICBIAS as microphone supply

The MICBIAS pin can supply power directly to PDM microphones, eliminating the need for an external supply. TI recommends configuring it to track AVDD (rather than VREF) so the PDM signal levels match AVDD directly and no level shifters are needed. MICBIAS is off by default.

```cpp
void setup() {
  Wire.begin();
  mic.begin();

  mic.configureAsSlave(FORMAT_I2S, WORD_32_BIT, 2);
  // Configure MICBIAS to follow AVDD voltage
  // VREF_SEL defaults to 2.75 V — set appropriately for your AVDD
  mic.configureBIAS(MICBIAS_AVDD, VREF_2V75);

  // Power on MICBIAS before the PDM block
  mic.powerMICBIAS(true);

  mic.configurePDMInput(PDMCLK_2822_KHZ, 2);
  mic.powerPDM(true);
  mic.powerPLL(true);
}
```

---

### AVDD at 1.8 V

If your AVDD supply is 1.8 V rather than 3.3 V, pass `AVDD_INPUT_18V` to the constructor.
The library will automatically set VREF to 1.375 V during `begin()` to keep it below AVDD.

```cpp
PCMD3180 mic(PCMD3180_SHDNZ_PIN_DEFAULT, PCMD3180_I2C_ADDR_DEFAULT,
             false, AVDD_INPUT_18V);
```

---

### Multiple devices on one bus

Each device needs a unique I2C address (set via hardware address pins). Call `begin()` on each device individually first, this resets and wakes each one. Then enable broadcast on all of them so subsequent shared configuration writes reach every device simultaneously.

```cpp
PCMD3180 mic_a(0xFF, 0x4C);
PCMD3180 mic_b(0xFF, 0x4D);

void setup() {
  Wire.begin();

  // Each device must be initialized individually
  mic_a.begin();
  mic_b.begin();

  // Enable broadcast on all devices before shared configuration
  mic_a.setI2CBroadcast(true);
  mic_b.setI2CBroadcast(true);

  // These calls now go to both devices simultaneously via the broadcast address
  mic_a.configureAsSlave(FORMAT_TDM, WORD_32_BIT, 8);
  mic_a.configurePDMInput(PDMCLK_2822_KHZ, 8);
  mic_a.powerPDM(true);
  mic_a.powerPLL(true);
}
```

## API Reference

### Constructor

```cpp
PCMD3180(uint8_t shdnz_pin    = PCMD3180_SHDNZ_PIN_DEFAULT,
         uint8_t i2c_addr     = PCMD3180_I2C_ADDR_DEFAULT,
         bool    areg_internal = false,
         AVDDInputVoltage avdd = AVDD_INPUT_33V);
```

Pass `PCMD3180_SHDNZ_PIN_DEFAULT` (0xFF) for `shdnz_pin` if you are not controlling the shutdown pin from firmware. Pass `true` for `areg_internal` if you want to generate the AREG voltage internally in the PCMD3180.

---

### Utility methods

These cover the majority of use cases and are the recommended starting point.

| Method | Description |
|--------|-------------|
| `begin(wirePort)` | Initialize device, reset, wake. Returns `false` if not found on I2C. |
| `configureAsSlave(format, word_len, num_channels)` | Set ASI format and enable channels; device follows host clocks. |
| `configureAsMaster(format, word_len, fsync_rate, bclk_ratio, num_channels)` | Set ASI format, program clock rates, enable channels; device drives clocks. |
| `configurePDMInput(clk, num_channels)` | Set PDM clock frequency and enable the required ports and channels. |
| `enableAllChannels(num_channels)` | Enable the first N input and output channels by count instead of bitmask. |
| `setAllChannelVolumes(volume)` | Set the same digital volume on all 8 channels. |

---

### Per-channel and per-port control

| Method | Description |
|--------|-------------|
| `setDigitalVolume(channel, volume)` | Volume for one channel. `0x00` ≈ mute, `0x7F` = 0 dB, `0xFF` = +24 dB. |
| `setGainCalibration(channel, value)` | Fine gain trim per channel (0–15). |
| `setPhaseCalibration(channel, value)` | Phase offset per channel (0–255). |
| `enablePort(port, enable)` | Enable or disable a PDM input port (1–4). |
| `setPortEdgeLatchMode(port, mode)` | Choose which clock edge latches the even-numbered channel. |
| `setASISlotAssignment(channel, slot)` | Assign a channel to a TDM slot (0–63). |
| `setASIOutputLine(channel, pin)` | Route a channel to the primary or secondary SDOUT. |

---

### Power management

| Method | Description |
|--------|-------------|
| `powerPDM(enable)` | Power the PDM input block on or off. |
| `powerMICBIAS(enable)` | Power MICBIAS on or off. |
| `powerPLL(enable)` | Power the PLL on or off. |
| `wakeUp()` | Exit sleep mode. |
| `sleep()` | Enter sleep mode (registers retained). |
| `hardwarePowerUp()` | Assert SHDNZ pin high (requires pin configured in constructor). |
| `hardwarePowerDown()` | Assert SHDNZ pin low. |

---

### Status and diagnostics

| Method | Description |
|--------|-------------|
| `isConnected()` | Returns `true` if the device acknowledges on I2C. |
| `getDeviceStatus(status0, status1)` | Read DEV_STS0 and DEV_STS1 registers. |
| `getLatchedInterruptStatus(asi_error, pll_error)` | Read latched interrupt flags. |
| `getAutodetectedClocks(fsync, ratio)` | Read auto-detected FSYNC and BCLK/FSYNC ratio (slave mode). |

---

### Low-level register access

Use these if you need registers not covered by the higher-level API.

```cpp
mic.writeRegister(reg, value);
mic.readRegister(reg, value);
mic.updateRegisterBits(reg, mask, value);  // read-modify-write
```

The PCMD3180 uses register paging (pages 0–4). This library implements page 0 only. To access other pages:

```cpp
mic.writeRegister(0x00, page_number);  // select page
mic.writeRegister(reg, value);         // access register on that page
mic.writeRegister(0x00, 0);            // return to page 0
```

## Technical Notes

### Volume control
The volume register uses a linear code where each step is approximately 0.5 dB. Notable values:

| Register value | Decimal | Level |
|---------------|---------|-------|
| `0x00` | 0 | Mute |
| `0x01` | 1 | –100 dB |
| `0xC9` | 201 | 0 dB (unity gain, default) |
| `0xFF` | 255 | +27 dB |


Consult the datasheet "Digital Volume Control" section for the full dB-per-step table.

### Timing
The library handles the timing requirements from the datasheet automatically:
- 10 ms delay after software reset
- 1 ms delay after entering active mode
- 50 ms delay in `hardwarePowerDown()`

### PDM clock and sample rate
The PDM clock must be chosen to match your microphone and sample rate. Common pairings:

| Sample rate | PDM clock |
|-------------|-----------|
| 48 kHz | `PDMCLK_2822_KHZ` or `PDMCLK_5644_KHZ` |
| 44.1 kHz | `PDMCLK_2822_KHZ` |
| 16 kHz | `PDMCLK_1411_KHZ` |

Check your microphone's datasheet for its supported PDM clock range.

## Unimplemented features

The following device capabilities have no corresponding library method. All can be accessed using the low-level `writeRegister()` / `readRegister()` / `updateRegisterBits()` calls.

**ASI_CFG2 register (0x09) — TX offset**
The TX offset field (bits [4:0]) controls how many BCLKs after FSYNC the device waits before transmitting. There is no `setTXOffset()` method; the default value of 0 (transmit on the first BCLK after FSYNC) is suitable for standard I2S and TDM. If you need a non-zero offset, write directly:
```cpp
mic.updateRegisterBits(0x09, 0x1F, offset_value);
```

**Biquad filter coefficients (pages 1–4)**
`setBiquadCfg()` sets the number of active biquad stages (0–3), but the actual filter coefficients live in registers on pages 1–4 and are not accessible through any library method. To load custom coefficients you need to switch pages manually, write the coefficient registers, and return to page 0:
```cpp
mic.writeRegister(0x00, 1);          // select page 1
mic.writeRegister(coeff_reg, value); // write coefficient
mic.writeRegister(0x00, 0);          // return to page 0
```
Refer to the datasheet section "Programmable Biquad Filter" for the register map and coefficient format.

**I2C checksum (REG_I2C_CKSUM, 0x7E)**
The hardware checksum feature for verifying I2C writes is not exposed. `setI2CBroadcast()` uses this register internally to set the broadcast bit, but the checksum enable bit itself has no dedicated method.

## Troubleshooting

**Device not found (`begin()` returns `false`)**
- Check SDA/SCL wiring and pull-up resistors
- Verify the I2C address matches your hardware (default 0x4C)
- Try `Wire.setClock(100000)` to lower the bus speed

**No audio output**
- Confirm `powerPDM(true)` was called
- Verify PDM microphone wiring and that the correct PDM clock frequency is set
- Check that both input channels (`enableChannels`) and output channels (`enableOutputASIChannels`) are enabled — `configurePDMInput()` and `enableAllChannels()` handle both together
- Verify BCLK and FSYNC are present on the ASI pins (use a scope or logic analyser)

**Distorted or clipped audio**
- Lower the digital volume with `setDigitalVolume()` or `setAllChannelVolumes()`
- Verify the PDM clock frequency is appropriate for your sample rate

**Some channels are not synchronized to the others**
- Ensure that the number of enabled biquads per channel is less or equal to what the PCDM3180 supports. See the section 'Programmable Digital Biquad Filters' in the datasheet.

## Resources

- [PCMD3180 Datasheet](https://www.ti.com/product/PCMD3180)
- [TLV320ADC3140 Datasheet](https://www.ti.com/product/TLV320ADC3140) (related device, same register map)
- [Linux kernel driver](https://github.com/torvalds/linux/blob/master/sound/soc/codecs/tlv320adcx140.c) (reference implementation this library is based on)

## License

This library is provided under an MIT license. You are permitted to copy or modify this code, and you are permitted to use it in commercial products.

## Author

Magne Lauritzen / [Summa Cogni](https://summacogni.com)