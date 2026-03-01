/*
 * PCMD3180 Arduino Library
 *
 * The PCMD3180 is a multi-channel PDM-to-PCM converter with I2C control
 */

#ifndef PCMD3180_H
#define PCMD3180_H

#include <Arduino.h>
#include <Wire.h>

// Default I2C address (can be configured via hardware pins)
#define PCMD3180_I2C_ADDR_DEFAULT 0x4C

// Default SHDNZ pin is no pin, assumed to be set by an external source
#define PCMD3180_SHDNZ_PIN_DEFAULT 0xFF

// ASI_CFG0 bits [7:6]
enum AudioFormat {
  FORMAT_TDM,             // 0x00  Time-division multiplexed
  FORMAT_I2S,             // 0x40  Standard I2S; data offset one BCLK from FSYNC
  FORMAT_LEFT_JUSTIFIED,  // 0x80  MSB on first BCLK after FSYNC
};

// ASI_CFG0 bits [5:4]
enum WordLength {
  WORD_16_BIT,  // 0x00
  WORD_20_BIT,  // 0x10
  WORD_24_BIT,  // 0x20
  WORD_32_BIT   // 0x30
};

// ASI_CFG0: FSYNCINV bit [3] or BCLKINV bit [2] depending on which argument is being set
enum ASIPolarity {
  ASI_POLARITY_STANDARD,  // bit clear
  ASI_POLARITY_INVERTED   // bit set
};

// PDMCLK_CFG bits [1:0]  (bit 6 is always set to enable the clock output)
enum PDMClock {
  PDMCLK_2822_KHZ,  // 0x00  2.8224 MHz
  PDMCLK_1411_KHZ,  // 0x01  1.4112 MHz
  PDMCLK_705_KHZ,   // 0x02  705.6 kHz
  PDMCLK_5644_KHZ   // 0x03  5.6448 MHz
};

// MST_CFG1 bits [7:4]
enum FSRate {
  FSRATE_8,    // 0x00   8 kHz
  FSRATE_16,   // 0x10  16 kHz
  FSRATE_24,   // 0x20  24 kHz
  FSRATE_32,   // 0x30  32 kHz
  FSRATE_48,   // 0x40  48 kHz
  FSRATE_96,   // 0x50  96 kHz
  FSRATE_192,  // 0x60  192 kHz
  FSRATE_384,  // 0x70  384 kHz
  FSRATE_768   // 0x80  768 kHz
};

// MST_CFG1 bits [3:0]
// Note: ratio must be >= 2 * word_length * num_channels for TDM, or >= 2 * word_length for I2S/LJ
enum BCLKRatio {
  BCLKRATIO_16,    // 0x00
  BCLKRATIO_24,    // 0x01
  BCLKRATIO_32,    // 0x02
  BCLKRATIO_48,    // 0x03
  BCLKRATIO_64,    // 0x04
  BCLKRATIO_96,    // 0x05
  BCLKRATIO_128,   // 0x06
  BCLKRATIO_192,   // 0x07
  BCLKRATIO_256,   // 0x08
  BCLKRATIO_384,   // 0x09
  BCLKRATIO_512,   // 0x0A
  BCLKRATIO_1024,  // 0x0B
  BCLKRATIO_2048,  // 0x0C
};

// CLK_SRC bit [7]
enum PLLSlaveClockSource {
  PLLSLAVECLKSRC_BCLK,  // 0x00
  PLLSLAVECLKSRC_MCLK   // 0x80
};

// MST_CFG0 bit [7]
enum MasterMode {
  MODE_SLAVE,   // 0x00  Follows externally supplied BCLK and FSYNC
  MODE_MASTER   // 0x80  Device generates BCLK and FSYNC from its internal PLL
};

// MST_CFG0 bit [3]
enum FSMode {
  FSYNC_MODE_48000,  // 0x00  FSYNC is a multiple of 8 kHz   (8/16/32/48/96/192/384/768 kHz)
  FSYNC_MODE_44100   // 0x08  FSYNC is a multiple of 7.35 kHz (7.35/14.7/22.05/44.1/88.2/176.4 kHz)
};

// GPIO_CFG0 bits [7:4]
enum GPIOMode {
  GPIO_MODE_DISABLED,   // 0x00
  GPIO_MODE_GPO,        // 0x10  General-purpose output
  GPIO_MODE_IRQ,        // 0x20  Interrupt output
  GPIO_MODE_SDOUT2,     // 0x30  Secondary ASI data output
  GPIO_MODE_PDMCLK,     // 0x40  PDM clock output
  GPIO_MODE_MICBIAS_EN, // 0x80  MICBIAS enable output
  GPIO_MODE_GPI,        // 0x90  General-purpose input
  GPIO_MODE_MCLK,       // 0xA0  Master clock input
  GPIO_MODE_SDIN,       // 0xB0  ASI data input (daisy-chain)
  GPIO_MODE_PDMDIN1,    // 0xC0  PDM data input port 1
  GPIO_MODE_PDMDIN2,    // 0xD0  PDM data input port 2
  GPIO_MODE_PDMDIN3,    // 0xE0  PDM data input port 3
  GPIO_MODE_PDMDIN4     // 0xF0  PDM data input port 4
};

// GPIO_CFG0 bits [2:0] / GPO_CFGx bits [2:0]
// Note: GPO ports only support HI_Z and ACTIVE_LOW_ACTIVE_HIGH
enum DRIVEMode {
  DRIVE_MODE_HI_Z,                    // 0x00  Always high-impedance
  DRIVE_MODE_ACTIVE_LOW_ACTIVE_HIGH,  // 0x01  Push-pull
  DRIVE_MODE_ACTIVE_LOW_WEAK_HIGH,    // 0x02  Open-drain with weak pull-up (GPIO only)
  DRIVE_MODE_ACTIVE_LOW_HI_Z,         // 0x03  Actively drives low, hi-Z when high (GPIO only)
  DRIVE_MODE_WEAK_LOW_ACTIVE_HIGH,    // 0x04  Weak pull-down, actively drives high (GPIO only)
  DRIVE_MODE_HI_Z_ACTIVE_HIGH         // 0x05  Hi-Z when low, actively drives high (GPIO only)
};

