/*
 * PCMD3180 Basic Example
 *
 * This sketch demonstrates basic initialization and configuration
 * of the PCMD3180 PDM-to-PCM converter.
 *
 * Requires a Teensy 4.x with the Teensy Audio library, built with
 * USB type "Dual Serial" (-D USB_DUAL_SERIAL). Status messages are printed
 * on Serial and raw audio frames are streamed on SerialUSB1.
 *
 * Hardware connections:
 * - SDA -> Arduino SDA
 * - SCL -> Arduino SCL
 * - VDD -> 3.3V or regulated supply
 * - GND -> GND
 * - SHDNZ -> pin 10
 * - GPIO1 (IRQ) -> pin 0 or 1
 * - PDM microphones connected to PDMIN pins
 * - I2S/TDM output pins connected to your DSP/MCU
 */

#include <Audio.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Wire.h>
#include "PCMD3180.h"

// Create PCMD3180 instance with SHDNZ on pin 10, default I2C address (0x4C) and internal AREG
PCMD3180 codec(10, PCMD3180_I2C_ADDR_DEFAULT, true);

// Stream channel 1 and 2 to USB
// GUItool: begin automatically generated code
AudioInputTDM            tdm1;           //xy=224,316
AudioAnalyzeRMS          rms1;           //xy=397,218
AudioAnalyzePeak         peak1;          //xy=405,249
AudioRecordQueue         queue1;
AudioRecordQueue         queue2;
AudioRecordQueue         queue3;
AudioRecordQueue         queue4;
AudioRecordQueue         queue5;
AudioRecordQueue         queue6;
AudioRecordQueue         queue7;
AudioRecordQueue         queue8;
AudioConnection          patchCord1(tdm1, 0, rms1, 0);
AudioConnection          patchCord2(tdm1, 0, peak1, 0);
AudioConnection          patchCord3(tdm1, 0, queue1, 0);
AudioConnection          patchCord4(tdm1, 2, queue2, 0);
AudioConnection          patchCord5(tdm1, 4, queue3, 0);
AudioConnection          patchCord6(tdm1, 6, queue4, 0);
AudioConnection          patchCord7(tdm1, 8, queue5, 0);
AudioConnection          patchCord8(tdm1, 10, queue6, 0);
AudioConnection          patchCord9(tdm1, 12, queue7, 0);
AudioConnection          patchCord10(tdm1, 14, queue8, 0);

AudioRecordQueue *queues[8] = {&queue1, &queue2, &queue3, &queue4, &queue5, &queue6, &queue7, &queue8};
uint8_t micIds[8] = {4, 1, 2, 7, 8, 5, 6, 3};

// Sample rates reported by getAutodetectedClocks(), indexed by FSRate.
// The device does not report whether the bus is in the 44.1kHz or 48kHz family.
const char *fsRateNames[] = {"7.35/8", "14.7/16", "22.05/24", "29.4/32", "44.1/48",
                             "88.2/96", "176.4/192", "352.8/384", "705.6/768"};

// GUItool: end automatically generated code

