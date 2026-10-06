# PCMD3180 Arduino Library

Arduino library for the [Texas Instruments PCMD3180](https://www.ti.com/product/PCMD3180) eight-channel PDM-to-PCM converter with I2C control interface.

**Not yet published, still in beta**

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
Each PDMIN port carries two microphone channels on one data line, differentiated by clock edge. With the default edge setting:

| Port | Rising edge | Falling edge |
|------|------------|--------------|
| PDMIN1 | CH2 | CH1 |
| PDMIN2 | CH4 | CH3 |
| PDMIN3 | CH6 | CH5 |
| PDMIN4 | CH8 | CH7 |

The default edge assignment (`EDGE_MODE_EVEN_POSITIVE`) samples the even-numbered channel on the rising edge. Most PDM microphones specify which edge their data is valid on. Check your mic's datasheet and use `setPortEdgeLatchMode()` if you need to change the default.

### ASI Output
Connect BCLK, FSYNC, and SDOUT to your MCU or DSP's I2S/TDM input. In slave mode the PCMD3180 follows the clocks your host provides; in master mode it drives them.

## Installation

1. Create a folder named `PCMD3180` inside your Arduino `libraries` folder
2. Copy `PCMD3180.h` and `PCMD3180.cpp` into it
3. Restart the Arduino IDE

## Quick Start

Most setups follow the same three-step pattern after `begin()`:

1. `configureAsSlave()` or `configureAsMaster()` — sets the ASI format and channel count
2. `configurePDMInput()` — sets the PDM clock, selects PDM input on the needed channels, and configures the PDMCLK (GPO) and PDMDIN (GPI) pins
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

In master mode the PLL needs a master clock (MCLK) on GPIO1 or one of the GPI pins. Tell the
library its frequency, and configure the pin it arrives on. When all four GPI pins are used for
PDM data (more than 6 channels), MCLK must come in on GPIO1.

**Warning: Master mode has not been tested**
```cpp
#include <Wire.h>
#include "PCMD3180.h"

PCMD3180 mic;

void setup() {
  Wire.begin();

  if (!mic.begin()) {
    while (1);
  }

  // 12.288 MHz MCLK on GPIO1
  mic.setGPIOConfig(GPIO_MODE_MCLK, DRIVE_MODE_HI_Z);

  // I2S master at 48 kHz, BCLK = 64 × FSYNC = 3.072 MHz
  mic.configureAsMaster(FORMAT_I2S, WORD_32_BIT, FSRATE_48, BCLKRATIO_64, 2, MCLK_FREQ_12288_KHZ);
  mic.configurePDMInput(PDMCLK_2822_KHZ, 2);
  mic.powerPDM(true);
  mic.powerPLL(true);
}

void loop() {}
```

---

### Adjusting digital volume

`setDigitalVolume()` accepts values 0x00–0xFF in 0.5 dB steps. `0x00` is mute;
`0xC9` is unity gain (0 dB, the default); `0xFF` is +27 dB.

```cpp
// Set all channels to unity gain
mic.setAllChannelVolumes(0xC9);

// Or set individual channels
mic.setDigitalVolume(1, 0xC9);  // CH1 unity gain
mic.setDigitalVolume(2, 0xBF);  // CH2 5 dB lower
```

---

### Hardware shutdown pin

If your board connects a GPIO to the SHDNZ pin, pass it to the constructor.
`begin()` sets the pin as an output, drives it low to put the device in hardware shutdown, then releases it. The constructor does not touch the pin; to hold the device in shutdown from power-on, add a pull-down resistor on SHDNZ.

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
  mic.configureBias(MICBIAS_MODE_AVDD, VREF_MODE_275V);

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
PCMD3180 micA(0xFF, 0x4C);
PCMD3180 micB(0xFF, 0x4D);

void setup() {
  Wire.begin();

  // Each device must be initialized individually
  micA.begin();
  micB.begin();

  // Enable broadcast on all devices before shared configuration
  micA.setI2CBroadcast(true);
  micB.setI2CBroadcast(true);

  // These calls now go to both devices simultaneously via the broadcast address
  micA.configureAsSlave(FORMAT_TDM, WORD_32_BIT, 8);
  micA.configurePDMInput(PDMCLK_2822_KHZ, 8);
  micA.powerPDM(true);
  micA.powerPLL(true);
}
```

## API Reference

### Constructor

```cpp
PCMD3180(uint8_t shdnzPin      = PCMD3180_SHDNZ_PIN_DEFAULT,
         uint8_t i2cAddr       = PCMD3180_I2C_ADDR_DEFAULT,
         bool    aregInternal  = false,
         AVDDInputVoltage avddInputVoltage = AVDD_INPUT_33V);
```

Pass `PCMD3180_SHDNZ_PIN_DEFAULT` (0xFF) for `shdnzPin` if you are not controlling the shutdown pin from firmware. Pass `true` for `aregInternal` if you want to generate the AREG voltage internally in the PCMD3180.

---

### Utility methods

These cover the majority of use cases and are the recommended starting point.

| Method | Description |
|--------|-------------|
| `begin(wire)` | Initialize device, reset, wake. Returns `false` if not found on I2C. |
| `configureAsSlave(format, wordLen, numChannels)` | Set ASI format and enable channels; device follows host clocks. |
| `configureAsMaster(format, wordLen, fsyncRate, bclkRatio, numChannels, mclkFreq)` | Set ASI format, MCLK frequency and clock rates, enable channels; device drives clocks. |
| `configurePDMInput(clk, numChannels)` | Set PDM clock frequency; for each needed port, select PDM input, output PDMCLK on GPOx and take PDMDINx on GPIx; enable the channels. Assumes port x uses PDMCLKx_GPOx and PDMDINx_GPIx. |
| `enableAllChannels(numChannels)` | Enable the first N input and output channels by count instead of bitmask. |
| `setAllChannelVolumes(volume)` | Set the same digital volume on all 8 channels. |

---

### Per-channel and per-port control

| Method | Description |
|--------|-------------|
| `setDigitalVolume(channel, volume)` | Volume for one channel. `0x00` = mute, `0xC9` = 0 dB, `0xFF` = +27 dB. |
| `setGainCalibration(channel, value)` | Fine gain trim per channel (0–15). |
| `setPhaseCalibration(channel, value)` | Phase delay per channel, in modulator clock cycles (0–255, ~163 ns each). |
| `enablePort(port, enable)` | Select PDM as the input source for a port's two channels (ports 1–2; channels 5–8 are always PDM). Does not configure pins. |
| `setPortEdgeLatchMode(port, mode)` | Choose which clock edge latches the even-numbered channel. |
| `setASISlotAssignment(channel, slot)` | Assign a channel to a TDM slot (0–63). |
| `setASIOutputLine(channel, pin)` | Route a channel to the primary or secondary SDOUT. |

---

### ASI output timing and clocking

| Method | Description |
|--------|-------------|
| `setASITXOffset(offset)` | Offset slot 0 by 0–31 BCLK cycles from the standard protocol timing. |
| `setASITXEdge(edge)` | Transmit data on the default or the inverted (half-cycle delayed) BCLK edge. |
| `setASITXFill(fill)` | Transmit 0 or Hi-Z during unused cycles (Hi-Z lets devices share the data line). |
| `setASITXLSB(lsb)` | Drive the LSB for a full cycle, or for half a cycle and then Hi-Z. |
| `setASIBusKeeper(keeper)` | Bus keeper on the data output: off, always on, or only during the LSB. |
| `setMCLKFrequency(freq)` | MCLK frequency used as the PLL reference in master mode. |
| `setMCLKRatio(ratio)` | Specify MCLK as a multiple of FSYNC instead (master mode, or slave mode with MCLK as root clock). |

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
| `getDeviceStatus(status0, deviceModeStatus)` | Read DEV_STS0 and the device mode from DEV_STS1 (bits 7:5). |
| `getLatchedInterruptStatus(asiBusClockError, pllLockError)` | Read latched interrupt flags. |
| `getI2CChecksum(checksum)` | Read the I2C transactions checksum (I2C_CKSUM, 0x7E). The datasheet does not document how it is computed. |
| `resetI2CChecksum(value)` | Reset the checksum to `value` (default 0). |
| `getAutodetectedClocks(fsRate, ratio)` | Read auto-detected sample rate (as `FSRate`, e.g. `FSRATE_48` = 44.1 or 48 kHz) and BCLK/FSYNC ratio (slave mode). |

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
mic.writeRegister(0x00, pageNumber);  // select page
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

**Programmable coefficients (pages 2–4)**
`setBiquadConfig()` sets the number of biquads per channel (0–3), but the biquad coefficients (pages 2 and 3), the digital mixer coefficients and the custom high-pass filter coefficients (page 4) are not accessible through any library method. Each coefficient is a 32-bit value written as four bytes, most significant byte first. To load coefficients you need to switch pages manually, write the coefficient registers, and return to page 0:
```cpp
mic.writeRegister(0x00, 2);          // select page 2
mic.writeRegister(coeffReg, value); // write coefficient byte
mic.writeRegister(0x00, 0);          // return to page 0
```
Refer to datasheet sections 6.3.6.4 to 6.3.6.6 and 7.2 for the register map and coefficient format.

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
- Ensure that the number of enabled biquads per channel is less or equal to what the PCDM3180 supports. See the section `Programmable Digital Biquad Filters` in the datasheet.

## Resources

- [PCMD3180 Datasheet](https://www.ti.com/product/PCMD3180)

## License

This library is provided under an MIT license. You are permitted to copy or modify this code, and you are permitted to use it in commercial products.

## Author

Magne Lauritzen / [Summa Cogni](https://summacogni.com)