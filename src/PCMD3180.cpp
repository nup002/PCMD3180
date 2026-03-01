/*
 * PCMD3180 Arduino Library Implementation
 */

#include "PCMD3180.h"

PCMD3180::PCMD3180(uint8_t shdnz_pin, uint8_t i2c_addr, bool areg_internal, AVDDInputVoltage avdd_input_voltage) {
  _i2c_addr = i2c_addr;
  _shdnz_pin = shdnz_pin;
  _areg_internal = areg_internal;  // Whether AREG is set to be generated internally or externally
  _avdd_input_voltage = avdd_input_voltage;
  _wire = nullptr;
  _initialized = false;
  hardwarePowerDown();
}

bool PCMD3180::begin(TwoWire &wirePort) {
  _wire = &wirePort;
  
  // Pull SHDNZ pin high to take the device out of hardware shutdown
  hardwarePowerUp();

  // Check if device is present
  if (!isConnected()) {
    return false;
  }
  
  // Perform reset and basic initialization
  if (!reset()) {
    return false;
  }
  
  // Configure sleep mode - wake device and use internal AREG
  if (!wakeUp()) {
    return false;
  }
  
  // Wait >= 1ms after entering active mode
  delay(1);
  
  _initialized = true;
  return true;
}

bool PCMD3180::reset() {
  // Software reset
  if (!writeRegister(REG_SW_RESET, 0x01)) {
    return false;
  }
  
  // Wait >= 10ms after reset
  delay(10);
  
  return true;
}

/*
Utility methods
*/
bool PCMD3180::configureAsSlave(AudioFormat format, WordLength word_len, uint8_t num_channels) {
  if (num_channels < 1 || num_channels > 8) {
    return false;
  }
  if (!setMasterMode(MODE_SLAVE)) return false;
  if (!setAutoClockConfigEnabled(true)) return false;
  if (!setAutoclockPLLEnabled(true)) return false;
  if (!setASIFormat(format, word_len)) return false;
  if (!enableAllChannels(num_channels)) return false;
  return true;
}

bool PCMD3180::configureAsMaster(AudioFormat format, WordLength word_len, FSRate fsync_rate, BCLKRatio bclk_ratio, uint8_t num_channels) {
  if (num_channels < 1 || num_channels > 8) {
    return false;
  }
  if (!setMasterMode(MODE_MASTER)) return false;
  if (!setASIClock(fsync_rate, bclk_ratio)) return false;
  if (!setASIFormat(format, word_len)) return false;
  if (!enableAllChannels(num_channels)) return false;
  return true;
}

bool PCMD3180::configurePDMInput(PDMClock clk, uint8_t num_channels) {
  if (num_channels < 1 || num_channels > 8) {
    return false;
  }
  if (!setPDMClock(clk)) return false;

  // Enable ports in pairs: port 1 covers ch1+2, port 2 covers ch3+4, etc.
  uint8_t num_ports = (num_channels + 1) / 2;
  for (uint8_t port = 1; port <= num_ports; port++) {
    if (!enablePort(port, true)) return false;
  }

  if (!enableAllChannels(num_channels)) return false;
  return true;
}

bool PCMD3180::enableAllChannels(uint8_t num_channels) {
  if (num_channels < 1 || num_channels > 8) {
    return false;
  }
  // Channels are mapped MSB-first: ch1 = bit 7, ch2 = bit 6, etc.
  uint8_t mask = (uint8_t)(0xFF00 >> num_channels);
  if (!enableChannels(mask)) return false;
  if (!enableOutputASIChannels(mask)) return false;
  return true;
}

bool PCMD3180::setAllChannelVolumes(uint8_t volume) {
  for (uint8_t ch = 1; ch <= 8; ch++) {
    if (!setDigitalVolume(ch, volume)) return false;
  }
  return true;
}

/*
Read-methods
*/
bool PCMD3180::getLatchedInterruptStatus(bool &asi_bus_clock_error, bool &pll_lock_error) {
  uint8_t status;
  if (!readRegister(REG_INT_LTCH0, status)) {
    return false;
  }
  asi_bus_clock_error = status & 0x80;
  pll_lock_error = status & 0x40;
  return true;
}