void setup() {
  AudioMemory(50);
  Serial.begin(115200); // Debug messages
  SerialUSB1.begin(12000000); // Raw audio data (high speed)
  while (!Serial) {
    ; // Wait for serial port to connect
  }

  Serial.println("PCMD3180 Basic Example");
  Serial.println("======================");
  Serial.println("Audio data will stream on SerialUSB1");

  // Initialize I2C
  int i2cFrequency = 50000;
  Serial.println("Initializing I2C @ " + String(i2cFrequency) + "Hz");
  Wire.begin();
  Wire.setClock(i2cFrequency); // 50kHz I2C

  // Setup Interrupt pins
  pinMode(0, INPUT); // IRQ1
  pinMode(1, INPUT); // IRQ0


  // Initialize the codec
  Serial.println("Initializing PCMD3180...");
  while (!codec.begin(Wire)) {
    Serial.println("ERROR: Failed to initialize PCMD3180!");
    Serial.println("Check I2C connections and address.");
    delay(100);
  }
  Serial.println("PCMD3180 initialized successfully!");

  Serial.println("Setting Interrupt active High...");
  if (!codec.setInterruptConfig(INT_MODE_ASSERT_CONSTANT, INT_LATCH_MODE_READ_ALL, INT_POLARITY_ACTIVE_HIGH)) {
    Serial.println("ERROR: Failed to set Interrupt active High");
  }

  Serial.println("Unmasking interrupts...");
  if (!codec.setInterruptMasks(false, false)) {
    Serial.println("ERROR: Failed to unmask interrupts");
  }

  Serial.println("Setting GPIO as IRQ output, driven active high and low...");
  if (!codec.setGPIOConfig(GPIO_MODE_IRQ, DRIVE_MODE_ACTIVE_LOW_ACTIVE_HIGH)) {
    Serial.println("ERROR: Failed to configure GPIO as IRQ output");
  }

  // Set the PDM clock, select PDM input on all channels, output PDMCLK on GPO1-4,
  // take PDM data in on GPI1-4, and enable all 8 input and ASI output channels
  Serial.println("Setting up all 8 channels as PDM input...");
  if (!codec.configurePDMInput(PDMCLK_2822_KHZ, 8)) {
    Serial.println("ERROR: Failed to configure PDM input");
  }

  // Map channels to TDM slots matching their physical IDs
  Serial.println("Mapping channels to TDM slots...");
  bool success = true;
  success &= codec.setASISlotAssignment(1, 0);
  success &= codec.setASISlotAssignment(2, 2);
  success &= codec.setASISlotAssignment(3, 4);
  success &= codec.setASISlotAssignment(4, 6);
  success &= codec.setASISlotAssignment(5, 8);
  success &= codec.setASISlotAssignment(6, 10);
  success &= codec.setASISlotAssignment(7, 12);
  success &= codec.setASISlotAssignment(8, 14);
  if (!success) {
    Serial.println("ERROR: Failed to map channels to TDM slots");
  }

  // Set number of biquads to 1 per channel to support 8 channels
  Serial.println("Setting number of biquads per channel to 1...");
  if (!codec.setBiquadConfig(1)) {
    Serial.println("ERROR: Failed to set number of biquads");
  }

  // Run as slave, following the BCLK and FSYNC provided by the Teensy
  Serial.println("Setting slave mode...");
  if (!codec.setMasterMode(MODE_SLAVE)) {
    Serial.println("ERROR: Failed to set slave mode");
  }

  // Set audio format to 16 bits TDM
  Serial.println("Setting audio format to 16-bit TDM...");
  if (!codec.setASIFormat(FORMAT_TDM, WORD_16_BIT)) {
    Serial.println("ERROR: Failed to set audio format");
  }

  // DSP
  // Adjust filter if desired
  Serial.println("Setting filter to linear...");
  if (!codec.setDecimationFilterMode(FILTER_MODE_LINEAR)) {
    Serial.println("ERROR: Failed to set filter mode");
  }


  // Set MICBIAS to AVDD
  Serial.println("Setting MICBIAS to AVDD...");
  if (!codec.configureBias(MICBIAS_MODE_AVDD, VREF_MODE_275V)) {
    Serial.println("ERROR: Failed to set MICBIAS to AVDD");
  }

  // Power up MICBIAS
  Serial.println("Powering up MICBIAS...");
  if (!codec.powerMICBIAS(true)){
    Serial.println("ERROR: Failed to power on MICBIAS");
  }

  // Power up PDM converter
  Serial.println("Powering up PDM converter...");
  if (!codec.powerPDM(true)){
    Serial.println("ERROR: Failed to power on PDM converter");
  }

  // Power up PLL
  Serial.println("Powering up PLL...");
  if (!codec.powerPLL(true)){
    Serial.println("ERROR: Failed to power on PLL");
  }

  // Clear latched interrupts
  bool asiError, pllError;
  codec.getLatchedInterruptStatus(asiError, pllError);

  Serial.println("\n======================");
  Serial.println("Setup complete!");
  Serial.println("PCMD3180 is now capturing PDM audio and outputting via TDM");
  Serial.println("======================\n");

  // Begin recording audio to the queues
  for (uint8_t n=0; n<8; n++) {
    queues[n]->begin();
  }
}