// GPO_CFG0..3 bits [6:4]  (one register per port)
enum GPOMode {
  GPO_MODE_DISABLED,  // 0x00
  GPO_MODE_GPO,       // 0x10  General-purpose output
  GPO_MODE_IRQ,       // 0x20  Interrupt output
  GPO_MODE_ASI,       // 0x30  ASI data output
  GPO_MODE_PDM        // 0x40  PDM clock output
};

// GPI_CFG0 (ports 1-2) and GPI_CFG1 (ports 3-4); odd ports bits [6:4], even ports bits [2:0]
enum GPIMode {
  GPI_MODE_DISABLED,  // 0x00
  GPI_MODE_GPI,       // 0x01  General-purpose input
  GPI_MODE_MCLK,      // 0x02  Master clock input
  GPI_MODE_ASI,       // 0x03  ASI data input
  GPI_MODE_PDMDIN1,   // 0x04  PDM data input, microphone pair ch1+ch2
  GPI_MODE_PDMDIN2,   // 0x05  PDM data input, microphone pair ch3+ch4
  GPI_MODE_PDMDIN3,   // 0x06  PDM data input, microphone pair ch5+ch6
  GPI_MODE_PDMDIN4    // 0x07  PDM data input, microphone pair ch7+ch8
};

// BIAS_CFG bits [6:5]
enum MICBIASmode {
  MICBIAS_MODE_VREF,  // 0x00  MICBIAS derived from internal VREF
  MICBIAS_MODE_AVDD   // 0x60  MICBIAS tied directly to AVDD
};

// Not written to a register directly; selects the automatic VREF setting applied during wakeUp()
enum AVDDInputVoltage {
  AVDD_INPUT_18V,  // VREF automatically set to 1.375 V (BIAS_CFG bits [1:0] = 0x02)
  AVDD_INPUT_33V   // VREF left at default 2.75 V
};

// BIAS_CFG bits [1:0]
// Note: VREF must be below AVDD; use VREF_MODE_1375V when AVDD = 1.8 V
enum VREFmode {
  VREF_MODE_275V,   // 0x00  2.75 V  (default)
  VREF_MODE_25V,    // 0x01  2.5 V
  VREF_MODE_1375V   // 0x02  1.375 V
};

// ASI_CH1..8 bit [6]
// Note: ASIPIN_SECONDARY requires the GPIO pin to be configured as GPIO_MODE_SDOUT2
enum ASIPIN {
  ASIPIN_PRIMARY,    // 0x40  Primary SDOUT pin
  ASIPIN_SECONDARY   // 0x00  Secondary SDOUT (GPIO pin)
};

// INT_CFG bits [6:5]
enum INTMode {
  INT_MODE_ASSERT_CONSTANT,    // 0x00  Pin stays asserted while condition is active
  INT_MODE_ASSERT_2MS_PULSED,  // 0x20  Pulses 2 ms on each event
  INT_MODE_ASSERT_2MS_ONCE     // 0x60  Pulses once for 2 ms then de-asserts until cleared
};

// INT_CFG bit [2]
enum INTLatchReadMode {
  INT_LATCH_MODE_READ_ALL,            // 0x00  INT_LTCH0 returns all latched flags
  INT_LATCH_MODE_READ_ONLY_UNMASKED   // 0x04  INT_LTCH0 returns only unmasked flags
};

// INT_CFG bit [7]
enum INTPolarity {
  INT_POLARITY_ACTIVE_LOW,   // 0x00
  INT_POLARITY_ACTIVE_HIGH   // 0x80
};

// DSP_CFG0 bits [5:4]
enum FILTERmode {
  FILTER_MODE_LINEAR,             // 0x00  Best stop-band attenuation, highest latency
  FILTER_MODE_LOW_LATENCY,        // 0x10  Reduced latency, moderate attenuation
  FILTER_MODE_ULTRA_LOW_LATENCY   // 0x20  Minimum latency, lowest attenuation
};

// DSP_CFG0 bits [3:2]
enum SUMMATIONmode {
  SUMMATION_MODE_DISABLED,  // 0x00  Channels are independent
  SUMMATION_MODE_2CHAN,      // 0x04  Adjacent pairs summed: ch1+ch2 → ch1, ch3+ch4 → ch3, ...
  SUMMATION_MODE_4CHAN       // 0x08  Groups of four summed: ch1+ch2+ch3+ch4 → ch1, ...
};

// DSP_CFG0 bits [1:0]
enum HPFILTERmode {
  HP_FILTER_MODE_CUSTOM,  // 0x00  User-defined corner via biquad coefficients
  HP_FILTER_MODE_12HZ,    // 0x01  ~12 Hz
  HP_FILTER_MODE_96HZ,    // 0x02  ~96 Hz
  HP_FILTER_MODE_384HZ    // 0x03  ~384 Hz
};

// PDMIN_CFG: one bit per port (bit 7 = port 1, bit 6 = port 2, ...)
// Each PDM port shares one data line between two mics; one is latched on rising, one on falling edge
enum EDGELatchMode {
  EDGE_MODE_EVEN_NEGATIVE,  // 0  Even-numbered channel latched on falling edge
  EDGE_MODE_EVEN_POSITIVE   // 1  Even-numbered channel latched on rising edge
};

// SLEEP_CFG bits [4:3]
// Longer durations give a more stable reference but increase wake-up time
enum VREFChargeDuration {
  VREF_CHARGE_DURATION_35,   // 0x00   3.5 ms
  VREF_CHARGE_DURATION_10,   // 0x08  10 ms
  VREF_CHARGE_DURATION_50,   // 0x10  50 ms
  VREF_CHARGE_DURATION_100,  // 0x18  100 ms
};