bool PCMD3180::getAutodetectedClocks(uint32_t &fsync, uint32_t &ratio) {
  uint8_t status;
  if (!readRegister(REG_ASI_STS, status)) {
    return false;
  }
  uint8_t fsync_rate = (status & 0xF0) >> 4;
  uint8_t fsync_ratio = status & 0x0F;

  static const uint32_t fsync_table[] = {7350, 14700, 22050, 29400, 44100, 88200, 176400, 352800, 705600};
  static const uint16_t ratio_table[] = {16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 1024, 2048};

  if (fsync_rate >= sizeof(fsync_table)/sizeof(fsync_table[0])) {
    return false;
  }

  if (fsync_ratio >= sizeof(ratio_table)/sizeof(ratio_table[0])) {
    return false;
  }

  fsync = fsync_table[fsync_rate];
  ratio = ratio_table[fsync_ratio];
  return true;
}

bool PCMD3180::getGPIOMonitorValue(uint8_t &monitor_value) {
  uint8_t status;
  if (!readRegister(REG_GPIO_MON, status)) {
    return false;
  }
  monitor_value = (status & 0x80) >> 7;
  return true;
}

bool PCMD3180::getGPIMonitorValue(uint8_t port, uint8_t &monitor_value) {
  if (port < 1 || port > 4) {
    return false;
  }
  uint8_t status;
  if (!readRegister(REG_GPI_MON, status)) {
    return false;
  }
  uint8_t mask = 0x80 >> (port - 1);
  monitor_value = (status & mask) >> (8 - port);
  return true;
}

bool PCMD3180::getDeviceStatus(uint8_t &status0, uint8_t &device_mode_status) {
  if (!readRegister(REG_DEV_STS0, status0)) {
    return false;
  }
  uint8_t sts1;
  if (!readRegister(REG_DEV_STS1, sts1)) {
    return false;
  }
  device_mode_status = sts1 >> 5;
  return true;
}


bool PCMD3180::wakeUp() {
  if (!updateRegisterBits(REG_SLEEP_CFG, 0x01, 0x01)) {
    return false;
  }
  delay(1);

  if (!setAREG(_areg_internal)){
    return false;
  }
  if (_avdd_input_voltage == AVDD_INPUT_18V){
    return updateRegisterBits(REG_BIAS_CFG, 0x03, 0x02);
  }
  return true;
}

bool PCMD3180::sleep() {
  if (!updateRegisterBits(REG_SLEEP_CFG, 0x01, 0x00)) {
    return false;
  }
  delay(10);
  return true;
}

/*
SLEEP_CFG register methods
*/
bool PCMD3180::setAREG(bool internal) {
  if (!updateRegisterBits(REG_SLEEP_CFG, 0x80, internal ? 0x80 : 0x00)) {
    return false;
  }
  return true;
}

bool PCMD3180::setVREFQuickChargeDuration(VREFChargeDuration duration) {
  uint8_t cfg = 0x00;
  switch (duration) {
    case VREF_CHARGE_DURATION_35:
      cfg |= 0x00 << 3;
      break;
    case VREF_CHARGE_DURATION_10:
      cfg |= 0x01 << 3;
      break;
    case VREF_CHARGE_DURATION_50:
      cfg |= 0x02 << 3;
      break;
    case VREF_CHARGE_DURATION_100:
      cfg |= 0x03 << 3;
      break;
    default:
      return false;
  }
  if (!updateRegisterBits(REG_SLEEP_CFG, 0x18, cfg)) {
    return false;
  }
  return true;
}

bool PCMD3180::setI2CBroadcast(bool enable) {
  if (!updateRegisterBits(REG_SLEEP_CFG, 0x04, enable ? 0x04 : 0x00)) {
    return false;
  }
  return true;
}

