/*
 * PCMD3180 Basic Example
 * 
 * This sketch demonstrates basic initialization and configuration
 * of the PCMD3180 PDM-to-PCM converter.
 * 
 * Hardware connections:
 * - SDA -> Arduino SDA
 * - SCL -> Arduino SCL
 * - VDD -> 3.3V or regulated supply
 * - GND -> GND
 * - PDM microphones connected to PDMIN pins
 * - I2S/TDM output pins connected to your DSP/MCU
 */

#include <Audio.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Wire.h>
#include "PCMD3180.h"

// Create PCMD3180 instance with default I2C address (0x4C)
PCMD3180 codec(10, PCMD3180_I2C_ADDR_DEFAULT, true);

// Stream channel 1 and 2 to USB
// GUItool: begin automatically generated code
AudioInputTDM            tdm_1;         //xy=224,316
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
AudioConnection          patchCord1(tdm_1, 0, rms1, 0);
AudioConnection          patchCord2(tdm_1, 0, peak1, 0);
AudioConnection          patchCord3(tdm_1, 0, queue1, 0);
AudioConnection          patchCord4(tdm_1, 2, queue2, 0);
AudioConnection          patchCord5(tdm_1, 4, queue3, 0);
AudioConnection          patchCord6(tdm_1, 6, queue4, 0);
AudioConnection          patchCord7(tdm_1, 8, queue5, 0);
AudioConnection          patchCord8(tdm_1, 10, queue6, 0);
AudioConnection          patchCord9(tdm_1, 12, queue7, 0);
AudioConnection          patchCord10(tdm_1, 14, queue8, 0);