// SHDN_CFG bits [3:2]
enum SHUTDOWNMode {
  SHUTDOWN_DREG_IMMEDIATELY,           // 0x00  DREG powers off immediately
  SHUTDOWN_DREG_ACTIVE_UNTIL_TIMOUT,   // 0x04  DREG stays active until DREG timeout
  SHUTDOWN_DREG_ACTIVE_UNTIL_SHUTDOWN  // 0x08  DREG stays active until supply falls below threshold
};

// SHDN_CFG bits [1:0]
// Note: register encoding is inverted (5 ms = 0x03, 30 ms = 0x00)
enum DREGActiveTime {
  DREG_ACTIVE_TIME_5,   // 0x03   5 ms
  DREG_ACTIVE_TIME_10,  // 0x02  10 ms
  DREG_ACTIVE_TIME_25,  // 0x01  25 ms
  DREG_ACTIVE_TIME_30   // 0x00  30 ms
};

// PWR_CFG bits [3:2]
enum DYNAMICPOWERMode {
  DYNAMIC_POWER_ENABLE_CH1_TO_CH2,  // 0x00
  DYNAMIC_POWER_ENABLE_CH1_TO_CH4,  // 0x04
  DYNAMIC_POWER_ENABLE_CH1_TO_CH6,  // 0x08
  DYNAMIC_POWER_ENABLE_ALL_CH,      // 0x0C
};

class PCMD3180 {
public:
  /** Constructor for PCMD3180 device
   * @param shdnz_pin GPIO pin for hardware shutdown control (0xFF if not used)
   * @param i2c_addr I2C address of the device (default 0x4C)
   * @param areg_internal True for internal AREG, false for external
   * @param avdd_input_voltage Voltage being provided to the AVDD input pin. 
   *        If AVDD_INPUT_18V, VREF will automatically be set to 1.375V
   * @note This only stores parameters. Actual hardware initialization is done
   *       in begin(), which reads DEV_STS0 and configures AREG/VREF (see
   *       datasheet section "Device Initialization" and register
   *       PCMD3180_DEV_STS0).
   */
  PCMD3180(uint8_t shdnz_pin = PCMD3180_SHDNZ_PIN_DEFAULT, 
           uint8_t i2c_addr = PCMD3180_I2C_ADDR_DEFAULT,
           bool areg_internal = false,
           AVDDInputVoltage avdd_input_voltage = AVDD_INPUT_33V);
  
  /** Initialize the PCMD3180 device and perform basic setup
   * @param wirePort TwoWire interface to use (default Wire)
   * @return True if initialization successful, false otherwise
   * @note begins by checking I2C connectivity, performing a soft reset
   *       (PCMD3180_SW_RESET) and waking the part. Refer to the datasheet
   *       sections "Software Reset" and "Sleep/Wake Configuration" for
   *       timing requirements and the use of PCMD3180_SLEEP_CFG.
   */
  bool begin(TwoWire &wirePort = Wire);
  
  /** Perform software reset of the PCMD3180 device
   * @return True if reset successful, false otherwise
   * @note Writes 0x01 to PCMD3180_SW_RESET register. See datasheet section
   *       "Software Reset" for details on required delay after reset.
   */
  bool reset();
  
  // -------------------------------------------------------------------------
  // Utility methods — use these for typical setups
  // -------------------------------------------------------------------------

  /** Configure the device as an I2S/TDM slave (host provides BCLK and FSYNC)
   * @param format Audio format (TDM, I2S, or Left Justified)
   * @param word_len Word length (16, 20, 24, or 32 bits)
   * @param num_channels Number of channels to enable (1-8)
   * @return True if successful, false otherwise
   * @note Enables auto clock configuration and PLL so the device locks to
   *       whatever BCLK/FSYNC the host drives.  Call configurePDMInput() and
   *       powerPDM() afterwards to complete setup.
   */
  bool configureAsSlave(AudioFormat format, WordLength word_len, uint8_t num_channels);

  /** Configure the device as an I2S/TDM master (device drives BCLK and FSYNC)
   * @param format Audio format (TDM, I2S, or Left Justified)
   * @param word_len Word length (16, 20, 24, or 32 bits)
   * @param fsync_rate FSYNC sample rate
   * @param bclk_ratio BCLK-to-FSYNC ratio
   * @param num_channels Number of channels to enable (1-8)
   * @return True if successful, false otherwise
   * @note Sets master mode and programs the ASI clock registers.  Call
   *       configurePDMInput() and powerPDM() afterwards to complete setup.
   */
  bool configureAsMaster(AudioFormat format, WordLength word_len, FSRate fsync_rate, BCLKRatio bclk_ratio, uint8_t num_channels);

  /** Configure PDM clock and enable the required input ports and channels
   * @param clk PDM clock frequency
   * @param num_channels Number of channels to enable (1-8); ports are enabled
   *        in pairs (port 1 = ch1+2, port 2 = ch3+4, etc.)
   * @return True if successful, false otherwise
   * @note Calls setPDMClock(), enables the needed ports, and calls
   *       enableAllChannels().  Does not power up the PDM block — call
   *       powerPDM(true) after this.
   */
  bool configurePDMInput(PDMClock clk, uint8_t num_channels);

  /** Enable a contiguous set of input and ASI output channels by count
   * @param num_channels Number of channels to enable, starting from channel 1
   * @return True if successful, false otherwise
   * @note Builds the correct bitmask for both IN_CH_EN and ASI_OUT_CH_EN
   *       so you don't have to compute it manually.
   */
  bool enableAllChannels(uint8_t num_channels);

  /** Set the same digital volume on all eight channels
   * @param volume Volume value (0x00 = mute, 0x7F = unity gain, 0xFF = +24dB)
   * @return True if successful, false otherwise
   * @note Calls setDigitalVolume() for each channel in sequence.  Stops and
   *       returns false immediately if any individual write fails.
   */
  bool setAllChannelVolumes(uint8_t volume);

  // -------------------------------------------------------------------------

  /** Enable or disable I2C broadcast mode for the device
   * @param enable True to enable broadcast, false to disable
   * @return True if successful, false otherwise
   * @note Controlled via the I2C_CKSUM register bit described in the
   *       datasheet section "I2C Interface"; enabling broadcast allows
   *       multiple parts to share the same address during configuration.
   */
  bool setI2CBroadcast(bool enable);