/*
SHDN_CFG register methods
*/
bool PCMD3180::setShutdownMode(SHUTDOWNMode mode) {
  uint8_t cfg = 0x00;
  switch (mode) {
    case SHUTDOWN_DREG_IMMEDIATELY:
      cfg = 0x00;
    case SHUTDOWN_DREG_ACTIVE_UNTIL_TIMOUT:
      cfg = 0x04;
    case SHUTDOWN_DREG_ACTIVE_UNTIL_SHUTDOWN:
      cfg = 0x08;
    default:
      return false;
  }
  if (!updateRegisterBits(REG_SHDN_CFG, 0x0C, cfg)) {
    return false;
  }
  return true;
}

bool PCMD3180::setDREGActiveTime(DREGActiveTime time) {
  uint8_t cfg = 0x00;
  switch (time) {
    case DREG_ACTIVE_TIME_5:
      cfg |= 3;
      break;
    case DREG_ACTIVE_TIME_10:
      cfg |= 2;
      break;
    case DREG_ACTIVE_TIME_25:
      cfg |= 1;
      break;
    case DREG_ACTIVE_TIME_30:
      break;
    default:
      return false;
  }
  if (!updateRegisterBits(REG_SHDN_CFG, 0x03, cfg)) {
    return false;
  }
  return true;
}

/*
ASI_CFG0 register methods
*/
bool PCMD3180::setASIFormat(AudioFormat format, WordLength wordLen) {
  uint8_t asi_cfg0 = 0;
  
  // Set audio format
  switch (format) {
    case FORMAT_I2S:
      asi_cfg0 |= 0x40;
      break;
    case FORMAT_TDM:
      asi_cfg0 |= 0x00;
      break;
    case FORMAT_LEFT_JUSTIFIED:
      asi_cfg0 |= 0x80;
      break;
  }
  
  // Set word length
  switch (wordLen) {
    case WORD_16_BIT:
      asi_cfg0 |= 0x00;
      break;
    case WORD_20_BIT:
      asi_cfg0 |= 0x10;
      break;
    case WORD_24_BIT:
      asi_cfg0 |= 0x20;
      break;
    case WORD_32_BIT:
      asi_cfg0 |= 0x30;
      break;
  }
  
  // Write configurations
  if (!updateRegisterBits(REG_ASI_CFG0, 0xFC, asi_cfg0)) {
    return false;
  }
  return true;
}

bool PCMD3180::setASIPolarities(ASIPolarity fsyncPolarity, ASIPolarity blckPolarity) {
  uint8_t asi_cfg0 = 0;
  
  // Set FSYNC polarity
  switch (fsyncPolarity) {
    case ASI_POLARITY_STANDARD:
      break;
    case ASI_POLARITY_INVERTED:
      asi_cfg0 |= 0x08;
      break;
    default:
      return false;
  }

  // Set BLCK polarity
  switch (blckPolarity) {
    case ASI_POLARITY_STANDARD:
      break;
    case ASI_POLARITY_INVERTED:
      asi_cfg0 |= 0x04;
      break;
    default:
      return false;
  }
  
  // Write configurations
  if (!updateRegisterBits(REG_ASI_CFG0, 0x0C, asi_cfg0)) {
    return false;
  }
  return true;
}

/*
ASI_CFG2 register methods
*/
bool PCMD3180::setASIErrorDetection(bool enableErrorDetection, bool enableAutoResumeOnRecovery) {
  uint8_t asi_cfg1 = 0;

  if (!enableErrorDetection) {
    asi_cfg1 |= 0x20;
  }
  if (!enableAutoResumeOnRecovery) {
    asi_cfg1 |= 0x10;
  }

  // Write configurations
  if (!updateRegisterBits(REG_ASI_CFG1, 0x30, asi_cfg1)) {
    return false;
  }
  return true;
}

bool PCMD3180::setASIDaisyChained(bool enableDaisyChain) {
  return updateRegisterBits(REG_ASI_CFG1, 0xF0, enableDaisyChain ? 0xF0 : 0x00);
}


/*
ASI_CH1 to ASI_CH7 register methods
*/
bool PCMD3180::setASISlotAssignment(uint8_t channel, uint8_t slot) {
  if ((slot > 63) || (channel < 1 || channel > 8)) {
    return false;
  }
  uint8_t asi_slot_reg = REG_ASI_CH1 + channel - 1;
  return writeRegister(asi_slot_reg, slot);
}