AudioRecordQueue *queues[8] = {&queue1, &queue2, &queue3, &queue4, &queue5, &queue6, &queue7, &queue8};
uint8_t mic_ids[8] = {4, 1, 2, 7, 8, 5, 6, 3};

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
  int i2cf = 50000;
  Serial.println("Initializing I2C @ " + String(i2cf) + "Hz");
  Wire.begin();
  Wire.setClock(i2cf); // 50kHz I2C
  
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
  if (!codec.setInterruptActiveHigh(true)){
      Serial.println("ERROR: Failed to set Interrupt active High");
  }

  Serial.println("Unmasking interrupts...");
  if (!codec.writeRegister(PCMD3180_INT_MASK0, 0x00)){
      Serial.println("ERROR: Failed to unmask interrupts");
  }

  Serial.println("Setting Interrupt drive active high and low...");
  uint8_t mask = 0x03;
  if (!codec.updateRegisterBits(PCMD3180_GPIO_CFG0, mask, 0x01)){
      Serial.println("ERROR: Failed to set Interrupt active High");
  }

  Serial.println("Setting up all ports as PDM input...");
  for (uint8_t n = 1; n < 5; n++){
    // Enable ports 1-4 
    if (!codec.enablePort(n, true)) {
      Serial.println("ERROR: Failed to enable port " + n);
    }
    
    // Configure their GPO as PDMCLK
    if (!codec.setGPOMode(n, GPO_MODE_PDM)){
      Serial.println("ERROR: Failed to set GPO mode to PDM for port " + n);
    }

    // Set their GPI as PDMDIN1 to PDMDIN4
    GPIMode gpi_mode;
    switch (n) { 
      case 1:
        gpi_mode = GPI_MODE_PDMDIN1;
        break;
      case 2:
        gpi_mode = GPI_MODE_PDMDIN2;
        break;
      case 3:
        gpi_mode = GPI_MODE_PDMDIN3;
        break;
      case 4:
        gpi_mode = GPI_MODE_PDMDIN4;
        break;
      default:
        Serial.println("ERROR: Invalid port number " + String(n));
    }
    if (!codec.setGPIMode(n, gpi_mode)){
      Serial.println("ERROR: Failed to set GPI mode to PDMIN" + String(n) + " for port " + String(n));
    }
  }

  // Enable all 8 input channels
  Serial.println("Enabling all 8 input channels...");
  if (!codec.enableChannels(0xFF)) {
    Serial.println("ERROR: Failed to enable input channels");
  }

  // Enable all 8 output channels on ASI (Audio Serial Interface)
  Serial.println("Enabling all 8 output channels on ASI...");
  if (!codec.enableOutputASIChannels(0xFF)) {
    Serial.println("ERROR: Failed to enable output channels");
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
  Serial.println("Setting number of biquads per channel to 2...");
  if (!codec.setBiquadCfg(1)) {
    Serial.println("ERROR: Failed to set number of biquads");
  }

  // Set audio format to 16 bits TDM
  Serial.println("Setting audio format to 16-bit TDM...");
  if (!codec.setASIFormat(FORMAT_TDM, WORD_16_BIT, MODE_SLAVE)) {
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
  if (!codec.configureBIAS(MICBIAS_MODE_AVDD, VREF_MODE_275V)) {
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
  bool asi_err, pll_error;
  codec.getLatchedInterruptStatus(asi_err, pll_error);

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
  static uint32_t last_fsync = 0xFF;
  static uint32_t last_blck_ratio = 0xFF;
  static uint32_t last_STS0 = 0xFF;
  static uint32_t last_STS1 = 0xFF;
  static float last_rms = 0xFF;
  static float last_peak = 0xFF;
  static uint8_t sync_byte = 0x00;
  static bool cleared = false;


  // Check if ALL queues have data available
  bool all_ready = true;
  for (uint8_t n = 0; n < 8; n++) {
    if (queues[n]->available() < 1) {
      all_ready = false;
      break;
    }
  }
  
  // Only read when ALL channels have data
  if (all_ready) {
    int16_t *buffers[8];
    
    // Read all 8 buffers first (don't free yet)
    for (uint8_t n = 0; n < 8; n++) {
      buffers[n] = queues[n]->readBuffer();
    }
    
    // Send all channels sequentially (one complete buffer per channel)
    for (uint8_t n = 0; n < 8; n++) {
      SerialUSB1.write(0x88);
      SerialUSB1.write(0x88);
      SerialUSB1.write(mic_ids[n]);  // Mic ID
      SerialUSB1.write(sync_byte);  // Frame sync byte
      SerialUSB1.write((uint8_t*)buffers[n], 128 * sizeof(int16_t));
    }
    
    // Free all buffers after processing
    for (uint8_t n = 0; n < 8; n++) {
      queues[n]->freeBuffer();
    }

    // Increment frame sync byte
    sync_byte += 1;
  }

  if (millis() - lastCheck > 2000) {
    lastCheck = millis();
    
    // Print status 
    uint8_t status0, status1;
    if (codec.getDeviceStatus(status0, status1)) {
      if ((status0 != last_STS0) || (status1 != last_STS1)) {
        last_STS0 = status0;
        last_STS1 = status1;
        Serial.print("Status check - STS0: 0b");
        Serial.print(status0, BIN);
        Serial.print(", STS1: ");
        
        String status;
        status1 >>= 5;
        switch (status1) {
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
            status = "Device is in an unknown status: " + String(status1);
        }
        Serial.println(status);
      }
    } else {
      Serial.println("ERROR: Failed to read device status!");
    }
    
    // Print latched interrupts if IRQ is high
    if (digitalRead(0) | digitalRead(1)){
      Serial.print("Interrupt check - ");
      bool asi_err, pll_error;
      if (codec.getLatchedInterruptStatus(asi_err, pll_error)) {
        Serial.print("ASI Error: " + String(asi_err));
        Serial.println(", PLL Error: " + String(pll_error));
      } else {
        Serial.println("ERROR: Failed to read device status!");
      }
    }

    // Print autodetected ASI bus frequencies if they have changed
    uint32_t fsync, ratio;
    if (codec.getAutodetectedClocks(fsync, ratio)){
      if (((fsync != last_fsync) || (ratio != last_blck_ratio))) {
        Serial.println("Autodetected FSYNC: " + String(fsync/1000) + "kHz, FSYNC/BCLK ratio: " + String(ratio));
        last_fsync = fsync;
        last_blck_ratio = ratio;
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
      if ((rms != last_rms) || (peak != last_peak)) {
        Serial.println("RMS1: " + String(log10(rms)) + ", Peak1: " + String(log10(peak))); 
        last_rms = rms;
        last_peak = peak;
      }
    }
  }
}