  /** Configure ASI audio format and word length
   * @param format Audio format (TDM, I2S, or Left Justified)
   * @param wordLen Word length (16, 20, 24, or 32 bits)
   * @return True if successful, false otherwise
   * @note This writes to PCMD3180_ASI_CFG0 register bits 7:6 (format) and
   *       5:4 (word length). See datasheet section "ASI Interface – Format"
   *       for details and valid combinations.
   */
  bool setASIFormat(AudioFormat format, WordLength wordLen);
  
  /** Configure FSYNC and BCLK polarity for ASI interface
   * @param fsyncPolarity FSYNC polarity (standard or inverted)
   * @param blckPolarity BCLK polarity (standard or inverted)
   * @return True if successful, false otherwise
   * @note Polarity bits are in PCMD3180_ASI_CFG0 (FSYNCINV_BIT and
   *       BCLKINV_BIT). Details are in datasheet section "ASI Interface –
   *       Clock Polarity".
   */
  bool setASIPolarities(ASIPolarity fsyncPolarity, ASIPolarity blckPolarity);
  
  /** Configure ASI error detection and auto-recovery settings
   * @param enableErrorDetection True to enable error detection
   * @param enableAutoResumeOnRecovery True to auto-resume after error recovery
   * @return True if successful, false otherwise
   * @note These options are in PCMD3180_ASI_CFG1. Refer to the datasheet
   *       subsection "ASI Error Detection" for how the device behaves when
   *       invalid ASI frames are detected.
   */
  bool setASIErrorDetection(bool enableErrorDetection, bool enableAutoResumeOnRecovery);
  
  /** Enable or disable ASI daisy-chain mode
   * @param enableDaisyChain True to enable daisy-chain, false to disable
   * @return True if successful, false otherwise
   * @note The daisy‑chain enable bit resides in PCMD3180_ASI_CFG1. See the
   *       datasheet section "ASI Daisy-Chaining" for wiring and timing
   *       guidance when multiple converters are chained.
   */
  bool setASIDaisyChained(bool enableDaisyChain);

  /** Set device to master or slave mode
   * @param masterMode MODE_SLAVE for slave mode, MODE_MASTER for master mode
   * @return True if successful, false otherwise
   * @note Sets MASTER_BIT in PCMD3180_MST_CFG0. See section "Master/Slave
   *       Operation" of the datasheet for the consequences on clocking and
   *       ASI timing.
   */
  bool setMasterMode(MasterMode masterMode);
  
  /** Configure master mode settings
   * @param enableGatedFsyncAndBclk True to gate FSYNC and BCLK
   * @param fsMode FSYNC mode (48kHz or 44.1kHz based)
   * @return True if successful, false otherwise
   * @note Gating and FS mode bits are found in PCMD3180_MST_CFG0 (see
   *       datasheet section "Master Configuration"). Gated clocks are
   *       typically used in power‑saving applications.
   */
  bool setMasterConfig(bool enableGatedFsyncAndBclk, FSMode fsMode);

  /** Enable or disable automatic clock configuration in slave mode
   * @param auto_clock_config_enabled True to enable auto clock config
   * @return True if successful, false otherwise
   * @note This toggles the AC_CLK_EN bit in PCMD3180_MST_CFG1. Refer to the
   *       datasheet section "Automatic Clock Configuration" for how the
   *       device detects and locks to incoming BCLK/FSYNC.
   */
  bool setAutoClockConfigEnabled(bool auto_clock_config_enabled);
  
  /** Enable or disable PLL in slave mode auto-clock configuration
   * @param PLL_enabled True to enable PLL, false to disable
   * @return True if successful, false otherwise
   * @note Controls the PLL_EN bit inside PCMD3180_MST_CFG1.  See the
   *       "Auto Clock PLL" subsection of the datasheet for supported
   *       configurations when using an external PLL clock.
   */
  bool setAutoclockPLLEnabled(bool PLL_enabled);
  
  /** Select clock source when PLL is disabled in slave mode
   * @param clk_source BCLK or MCLK as clock source
   * @return True if successful, false otherwise
   * @note This writes the CLK_SRC register (PCMD3180_CLK_SRC).  See the
   *       "Clock Source Selection" section of the datasheet for details
   *       on supporting BCLK vs MCLK inputs.
   */
  bool setDisabledPLLClockSource(PLLSlaveClockSource clk_source);

  /** Configure ASI bus clock in master mode
   * @param fsync_rate FSYNC sample rate (8kHz to 768kHz)
   * @param bclk_ratio BCLK to FSYNC ratio (16 to 2048)
   * @return True if successful, false otherwise
   * @note Writes to PCMD3180_ASI_CFG1/ASI_CFG2 to program FSYNC rate and
   *       BCLK ratio. Consult the datasheet table "ASI Clock Rates" for
   *       valid combinations and required PLL settings.
   */
  bool setASIClock(FSRate fsync_rate, BCLKRatio bclk_ratio);
  
  /** Configure PDM clock frequency
   * @param clk PDM clock frequency (2.8224, 1.4112, 705.6 kHz, or 5.6448 MHz)
   * @return True if successful, false otherwise
   * @note The value is written to PCMD3180_PDMCLK_CFG register.  See the
   *       datasheet section "PDM Clocking" for recommended clock sources and
   *       maximum frequency limits.
   */
  bool setPDMClock(PDMClock clk);

  /** Configure GPIO pin function and drive mode
   * @param mode GPIO function mode (GPO, IRQ, PDMCLK, etc.)
   * @param drive_mode Drive mode (Hi-Z, active push-pull, etc.)
   * @return True if successful, false otherwise
   * @note Writes to PCMD3180_GPIO_CFG0.  See datasheet section "GPIO
   *       Configuration" for the list of available modes and drive strength
   *       settings.
   */
  bool setGPIOConfig(GPIOMode mode, DRIVEMode drive_mode);
  