bool PCMD3180::setASIOutputLine(uint8_t channel, ASIPIN asi_pin) {
  if (channel < 1 || channel > 8) {
    return false;
  }
  uint8_t asi_slot_reg = REG_ASI_CH1 + channel - 1;

  uint8_t cfg = 0x00;
  if (asi_pin == ASIPIN_PRIMARY) {
    cfg = 0x40;  
  }
  return updateRegisterBits(asi_slot_reg, 0x40, cfg);
}

/*
MST_CFG methods
*/
bool PCMD3180::setMasterMode(MasterMode masterMode) {
  bool master = (masterMode == MODE_MASTER);
  return updateRegisterBits(REG_MST_CFG0, 0x80, master ? 0x80 : 0x00);
}


bool PCMD3180::setMasterConfig(bool enable_gated_fsync_and_bclk, FSMode fs_mode) {
  uint8_t cfg = 0;
  if (enable_gated_fsync_and_bclk) {
    cfg |= 0x10;
  }
  if (fs_mode == FSYNC_MODE_44100) {
    cfg |= 0x08;
  }
  return updateRegisterBits(REG_MST_CFG0, 0x18, cfg);
}

bool PCMD3180::setASIClock(FSRate fsync_rate, BCLKRatio bclk_ratio) {
  uint8_t cfg = 0;
  switch (fsync_rate) {
    case FSRATE_8:
      cfg = 0x00;
      break;
    case FSRATE_16:
      cfg = 0x10;
      break;
    case FSRATE_24:
      cfg = 0x20;
      break;
    case FSRATE_32:
      cfg = 0x30;
      break;
    case FSRATE_48:
      cfg = 0x40;
      break;
    case FSRATE_96:
      cfg = 0x50;
      break;
    case FSRATE_192:
      cfg = 0x60;
      break;
    case FSRATE_384:
      cfg = 0x70;
      break;
    case FSRATE_768:
      cfg = 0x80;
      break;
    default:
      return false;
  };

  switch (bclk_ratio) {
    case BCLKRATIO_16:
      cfg |= 0x00;
      break;
    case BCLKRATIO_24:
      cfg |= 0x01;
      break;
    case BCLKRATIO_32:
      cfg |= 0x02;
      break;
    case BCLKRATIO_48:
      cfg |= 0x03;
      break;
    case BCLKRATIO_64:
      cfg |= 0x04;
      break;
    case BCLKRATIO_96:
      cfg |= 0x05;
      break;
    case BCLKRATIO_128:
      cfg |= 0x06;
      break;
    case BCLKRATIO_192:
      cfg |= 0x07;
      break;
    case BCLKRATIO_256:
      cfg |= 0x08;
      break;
    case BCLKRATIO_384:
      cfg |= 0x09;
      break;
    case BCLKRATIO_512:
      cfg |= 0x0A;
      break;
    case BCLKRATIO_1024:
      cfg |= 0x0B;
      break;
    case BCLKRATIO_2048:
      cfg |= 0x0C;
      break;
    default:
      return false;
  }
  return writeRegister(REG_MST_CFG1, cfg);
}

/*
Slave mode clock configuration
*/
bool PCMD3180::setAutoClockConfigEnabled(bool auto_clock_config_enabled) {
  return updateRegisterBits(REG_MST_CFG1, 0x40, auto_clock_config_enabled ? 0x40 : 0x00);
}

bool PCMD3180::setAutoclockPLLEnabled(bool PLL_enabled) {
  return updateRegisterBits(REG_MST_CFG1, 0x10, PLL_enabled ? 0x10 : 0x00);
}

bool PCMD3180::setDisabledPLLClockSource(PLLSlaveClockSource clk_source) {
  uint8_t cfg = 0;
  if (clk_source == PLLSLAVECLKSRC_MCLK) {
    cfg = 0x80;
  }
  return updateRegisterBits(REG_CLK_SRC, 0x80, cfg);
}


