/*
 * PCMD3180 SPI Example
 *
 * Same as the Basic example, but controls the PCMD3180 over SPI instead of
 * I2C. The device detects which control interface is used from its pins.
 *
 * Hardware connections:
 * - SCL_MOSI -> MOSI, ADDR1_MISO -> MISO, ADDR0_SCLK -> SCK
 * - SDA_SSZ -> CS_PIN (chip select)
 * - GND -> GND, AVDD and IOVDD -> 3.3V
 * - PDM microphones on PDMCLKx_GPOx and PDMDINx_GPIx (x = 1-4)
 * - BCLK and FSYNC driven by your TDM/I2S host, SDOUT to the host's data input
 */

#include <SPI.h>
#include <PCMD3180.h>

const uint8_t CS_PIN = 10;

PCMD3180 mic;

void setup() {
  Serial.begin(115200);
  SPI.begin();

  // SPI mode 1, 1 MHz clock by default (up to 25 MHz)
  if (!mic.begin(SPI, CS_PIN)) {
    Serial.println("ERROR: PCMD3180 not responding. Check SPI wiring and CS_PIN.");
    while (1);
  }

  if (!mic.configureAsSlave(FORMAT_TDM, WORD_32_BIT, 8)) {
    Serial.println("ERROR: Failed to configure ASI");
  }
  if (!mic.configurePDMInput(PDMCLK_2822_KHZ, 8)) {
    Serial.println("ERROR: Failed to configure PDM input");
  }
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

  delay(1000);
}