  /** Set GPIO output value (for GPO mode)
   * @param drive_high True to drive high, false to drive low
   * @return True if successful, false otherwise
   * @note Uses PCMD3180_GPO_VAL register when the GPIO pin is configured as a
   *       general-purpose output. See datasheet "GPIO Output" section.
   */
  bool setGPIOVal(bool drive_high);
  
  /** Set GPO port output value
   * @param port GPO port number (1-4)
   * @param drive_high True to drive high, false to drive low
   * @return True if successful, false otherwise
   * @note The four GPO outputs are controlled by bits 0-3 of
   *       PCMD3180_GPO_VAL. See datasheet table "GPO Pin Assignment".
   */
  bool setGPOVal(uint8_t port, bool drive_high);

  /** Read GPIO input monitor value
   * @param monitor_value Reference to store the GPIO input state (0 or 1)
   * @return True if successful, false otherwise
   * @note Reads the GPIO_MON register.  This pin must be configured as an
   *       input in GPIO_CFG0 before monitoring; see the datasheet section
   *       "GPIO Input Monitoring".
   */
  bool getGPIOMonitorValue(uint8_t &monitor_value);
  
  /** Read GPI input monitor value
   * @param port GPI port number (1-4)
   * @param monitor_value Reference to store the GPI input state (0 or 1)
   * @return True if successful, false otherwise
   * @note Reads PCMD3180_GPI_MON; the port number selects the bit to return.
   *       Refer to "GPI Pin Monitoring" in the datasheet for timing
   *       considerations.
   */
  bool getGPIMonitorValue(uint8_t port, uint8_t &monitor_value);

  /** Configure GPO port function and drive mode
   * @param port GPO port number (1-4)
   * @param mode GPO function mode (disabled, GPO, IRQ, ASI, PDM)
   * @param drive_mode Drive mode for the output
   * @return True if successful, false otherwise
   * @note Each port has its own register (PCMD3180_GPO_CFG0-3).  See the
   *       datasheet tables under "GPO Configuration" for the effect of each
   *       mode, including ASI and PDM output options.
   */
  bool setGPOMode(uint8_t port, GPOMode mode, DRIVEMode drive_mode);
  
  /** Configure GPI port function
   * @param port GPI port number (1-4)
   * @param mode GPI function mode (disabled, GPI, MCLK, ASI, PDM)
   * @return True if successful, false otherwise
   * @note Uses PCMD3180_GPI_CFG0/1 registers.  Refer to datasheet section
   *       "GPI Configuration" for descriptions of each mode and which input
   *       signals they correspond to.
   */
  bool setGPIMode(uint8_t port, GPIMode mode);
  
  /** Enable or disable PDM input ports
   * @param ch_mask Bit mask for ports 1-4 (bit 0 = port 1)
   * @param enable True to enable, false to disable
   * @return True if successful, false otherwise
   * @note Modifies PCMD3180_CHx_CFG0 registers' INSRC field.  See datasheet
   *       section "PDM Input Selection" for port enabling and channel
   *       assignment rules.
   */
  bool enablePort(uint8_t ch_mask, bool enable);
  
  /** Enable or disable input channels
   * @param ch_mask Bit mask for channels 1-8 (bit 7 = Ch1, bit 0 = Ch8)
   * @return True if successful, false otherwise
   * @note Writes to PCMD3180_IN_CH_EN register.  See datasheet table
   *       "Input Channel Enable" for mapping between mask bits and physical
   *       channel numbers.
   */
  bool enableChannels(uint8_t ch_mask);
  
  /** Enable or disable ASI output channels
   * @param ch_mask Bit mask for channels 1-8 (bit 7 = Ch1, bit 0 = Ch8)
   * @return True if successful, false otherwise
   * @note Uses PCMD3180_ASI_OUT_CH_EN register.  Consult the datasheet
   *       section "ASI Output Channel Enable" for how this interacts with
   *       TDM slot assignments.
   */
  bool enableOutputASIChannels(uint8_t ch_mask);
  
  /** Assign input channel to ASI output slot
   * @param channel Input channel number (1-8)
   * @param slot ASI output slot (0-63)
   * @return True if successful, false otherwise
   * @note Each channel has its own ASI_CHx register (PCMD3180_ASI_CH1..8)
   *       holding the slot number. Refer to the datasheet's "ASI Slot
   *       Assignment" section for the slot numbering scheme.
   */
  bool setASISlotAssignment(uint8_t channel, uint8_t slot);
  
  /** Set ASI output line for a channel
   * @param channel Channel number (1-8)
   * @param asi_pin Primary or secondary ASI output line
   * @return True if successful, false otherwise
   * @note Controlled via bit 7 of the channel's ASI_CHx register. See the
   *       datasheet subsection "ASI Output Line Selection" for when the
   *       secondary line is required.
   */
  bool setASIOutputLine(uint8_t channel, ASIPIN asi_pin);
  
  /** Configure PDM input latch edge mode
   * @param port PDM port number (1-4)
   * @param mode Rising (even positive) or falling (even negative) edge
   * @return True if successful, false otherwise
   * @note This affects the PDM edge config bits in PCMD3180_CHx_CFG0.  See
   *       datasheet section "PDM Input Timing" for explanation of even/odd
   *       port pair behavior.
   */
  bool setPortEdgeLatchMode(uint8_t port, EDGELatchMode mode);
  
  /** Set digital volume for a channel
   * @param channel Channel number (1-8)
   * @param volume Volume value (0-255, where 0x7F is unity gain)
   * @return True if successful, false otherwise
   * @note Volume register is PCMD3180_CHx_CFG3 bits 7:0.  See datasheet
   *       section "Digital Volume Control" which explains the coding and
   *       how additional gain changes occur when you hit the limits.
   */
  bool setDigitalVolume(uint8_t channel, uint8_t volume);
  
  /** Set gain calibration for a channel
   * @param channel Channel number (1-8)
   * @param calibration Gain calibration value (0-15)
   * @return True if successful, false otherwise
   * @note Calibration lives in PCMD3180_CHx_CFG2 bits 3:0.  See the
   *       datasheet section "Gain Calibration" for recommended usage when
   *       matching channels.
   */
  bool setGainCalibration(uint8_t channel, uint8_t calibration);
  