/*
PDMCLK_CFG register methods
*/
bool PCMD3180::setPDMClock(PDMClock clk) {
  uint8_t pdmclk_val = 0;
  
  switch (clk) {
    case PDMCLK_2822_KHZ:
      pdmclk_val = 0X00;
      break;
    case PDMCLK_1411_KHZ:
      pdmclk_val = 0X01;
      break;
    case PDMCLK_705_KHZ:
      pdmclk_val = 0X02;
      break;
    case PDMCLK_5644_KHZ:
      pdmclk_val = 0X03;
      break;
  }
  
  return writeRegister(REG_PDMCLK_CFG, 0x40 | pdmclk_val);
}

/*
PDMDIN_CFG register methods
*/
bool PCMD3180::setPortEdgeLatchMode(uint8_t port, EDGELatchMode mode) {
  if (port < 1 || port > 4) {
    return false;
  }
  uint8_t mask = 0x80 >> (port - 1);
  uint8_t cfg = ((mode == EDGE_MODE_EVEN_NEGATIVE) ? 0x00 : 0x80) >> (port - 1);
  return updateRegisterBits(REG_PDMIN_CFG, mask, cfg);
}

/*
GPIO_CFG0  register methods
*/
bool PCMD3180::setGPIOConfig(GPIOMode mode, DRIVEMode drive_mode) {
  uint8_t cfg = 0;
  switch (mode) {
    case GPIO_MODE_DISABLED:
      cfg |= 0x00;
      break;
    case GPIO_MODE_GPO:
      cfg |= 0x10;
      break;
    case GPIO_MODE_IRQ:
      cfg |= 0x20;
      break;
    case GPIO_MODE_SDOUT2:
      cfg |= 0x30;
      break;
    case GPIO_MODE_PDMCLK:
      cfg |= 0x40;
      break;
    case GPIO_MODE_MICBIAS_EN:
      cfg |= 0x80;
      break;
    case GPIO_MODE_GPI:
      cfg |= 0x90;
      break;
    case GPIO_MODE_MCLK:
      cfg |= 0xA0;
      break;
    case GPIO_MODE_SDIN:
      cfg |= 0xB0;
      break;
    case GPIO_MODE_PDMDIN1:
      cfg |= 0xC0;
      break;
    case GPIO_MODE_PDMDIN2:
      cfg |= 0xD0;
      break;
    case GPIO_MODE_PDMDIN3:
      cfg |= 0xE0;
      break;
    case GPIO_MODE_PDMDIN4:
      cfg |= 0xF0;
      break;
    default:
      return false;
  };

  switch (drive_mode) {
    case DRIVE_MODE_HI_Z:
      cfg |= 0;
      break;
    case DRIVE_MODE_ACTIVE_LOW_ACTIVE_HIGH:
      cfg |= 1;
      break;
    case DRIVE_MODE_ACTIVE_LOW_WEAK_HIGH:
      cfg |= 2;
      break;
    case DRIVE_MODE_ACTIVE_LOW_HI_Z:
      cfg |= 3;
      break;
    case DRIVE_MODE_WEAK_LOW_ACTIVE_HIGH:
      cfg |= 4;
      break;
    case DRIVE_MODE_HI_Z_ACTIVE_HIGH:
      cfg |= 5;
      break;
    default:
      return false;
  }
  return writeRegister(REG_GPIO_CFG0, cfg);
}

/*
GPO_CFG0 to GPO_CFG3 register methods
*/
bool PCMD3180::setGPOMode(uint8_t port, GPOMode mode, DRIVEMode drive_mode) {
  if (port < 1 || port > 4) {
    return false;
  }
  uint8_t cfg;
  switch (mode) {
    case GPO_MODE_DISABLED:
      cfg = 0x00;
      break;
    case GPO_MODE_GPO:
      cfg = 0x10;
      break;
    case GPO_MODE_IRQ:
      cfg = 0x20;
      break;
    case GPO_MODE_ASI:
      cfg = 0x30;
      break;
    case GPO_MODE_PDM:
      cfg = 0x40;
      break;
    default:
      return false;
  }

  switch (drive_mode) {
    case DRIVE_MODE_HI_Z:
      cfg |= 0;
      break;
    case DRIVE_MODE_ACTIVE_LOW_ACTIVE_HIGH:
      cfg |= 1;
      break;
    default:
      return false;
  }

  uint8_t gpo_mode_reg = REG_GPO_CFG0 + (port - 1);
  return writeRegister(gpo_mode_reg, cfg);
}

