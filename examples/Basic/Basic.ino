/*
 * PCMD3180 Basic Example
 *
 * Sets up the PCMD3180 over I2C as a TDM slave with eight PDM microphones,
 * then prints the device status every second.
 *
 * Hardware connections:
 * - SDA, SCL -> Arduino SDA, SCL (with pull-up resistors)
 * - GND -> GND, AVDD and IOVDD -> 3.3V
 * - PDM microphones on PDMCLKx_GPOx and PDMDINx_GPIx (x = 1-4)
 * - BCLK and FSYNC driven by your TDM/I2S host, SDOUT to the host's data input
 */

#include <Wire.h>
#include <PCMD3180.h>

// Default I2C address (0x4C), no SHDNZ pin
PCMD3180 mic;

// Sample rates reported by getAutodetectedClocks(), indexed by FSRate.
// The device does not report whether the bus is in the 44.1kHz or 48kHz family.
const char *fsRateNames[] = {"7.35/8", "14.7/16", "22.05/24", "29.4/32", "44.1/48",
                             "88.2/96", "176.4/192", "352.8/384", "705.6/768"};

void setup() {
  Serial.begin(115200);
  Wire.begin();

  if (!mic.begin()) {
    Serial.println("ERROR: PCMD3180 not found. Check wiring and I2C address.");
    while (1);
  }

  // TDM, 32-bit words, 8 channels; the device follows the host's BCLK and FSYNC
  if (!mic.configureAsSlave(FORMAT_TDM, WORD_32_BIT, 8)) {
    Serial.println("ERROR: Failed to configure ASI");
  }

  // PDM clock, PDM input selection and PDMCLK/PDMDIN pins for all four ports
  if (!mic.configurePDMInput(PDMCLK_2822_KHZ, 8)) {
    Serial.println("ERROR: Failed to configure PDM input");
  }

  // Power up the PDM channels and the PLL
  if (!mic.powerPDM(true) || !mic.powerPLL(true)) {
    Serial.println("ERROR: Failed to power up");
  }

  Serial.println("PCMD3180 ready");
}

void loop() {
  uint8_t channelStatus, deviceMode;
  if (mic.getDeviceStatus(channelStatus, deviceMode)) {
    Serial.print("Channels powered up: 0b");
    Serial.print(channelStatus, BIN);
    Serial.print(", device mode: ");
    Serial.println(deviceMode);  // 4 = sleep, 6 = active with PDM off, 7 = active with PDM on
  } else {
    Serial.println("ERROR: Failed to read device status");
  }

  FSRate fsRate;
  uint32_t ratio;
  if (mic.getAutodetectedClocks(fsRate, ratio)) {
    Serial.print("Detected sample rate: ");
    Serial.print(fsRateNames[fsRate]);
    Serial.print(" kHz, BCLK/FSYNC ratio: ");
    Serial.println(ratio);
  } else {
    Serial.println("No valid BCLK/FSYNC detected");
  }

  delay(1000);
}