  /** Set phase calibration for a channel
   * @param channel Channel number (1-8)
   * @param phase_calibration Phase calibration value (0-255)
   * @return True if successful, false otherwise
   * @note The phase value is held in PCMD3180_CHx_CFG2 bits 7:0.  See the
   *       datasheet "Phase Calibration" subsection for how phase affects
   *       the PCM output alignment.
   */
  bool setPhaseCalibration(uint8_t channel, uint8_t phase_calibration);

  /** Set phase delay for a channel
   * @param channel Channel number (1-8)
   * @param delay Delay value (0-255) applied to the PCM output phase
   * @return True if successful, false otherwise
   * @note This writes to the CHx_CFG4 register (offset 0x40 + 5*(channel-1)).
   *       See datasheet section "Phase Delay" or refer to the CHn CFG4
   *       description for how the delay affects the output alignment.
   */
  bool setPhaseDelay(uint8_t channel, uint8_t delay);

  /** Set decimation filter mode
   * @param mode Filter mode (linear, low latency, or ultra-low latency)
   * @return True if successful, false otherwise
   * @note Controlled by PCMD3180_DSP_CFG0 bits 1:0.  Refer to datasheet
   *       section "Decimation Filter Modes" for tradeoffs between latency and
   *       noise performance.
   */
  bool setDecimationFilterMode(FILTERmode mode);
  
  /** Configure channel summation mode
   * @param mode Summation mode (disabled, 2-channel, or 4-channel)
   * @return True if successful, false otherwise
   * @note Bits 3:2 of PCMD3180_DSP_CFG0 select summation.  See datasheet
   *       "Channel Summation" for details on which inputs are added and how
   *       it affects headphone/mono mixing.
   */
  bool setChannelSummationMode(SUMMATIONmode mode);
  
  /** Configure high-pass filter mode
   * @param mode HPF mode (custom, 12Hz, 96Hz, or 384Hz)
   * @return True if successful, false otherwise
   * @note HPF settings are stored in PCMD3180_DSP_CFG0 bits 5:4.  Refer to
   *       "High-Pass Filter" section of the datasheet for frequency
   *       responses and how the custom mode allows user‑defined cutoff.
   */
  bool setHighpassFilterMode(HPFILTERmode mode);
  
  /** Configure digital volume as ganged or independent
   * @param ganged_digital_volume True for ganged, false for independent
   * @return True if successful, false otherwise
   * @note Controlled by bit 2 of PCMD3180_DSP_CFG1.  See datasheet note on
   *       "Ganged Volume Control" when multiple channels should track.
   */
  bool setDigitalVolumeCfg(bool ganged_digital_volume);
  
  /** Configure number of biquad filters
   * @param num_biquads Number of biquads (0-3)
   * @return True if successful, false otherwise
   * @note Value is stored in PCMD3180_DSP_CFG1 bits 1:0.  Refer to the
   *       datasheet "Biquad Filter Configuration" for explanation of each
   *       filter stage and its purpose.
   */
  bool setBiquadCfg(uint8_t num_biquads);
  
  /** Enable or disable soft-step digital volume changes (inverted by design)
   * @param soft_step_enabled True to enable soft-step control
   * @return True if successful, false otherwise
   * @note Bit 4 of PCMD3180_DSP_CFG1.  See datasheet section
   *       "Soft-Step Volume Control" which describes how the hardware slowly
   *       ramps between volume levels to avoid clicks.
   */
  bool setSoftStepEnabled(bool soft_step_enabled);

  /** Control microphone bias power supply
   * @param power True to power on, false to power off
   * @return True if successful, false otherwise
   * @note Sets PCMD3180_PWR_CFG MICBIAS bit.  Refer to datasheet section
   *       "Power Control" for microphone bias characteristics and voltage
   *       options.
   */
  bool powerMICBIAS(bool power);
  
  /** Control PDM input power
   * @param power True to power on, false to power off
   * @return True if successful, false otherwise
   * @note Uses PCMD3180_PWR_CFG PDM bit.  See datasheet "Power Control"
   *       section for PDM interface power requirements.
   */
  bool powerPDM(bool power);
  
  /** Control PLL power
   * @param power True to power on, false to power off
   * @return True if successful, false otherwise
   * @note PCMD3180_PWR_CFG PLL bit; consult "PLL Power and Locking" in
   *       datasheet for lock time and enabling sequence when used as clock
   *       source.
   */
  bool powerPLL(bool power);
  
  /** Configure dynamic power-up mode
   * @param enable_dynamic_power_up True to enable dynamic power-up
   * @param mode Channel range for dynamic power-up (Ch1-2, 1-4, 1-6, or all)
   * @return True if successful, false otherwise
   * @note Bitfields live in PCMD3180_PWR_CFG.  See datasheet's "Dynamic
   *       Power-Up" subsection.
   */
  bool setDynamicPowerUpConfig(bool enable_dynamic_power_up, DYNAMICPOWERMode mode);
  
  /** Wake up the device from sleep mode
   * @return True if successful, false otherwise
   * @note Clears the SLEEP_CFG register and re‑initializes AREG.  See
   *       datasheet section "Sleep and Wake" for required delays and
   *       sequence when toggling power states.
   */
  bool wakeUp();
  
  /** Put the device into sleep mode
   * @return True if successful, false otherwise
   * @note Writes to the SLEEP_CFG register.  Refer to "Sleep and Wake"
   *       section of the datasheet for expected current consumption and how
   *       registers are retained.
   */
  bool sleep();
  
  /** Assert hardware shutdown via SHDNZ pin
   * @return True if successful, false if SHDNZ pin not configured
   * @note This toggles the external SHDNZ pin; refer to datasheet section
   *       "Hardware Shutdown" for de‑assertion timing and strap options.
   */
  bool hardwarePowerUp();
  