/*
GPO_VAL register methods
*/
bool PCMD3180::setGPIOVal(bool drive_high) {
  return updateRegisterBits(REG_GPO_VAL, 0x80, drive_high ? 0x80 : 0x00);
}

bool PCMD3180::setGPOVal(uint8_t port, bool drive_high) {
  if (port < 1 || port > 4) {
    return false;
  }
  uint8_t cfg = drive_high ? 0x80 : 0x00;
  cfg = cfg >> (port - 1);
  uint8_t mask = 0x80 >> (port - 1);
  return updateRegisterBits(REG_GPO_VAL, mask, cfg);
}

/*
GPI_CFG0 to GPI_CFG1 register methods
*/
bool PCMD3180::setGPIMode(uint8_t port, GPIMode mode) {
  if (port < 1 || port > 4) {
    return false;
  }
  uint8_t mode_val;
  switch (mode) {
    case GPI_MODE_DISABLED:
      mode_val = 0x00;
      break;
    case GPI_MODE_GPI:
      mode_val = 0x01;
      break;
    case GPI_MODE_MCLK:
      mode_val = 0x02;
      break;
    case GPI_MODE_ASI:
      mode_val = 0x03;
      break;
    case GPI_MODE_PDMDIN1:
      mode_val = 0x04;
      break;
    case GPI_MODE_PDMDIN2:
      mode_val = 0x05;
      break;
    case GPI_MODE_PDMDIN3:
      mode_val = 0x06;
      break;
    case GPI_MODE_PDMDIN4:
      mode_val = 0x07;
      break;
    default:
      return false;
  }
  uint8_t gpi_mode_reg = REG_GPI_CFG0 + (port - 1) / 2; // Port 1 and 2 uses register 2B, port 3 and 4 uses 2C
  uint8_t mask = 0b00000111;
  if ((port == 1) || (port == 3)) {
    mode_val <<= 4; // The mode value is written to bits 6-4 for port 1 and 3, and bits 2-0 for port 2 and 4
    mask <<= 4;
  }
  return updateRegisterBits(gpi_mode_reg, mask, mode_val);
}

/*
INT_CFG and INT_MASK0 register methods
*/
bool PCMD3180::setInterruptConfig(INTMode mode, INTLatchReadMode latch_mode, INTPolarity polarity) {
  uint8_t cfg = 0;
  switch (mode) {
    case INT_MODE_ASSERT_CONSTANT:
      cfg |= 0x00;
      break;
    case INT_MODE_ASSERT_2MS_PULSED:
      cfg |= 0x01 << 5;
      break;
    case INT_MODE_ASSERT_2MS_ONCE:
      cfg |= 0x03 << 5;
      break;
    default:
      return false;
  }

  if (latch_mode == INT_LATCH_MODE_READ_ONLY_UNMASKED){
    cfg |= 0x04;
  }

  if (polarity == INT_POLARITY_ACTIVE_HIGH) {
    cfg |= 0x80;
  }

  return writeRegister(REG_INT_CFG, cfg);
}

bool PCMD3180::setInterruptMasks(bool mask_asi_clock_error, bool mask_pll_lock_interrupt) {
  uint8_t cfg = 0x00;
  if (mask_asi_clock_error){
    cfg |= 0x80;
  }

  if (mask_pll_lock_interrupt){
    cfg |= 0x40;
  }

  return writeRegister(REG_INT_MASK0, cfg);
}

/*
BIAS_CFG register methods
*/
bool PCMD3180::configureBIAS(MICBIASmode micbias_mode, VREFmode vref_mode) {
  uint8_t cfg = 0x00;
  switch (micbias_mode) {
    case MICBIAS_MODE_AVDD:
      cfg |= 0x60;
      break;
    case MICBIAS_MODE_VREF:
      cfg |= 0x00;
      break;
    default:
      return false;
  };

  switch (vref_mode) {
    case VREF_MODE_275V:
      cfg |= 0x00;
      break;
    case VREF_MODE_25V:
      cfg |= 0x01;
      break;
    case VREF_MODE_1375V:
      cfg |= 0x02;
      break;
    default:
      return false;
  }

  return writeRegister(REG_BIAS_CFG, cfg);
}