void loop() {
  static unsigned long lastCheck = 0;
  static int lastFsRate = -1;
  static uint32_t lastBclkRatio = 0xFF;
  static uint32_t lastStatus0 = 0xFF;
  static uint32_t lastDeviceMode = 0xFF;
  static float lastRms = 0xFF;
  static float lastPeak = 0xFF;
  static uint8_t syncByte = 0x00;


  // Check if ALL queues have data available
  bool allReady = true;
  for (uint8_t n = 0; n < 8; n++) {
    if (queues[n]->available() < 1) {
      allReady = false;
      break;
    }
  }

  // Only read when ALL channels have data
  if (allReady) {
    int16_t *buffers[8];

    // Read all 8 buffers first (don't free yet)
    for (uint8_t n = 0; n < 8; n++) {
      buffers[n] = queues[n]->readBuffer();
    }

    // Send all channels sequentially (one complete buffer per channel)
    for (uint8_t n = 0; n < 8; n++) {
      SerialUSB1.write(0x88);
      SerialUSB1.write(0x88);
      SerialUSB1.write(micIds[n]);  // Mic ID
      SerialUSB1.write(syncByte);  // Frame sync byte
      SerialUSB1.write((uint8_t*)buffers[n], 128 * sizeof(int16_t));
    }

    // Free all buffers after processing
    for (uint8_t n = 0; n < 8; n++) {
      queues[n]->freeBuffer();
    }

    // Increment frame sync byte
    syncByte += 1;
  }

  if (millis() - lastCheck > 2000) {
    lastCheck = millis();

    // Print status
    uint8_t status0, deviceMode;
    if (codec.getDeviceStatus(status0, deviceMode)) {
      if ((status0 != lastStatus0) || (deviceMode != lastDeviceMode)) {
        lastStatus0 = status0;
        lastDeviceMode = deviceMode;
        Serial.print("Status check - STS0: 0b");
        Serial.print(status0, BIN);
        Serial.print(", STS1: ");

        // getDeviceStatus() already returns DEV_STS1 bits 7:5
        String status;
        switch (deviceMode) {
          case 4:
            status = "Device is in sleep mode or software shutdown mode";
            break;
          case 6:
            status = "Device is in active mode with all PDM channels turned off";
            break;
          case 7:
            status = "Device is in active mode with at least one PDM channel turned on";
            break;
          default:
            status = "Device is in an unknown status: " + String(deviceMode);
        }
        Serial.println(status);
      }
    } else {
      Serial.println("ERROR: Failed to read device status!");
    }

    // Print latched interrupts if IRQ is high
    if (digitalRead(0) | digitalRead(1)){
      Serial.print("Interrupt check - ");
      bool asiError, pllError;
      if (codec.getLatchedInterruptStatus(asiError, pllError)) {
        Serial.print("ASI Error: " + String(asiError));
        Serial.println(", PLL Error: " + String(pllError));
      } else {
        Serial.println("ERROR: Failed to read device status!");
      }
    }

    // Print autodetected ASI bus frequencies if they have changed
    FSRate fsRate;
    uint32_t ratio;
    if (codec.getAutodetectedClocks(fsRate, ratio)){
      if (((fsRate != lastFsRate) || (ratio != lastBclkRatio))) {
        Serial.println("Autodetected FSYNC: " + String(fsRateNames[fsRate]) + "kHz, FSYNC/BCLK ratio: " + String(ratio));
        lastFsRate = fsRate;
        lastBclkRatio = ratio;
      }
    } else {
        Serial.println("ERROR: No autodetected ASI clock frequencies");
    }

    // Check if device is still connected
    if (!codec.isConnected()) {
      Serial.println("WARNING: Device not responding on I2C bus!");
    }

    // Print RMS and Peak info
    if (rms1.available() && peak1.available()) {
      float rms = rms1.read();
      float peak = peak1.read();
      if ((rms != lastRms) || (peak != lastPeak)) {
        Serial.println("RMS1: " + String(log10(rms)) + ", Peak1: " + String(log10(peak)));
        lastRms = rms;
        lastPeak = peak;
      }
    }
  }
}