  /** De-assert hardware shutdown via SHDNZ pin
   * @return True if successful, false if SHDNZ pin not configured
   * @note See datasheet "Hardware Shutdown" for proper sequencing to avoid
   *       latch-up when using external reset lines.
   */
  bool hardwarePowerDown();
  
  /** Set internal or external AREG source
   * @param internal True for internal AREG generation, false for external
   * @return True if successful, false otherwise
   * @note Controlled by SLEEP_CFG AREG bit.  See datasheet "AREG and VREF"
   *       for how internal vs external source affects power-up time and noise.
   */
  bool setAREG(bool internal);
  
  /** Set VREF quick-charge duration after power-on
   * @param duration Charge duration (3.5ms, 10ms, 50ms, or 100ms)
   * @return True if successful, false otherwise
   * @note Field is in SLEEP_CFG register; longer durations provide more stable
   *       reference but increase wake-up time.  See datasheet "VREF
   *       Quick-Charge" subsection.
   */
  bool setVREFQuickChargeDuration(VREFChargeDuration duration);
  
  /** Set device shutdown behavior mode for DREG
   * @param mode Shutdown mode
   * @return True if successful, false otherwise
   * @note Mode value written to SHDN_CFG register.  Refer to datasheet
   *       "Shutdown Configuration" for available options.
   */
  bool setShutdownMode(SHUTDOWNMode mode);
  
  /** Set DREG active time after shutdown
   * @param time DREG active timing (5ms, 10ms, 25ms, or 30ms)
   * @return True if successful, false otherwise
   * @note DREG timing bits reside in SHDN_CFG.  See datasheet for guidance
   *       on choosing times based on supply ramp characteristics.
   */
  bool setDREGActiveTime(DREGActiveTime time);
  
  /** Configure microphone bias voltage settings
   * @param micbias_mode Bias source (VREF or AVDD)
   * @param vref_mode VREF voltage (2.75V, 2.5V, or 1.375V)
   * @return True if successful, false otherwise
   * @note Settings live in the BIAS_CFG register.  Consult the datasheet
   *       section "MICBIAS and VREF Configuration" for allowable combinations
   *       and expected voltages.
   */
  bool configureBIAS(MICBIASmode micbias_mode, VREFmode vref_mode);

  /** Configure interrupt pin behavior
   * @param mode Interrupt assertion mode (constant, 2ms pulsed, or 2ms once)
   * @param latch_mode Latch read mode (all or unmasked only)
   * @param polarity Interrupt polarity (active low or active high)
   * @return True if successful, false otherwise
   * @note This writes the INT_CFG register.  See datasheet "Interrupt
   *       Configuration" for examples of how edge/level sensitivity and
   *       latching work.
   */
  bool setInterruptConfig(INTMode mode, INTLatchReadMode latch_mode, INTPolarity polarity);
  
  /** Mask or unmask interrupt sources
   * @param mask_asi_clock_error True to mask ASI clock error interrupt
   * @param mask_pll_lock_interrupt True to mask PLL lock error interrupt
   * @return True if successful, false otherwise
   * @note This updates INT_MASK0 register.  Datasheet section "Interrupt
   *       Masking" explains which masks correspond to which fault conditions.
   */
  bool setInterruptMasks(bool mask_asi_clock_error, bool mask_pll_lock_interrupt);

  /** Write a single register value
   * @param reg Register address
   * @param value Value to write
   * @return True if write successful, false otherwise
   * @note Basic I2C write operation; no datasheet reference required but
   *       consult "I2C Interface" for retry behavior when the bus is busy.
   */
  bool writeRegister(uint8_t reg, uint8_t value);
  
  /** Read a single register value
   * @param reg Register address
   * @param value Reference to store the read value
   * @return True if read successful, false otherwise
   * @note Used by most configuration routines; refer to "I2C Interface"
   *       section for read timing requirements and acknowledge behavior.
   */
  bool readRegister(uint8_t reg, uint8_t &value);
  
  /** Update specific bits in a register (read-modify-write)
   * @param reg Register address
   * @param mask Bit mask indicating which bits to update
   * @param value New value for masked bits
   * @return True if update successful, false otherwise
   * @note This helper avoids disturbing unrelated bits when modifying
   *       registers such as MST_CFG0, ASI_CFG0, etc.  Datasheet shows which
   *       fields are read/write.
   */
  bool updateRegisterBits(uint8_t reg, uint8_t mask, uint8_t value);
  
  /** Check if device is present and responding on I2C bus
   * @return True if device is connected, false otherwise
   * @note Attempts a dummy read of PCMD3180_DEV_STS0.  See datasheet
   *       "I2C Device Address and Communication" section for address
   *       details.
   */
  bool isConnected();
  
  /** Read device status registers
   * @param status0 Reference to store DEV_STS0 register value
   * @param status1 Reference to store device mode status from DEV_STS1
   * @return True if read successful, false otherwise
   * @note DEV_STS0/1 contain error flags and current operating mode.  See
   *       datasheet section "Status Registers" for bit definitions.
   */
  bool getDeviceStatus(uint8_t &status0, uint8_t &status1);
  
  /** Read latched interrupt status flags
   * @param asi_bus_clock_error Reference to store ASI clock error flag
   * @param pll_lock_error Reference to store PLL lock error flag
   * @return True if read successful, false otherwise
   * @note Reads PCMD3180_INT_LTCH0 register.  Refer to "Interrupt Status"
   *       in datasheet for decoding additional latched bits.
   */
  bool getLatchedInterruptStatus(bool &asi_bus_clock_error, bool &pll_lock_error);
  