/*
CH1_CFG0 TO CH8_CFG4 register methods
*/
bool PCMD3180::enablePort(uint8_t port, bool enable) {
  if (port < 1 || port > 4) {
    return false;
  }

  uint8_t input_pdm_reg = REG_CH1_CFG0 + 5 * (port - 1);

  if (enable) {
    return writeRegister(input_pdm_reg, 0x40);
  } else {
    return writeRegister(input_pdm_reg, 0x00);
  }
}

bool PCMD3180::setDigitalVolume(uint8_t channel, uint8_t volume) {
  if (channel < 1 || channel > 8) {
    return false;
  }
  
  // Calculate register address for channel volume
  uint8_t vol_reg = REG_CH1_CFG2 + ((channel - 1) * 5);
  
  return writeRegister(vol_reg, volume);
}

bool PCMD3180::setGainCalibration(uint8_t channel, uint8_t gain_calibration) {
  if (channel < 1 || channel > 8) {
    return false;
  }
  
  if (gain_calibration > 15) {
    return false;
  }

  // Calculate register address for channel gain calibration
  uint8_t gain_reg = REG_CH1_CFG3 + ((channel - 1) * 5);
  
  return writeRegister(gain_reg, gain_calibration << 4);
}

bool PCMD3180::setPhaseCalibration(uint8_t channel, uint8_t phase_calibration) {
  if (channel < 1 || channel > 8) {
    return false;
  }

  // Calculate register address for channel phase calibration
  uint8_t phase_reg = REG_CH1_CFG4 + ((channel - 1) * 5);
  
  return writeRegister(phase_reg, phase_calibration);
}

/*
DSP_CFG0 register methods
*/
bool PCMD3180::setDecimationFilterMode(FILTERmode mode) {
  uint8_t cfg;
  switch (mode) {
    case FILTER_MODE_LINEAR:
      cfg = 0x00;
      break;
    case FILTER_MODE_LOW_LATENCY:
      cfg = 0x10;
      break;
    case FILTER_MODE_ULTRA_LOW_LATENCY:
      cfg = 0x20;
      break;
    default:
      return false;
  }
  return updateRegisterBits(REG_DSP_CFG0, 0x30, cfg);
}

bool PCMD3180::setChannelSummationMode(SUMMATIONmode mode) {
  uint8_t cfg;
  switch (mode) {
    case SUMMATION_MODE_DISABLED:
      cfg = 0x00;
      break;
    case SUMMATION_MODE_2CHAN:
      cfg = 0x04;
      break;
    case SUMMATION_MODE_4CHAN:
      cfg = 0x08;
      break;
    default:
      return false;
  }
  return updateRegisterBits(REG_DSP_CFG0, 0x0C, cfg);
}

bool PCMD3180::setHighpassFilterMode(HPFILTERmode mode) {
  uint8_t cfg;
  switch (mode) {
    case HP_FILTER_MODE_CUSTOM:
      cfg = 0x00;
      break;
    case HP_FILTER_MODE_12HZ:
      cfg = 0x01;
      break;
    case HP_FILTER_MODE_96HZ:
      cfg = 0x02;
      break;
    case HP_FILTER_MODE_384HZ:
      cfg = 0x03;
      break;
    default:
      return false;
  }
  return updateRegisterBits(REG_DSP_CFG0, 0x03, cfg);
}

/*
DSP_CFG1 register methods
*/
bool PCMD3180::setDigitalVolumeCfg(bool ganged_digital_volume) {
  return updateRegisterBits(REG_DSP_CFG1, 0x80, ganged_digital_volume ? 0x80 : 0x00);
}

bool PCMD3180::setBiquadCfg(uint8_t num_biquads) {
  if (num_biquads > 3) {
    return false;
  }
  uint8_t cfg = num_biquads << 5;
  return updateRegisterBits(REG_DSP_CFG1, 0x60, cfg);
}

bool PCMD3180::setSoftStepEnabled(bool soft_step_enabled) {
  return updateRegisterBits(REG_DSP_CFG1, 0x10, soft_step_enabled ? 0x00 : 0x10);
}

/*
IN_CH_EN register methods
*/
bool PCMD3180::enableChannels(uint8_t ch_mask) {
  // IN_CH_EN register - bit 7 = CH1, bit 6 = CH2, etc.
  return writeRegister(REG_IN_CH_EN, ch_mask);
}

/*
ASI_OUT_CH_EN register methods
*/
bool PCMD3180::enableOutputASIChannels(uint8_t ch_mask) {
  // ASI_OUT_CH_EN register - bit 7 = CH1, bit 6 = CH2, etc.
  return writeRegister(REG_ASI_OUT_CH_EN, ch_mask);
}

/*
PWR_CFG register methods
*/
bool PCMD3180::powerMICBIAS(bool power) {
  uint8_t value = power ? 0x80 : 0x00;
  return updateRegisterBits(REG_PWR_CFG, 0x80, value);
}

bool PCMD3180::powerPDM(bool power) {
  uint8_t value = power ? 0x40 : 0x00;
  return updateRegisterBits(REG_PWR_CFG, 0x40, value);
}

bool PCMD3180::powerPLL(bool power) {
  uint8_t value = power ? 0x20 : 0x00;
  return updateRegisterBits(REG_PWR_CFG, 0x20, value);
}

bool PCMD3180::setDynamicPowerUpConfig(bool enable_dynamic_power_up, DYNAMICPOWERMode mode) {
  uint8_t cfg = enable_dynamic_power_up ? 0x10 : 0x00;

  switch (mode) {
    case DYNAMIC_POWER_ENABLE_CH1_TO_CH2:
      cfg |= 0x00 << 2;
      break;
    case DYNAMIC_POWER_ENABLE_CH1_TO_CH4:
      cfg |= 0x01 << 2;
      break;
    case DYNAMIC_POWER_ENABLE_CH1_TO_CH6:
      cfg |= 0x02 << 2;
      break;
    case DYNAMIC_POWER_ENABLE_ALL_CH:
      cfg |= 0x03 << 2;
      break;
    default:
      return false;
  }
  return updateRegisterBits(REG_PWR_CFG, 0x1C, cfg);
}



bool PCMD3180::hardwarePowerUp() {
  if (_shdnz_pin == 0xFF) {
    return false;
  }
  digitalWrite(_shdnz_pin, HIGH);
  delay(1);
  return true;
}

bool PCMD3180::hardwarePowerDown() {
  if (_shdnz_pin == 0xFF) {
    return false;
  }
  digitalWrite(_shdnz_pin, LOW);
  delay(50);
  return true;
}




bool PCMD3180::writeRegister(uint8_t reg, uint8_t value) {
  if (!_wire) {
    return false;
  }
  
  _wire->beginTransmission(_i2c_addr);
  _wire->write(reg);
  _wire->write(value);
  
  return (_wire->endTransmission() == 0);
}

bool PCMD3180::readRegister(uint8_t reg, uint8_t &value) {
  if (!_wire) {
    return false;
  }
  
  _wire->beginTransmission(_i2c_addr);
  _wire->write(reg);
  
  if (_wire->endTransmission(false) != 0) {
    return false;
  }
  
  if (_wire->requestFrom(_i2c_addr, (uint8_t)1) != 1) {
    return false;
  }
  
  value = _wire->read();
  return true;
}

bool PCMD3180::updateRegisterBits(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current_val;
  
  if (!readRegister(reg, current_val)) {
    return false;
  }
  
  uint8_t new_val = (current_val & ~mask) | (value & mask);
  
  return writeRegister(reg, new_val);
}

bool PCMD3180::isConnected() {
  if (!_wire) {
    return false;
  }
  
  _wire->beginTransmission(_i2c_addr);
  return (_wire->endTransmission() == 0);
}