  /** Read auto-detected clock frequencies (slave mode)
   * @param fsync Reference to store detected FSYNC frequency in Hz
   * @param ratio Reference to store detected BCLK/FSYNC ratio
   * @return True if read successful, false otherwise
   * @note Values are derived from DEV_STS1 and CLK_SRC registers.  See
   *       datasheet "Auto-Detected Clocks" for how the part measures these
   *       when auto clock configuration is enabled.
   */
  bool getAutodetectedClocks(uint32_t &fsync, uint32_t &ratio);
  
private:
  // Register address constants
  static constexpr uint8_t REG_PAGE_SELECT   = 0x00;
  static constexpr uint8_t REG_SW_RESET      = 0x01;
  static constexpr uint8_t REG_SLEEP_CFG     = 0x02;
  static constexpr uint8_t REG_SHDN_CFG      = 0x05;
  static constexpr uint8_t REG_ASI_CFG0      = 0x07;
  static constexpr uint8_t REG_ASI_CFG1      = 0x08;
  static constexpr uint8_t REG_ASI_CFG2      = 0x09;
  static constexpr uint8_t REG_ASI_CH1       = 0x0B;
  static constexpr uint8_t REG_ASI_CH2       = 0x0C;
  static constexpr uint8_t REG_ASI_CH3       = 0x0D;
  static constexpr uint8_t REG_ASI_CH4       = 0x0E;
  static constexpr uint8_t REG_ASI_CH5       = 0x0F;
  static constexpr uint8_t REG_ASI_CH6       = 0x10;
  static constexpr uint8_t REG_ASI_CH7       = 0x11;
  static constexpr uint8_t REG_ASI_CH8       = 0x12;
  static constexpr uint8_t REG_MST_CFG0      = 0x13;
  static constexpr uint8_t REG_MST_CFG1      = 0x14;
  static constexpr uint8_t REG_ASI_STS       = 0x15;
  static constexpr uint8_t REG_CLK_SRC       = 0x16;
  static constexpr uint8_t REG_PDMCLK_CFG    = 0x1F;
  static constexpr uint8_t REG_PDMIN_CFG     = 0x20;
  static constexpr uint8_t REG_GPIO_CFG0     = 0x21;
  static constexpr uint8_t REG_GPO_CFG0      = 0x22;
  static constexpr uint8_t REG_GPO_CFG1      = 0x23;
  static constexpr uint8_t REG_GPO_CFG2      = 0x24;
  static constexpr uint8_t REG_GPO_CFG3      = 0x25;
  static constexpr uint8_t REG_GPO_VAL       = 0x29;
  static constexpr uint8_t REG_GPIO_MON      = 0x2A;
  static constexpr uint8_t REG_GPI_CFG0      = 0x2B;
  static constexpr uint8_t REG_GPI_CFG1      = 0x2C;
  static constexpr uint8_t REG_GPI_MON       = 0x2F;
  static constexpr uint8_t REG_INT_CFG       = 0x32;
  static constexpr uint8_t REG_INT_MASK0     = 0x33;
  static constexpr uint8_t REG_INT_LTCH0     = 0x36;
  static constexpr uint8_t REG_BIAS_CFG      = 0x3B;
  static constexpr uint8_t REG_CH1_CFG0      = 0x3C;
  static constexpr uint8_t REG_CH1_CFG2      = 0x3E;
  static constexpr uint8_t REG_CH1_CFG3      = 0x3F;
  static constexpr uint8_t REG_CH1_CFG4      = 0x40;
  static constexpr uint8_t REG_CH2_CFG0      = 0x41;
  static constexpr uint8_t REG_CH2_CFG2      = 0x43;
  static constexpr uint8_t REG_CH2_CFG3      = 0x44;
  static constexpr uint8_t REG_CH2_CFG4      = 0x45;
  static constexpr uint8_t REG_CH3_CFG0      = 0x46;
  static constexpr uint8_t REG_CH3_CFG2      = 0x48;
  static constexpr uint8_t REG_CH3_CFG3      = 0x49;
  static constexpr uint8_t REG_CH3_CFG4      = 0x4A;
  static constexpr uint8_t REG_CH4_CFG0      = 0x4B;
  static constexpr uint8_t REG_CH4_CFG2      = 0x4D;
  static constexpr uint8_t REG_CH4_CFG3      = 0x4E;
  static constexpr uint8_t REG_CH4_CFG4      = 0x4F;
  static constexpr uint8_t REG_CH5_CFG0      = 0x50;
  static constexpr uint8_t REG_CH5_CFG2      = 0x52;
  static constexpr uint8_t REG_CH5_CFG3      = 0x53;
  static constexpr uint8_t REG_CH5_CFG4      = 0x54;
  static constexpr uint8_t REG_CH6_CFG0      = 0x55;
  static constexpr uint8_t REG_CH6_CFG2      = 0x57;
  static constexpr uint8_t REG_CH6_CFG3      = 0x58;
  static constexpr uint8_t REG_CH6_CFG4      = 0x59;
  static constexpr uint8_t REG_CH7_CFG0      = 0x5A;
  static constexpr uint8_t REG_CH7_CFG2      = 0x5C;
  static constexpr uint8_t REG_CH7_CFG3      = 0x5D;
  static constexpr uint8_t REG_CH7_CFG4      = 0x5E;
  static constexpr uint8_t REG_CH8_CFG0      = 0x5F;
  static constexpr uint8_t REG_CH8_CFG2      = 0x61;
  static constexpr uint8_t REG_CH8_CFG3      = 0x62;
  static constexpr uint8_t REG_CH8_CFG4      = 0x63;
  static constexpr uint8_t REG_DSP_CFG0      = 0x6B;
  static constexpr uint8_t REG_DSP_CFG1      = 0x6C;
  static constexpr uint8_t REG_IN_CH_EN      = 0x73;
  static constexpr uint8_t REG_ASI_OUT_CH_EN = 0x74;
  static constexpr uint8_t REG_PWR_CFG       = 0x75;
  static constexpr uint8_t REG_DEV_STS0      = 0x76;
  static constexpr uint8_t REG_DEV_STS1      = 0x77;
  static constexpr uint8_t REG_I2C_CKSUM     = 0x7E;
  
  uint8_t _i2c_addr;
  TwoWire *_wire;
  bool _initialized;
  uint8_t _shdnz_pin;
  bool _areg_internal;
  AVDDInputVoltage _avdd_input_voltage;
  
  // Helper methods
  void delayMicros(uint32_t us);
};

#endif // PCMD3180_H