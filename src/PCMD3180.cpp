/*
 * PCMD3180 Arduino Library Implementation
 */

#include "PCMD3180.h"

PCMD3180::PCMD3180(uint8_t shdnzPin, uint8_t i2cAddr, bool aregInternal, AVDDInputVoltage avddInputVoltage) {
  _i2cAddr = i2cAddr;
  _shdnzPin = shdnzPin;
  _aregInternal = aregInternal;  // Whether AREG is set to be generated internally or externally
  _avddInputVoltage = avddInputVoltage;
  _wire = nullptr;
  _initialized = false;
  _sleepCfg = 0x00;
}

bool PCMD3180::begin(TwoWire &wire) {
  _wire = &wire;
  
  // Drive SHDNZ low to put the device in hardware shutdown, then high to take it out
  if (_shdnzPin != PCMD3180_SHDNZ_PIN_DEFAULT) {
    pinMode(_shdnzPin, OUTPUT);
  }
  hardwarePowerDown();
  hardwarePowerUp();

  // Check if device is present
  if (!isConnected()) {
    return false;
  }
  
  // Perform reset and basic initialization
  if (!reset()) {
    return false;
  }
  
  // Exit sleep mode and select the AREG source given to the constructor
  if (!wakeUp()) {
    return false;
  }
  
  _initialized = true;
  return true;
}

bool PCMD3180::reset() {
  // Software reset
  if (!writeRegister(REG_SW_RESET, 0x01)) {
    return false;
  }
  
  // Wait 10ms after reset
  delay(10);

  // All registers are back at their defaults
  _sleepCfg = 0x00;
  
  return true;
}

/*
Utility methods
*/
bool PCMD3180::configureAsSlave(AudioFormat format, WordLength wordLen, uint8_t numChannels) {
  if (numChannels < 1 || numChannels > 8) {
    return false;
  }
  if (!setMasterMode(MODE_SLAVE)) return false;
  if (!setAutoClockConfigEnabled(true)) return false;
  if (!setAutoClockPLLEnabled(true)) return false;
  if (!setASIFormat(format, wordLen)) return false;
  if (!enableAllChannels(numChannels)) return false;
  return true;
}

bool PCMD3180::configureAsMaster(AudioFormat format, WordLength wordLen, FSRate fsyncRate, BCLKRatio bclkRatio, uint8_t numChannels, MCLKFrequency mclkFreq) {
  if (numChannels < 1 || numChannels > 8) {
    return false;
  }
  if (!setMasterMode(MODE_MASTER)) return false;
  if (!setMCLKFrequency(mclkFreq)) return false;
  if (!setASIClock(fsyncRate, bclkRatio)) return false;
  if (!setASIFormat(format, wordLen)) return false;
  if (!enableAllChannels(numChannels)) return false;
  return true;
}

bool PCMD3180::configurePDMInput(PDMClock clk, uint8_t numChannels) {
  if (numChannels < 1 || numChannels > 8) {
    return false;
  }
  if (!setPDMClock(clk)) return false;

  // Each port carries two channels: port 1 covers ch1+2, port 2 covers ch3+4, etc.
  // Per port: select PDM as the channels' input source, output PDMCLK on GPOx and
  // take PDM data in on GPIx (datasheet 8.2.1.2, step 3d-3f)
  uint8_t numPorts = (numChannels + 1) / 2;
  for (uint8_t port = 1; port <= numPorts; port++) {
    if (!enablePort(port, true)) return false;
    if (!setGPOMode(port, GPO_MODE_PDM, DRIVE_MODE_ACTIVE_LOW_ACTIVE_HIGH)) return false;
    if (!setGPIMode(port, (GPIMode)(GPI_MODE_PDMDIN1 + port - 1))) return false;
  }

  if (!enableAllChannels(numChannels)) return false;
  return true;
}

bool PCMD3180::enableAllChannels(uint8_t numChannels) {
  if (numChannels < 1 || numChannels > 8) {
    return false;
  }
  // Channels are mapped MSB-first: ch1 = bit 7, ch2 = bit 6, etc.
  uint8_t mask = (uint8_t)(0xFF00 >> numChannels);
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
bool PCMD3180::getLatchedInterruptStatus(bool &asiBusClockError, bool &pllLockError) {
  uint8_t status;
  if (!readRegister(REG_INT_LTCH0, status)) {
    return false;
  }
  asiBusClockError = status & 0x80;
  pllLockError = status & 0x40;
  return true;
}

bool PCMD3180::getAutodetectedClocks(FSRate &fsRate, uint32_t &ratio) {
  uint8_t status;
  if (!readRegister(REG_ASI_STS, status)) {
    return false;
  }
  uint8_t fsyncRate = (status & 0xF0) >> 4;
  uint8_t fsyncRatio = status & 0x0F;

  static const uint16_t ratioTable[] = {16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 1024, 2048};

  // FS_RATE_STS uses the same encoding as FSRate; 9-14 are reserved and 15 is invalid
  if (fsyncRate > FSRATE_768) {
    return false;
  }

  if (fsyncRatio >= sizeof(ratioTable)/sizeof(ratioTable[0])) {
    return false;
  }

  fsRate = (FSRate)fsyncRate;
  ratio = ratioTable[fsyncRatio];
  return true;
}

bool PCMD3180::getI2CChecksum(uint8_t &checksum) {
  return readRegister(REG_I2C_CKSUM, checksum);
}

bool PCMD3180::resetI2CChecksum(uint8_t value) {
  return writeRegister(REG_I2C_CKSUM, value);
}

bool PCMD3180::getGPIOMonitorValue(uint8_t &monitorValue) {
  uint8_t status;
  if (!readRegister(REG_GPIO_MON, status)) {
    return false;
  }
  monitorValue = (status & 0x80) >> 7;
  return true;
}

bool PCMD3180::getGPIMonitorValue(uint8_t port, uint8_t &monitorValue) {
  if (port < 1 || port > 4) {
    return false;
  }
  uint8_t status;
  if (!readRegister(REG_GPI_MON, status)) {
    return false;
  }
  uint8_t mask = 0x80 >> (port - 1);
  monitorValue = (status & mask) >> (8 - port);
  return true;
}

bool PCMD3180::getDeviceStatus(uint8_t &status0, uint8_t &deviceModeStatus) {
  if (!readRegister(REG_DEV_STS0, status0)) {
    return false;
  }
  uint8_t sts1;
  if (!readRegister(REG_DEV_STS1, sts1)) {
    return false;
  }
  deviceModeStatus = sts1 >> 5;
  return true;
}

/*
Sleep and wake (SLEEP_CFG SLEEP_ENZ)
*/
bool PCMD3180::wakeUp() {
  // Datasheet 6.4.2: no I2C transactions other than the exit write are allowed in sleep mode,
  // and AREG_SELECT must be configured while exiting sleep, so set SLEEP_ENZ and AREG_SELECT in
  // one write. VREF_QCHG and I2C_BRDCAST_EN are restored from the value saved by sleep().
  uint8_t sleepCfg = (_sleepCfg & 0x1C) | (_aregInternal ? 0x80 : 0x00) | 0x01;
  if (!writeRegister(REG_SLEEP_CFG, sleepCfg)) {
    return false;
  }
  // Wait >= 1ms for the internal wake-up sequence (datasheet 6.4.3)
  delay(1);

  if (_avddInputVoltage == AVDD_INPUT_18V){
    return updateRegisterBits(REG_BIAS_CFG, 0x03, 0x02);
  }
  return true;
}

bool PCMD3180::sleep() {
  uint8_t sleepCfg;
  if (!readRegister(REG_SLEEP_CFG, sleepCfg)) {
    return false;
  }
  // Save SLEEP_CFG so wakeUp() can restore it without reading in sleep mode
  _sleepCfg = sleepCfg & ~0x01;
  if (!writeRegister(REG_SLEEP_CFG, _sleepCfg)) {
    return false;
  }
  // Wait >= 10ms before the next I2C transaction (datasheet 6.4.2)
  delay(10);
  return true;
}

/*
SLEEP_CFG register methods
*/
bool PCMD3180::setAREG(bool internal) {
  _aregInternal = internal;
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
bool PCMD3180::setShutdownMode(ShutdownMode mode) {
  uint8_t cfg = 0x00;
  switch (mode) {
    case SHUTDOWN_DREG_IMMEDIATELY:
      cfg = 0x00;
      break;
    case SHUTDOWN_DREG_ACTIVE_UNTIL_TIMEOUT:
      cfg = 0x04;
      break;
    case SHUTDOWN_DREG_ACTIVE_UNTIL_SHUTDOWN:
      cfg = 0x08;
      break;
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
  uint8_t asiCfg0 = 0;
  
  // Set audio format
  switch (format) {
    case FORMAT_I2S:
      asiCfg0 |= 0x40;
      break;
    case FORMAT_TDM:
      asiCfg0 |= 0x00;
      break;
    case FORMAT_LEFT_JUSTIFIED:
      asiCfg0 |= 0x80;
      break;
  }
  
  // Set word length
  switch (wordLen) {
    case WORD_16_BIT:
      asiCfg0 |= 0x00;
      break;
    case WORD_20_BIT:
      asiCfg0 |= 0x10;
      break;
    case WORD_24_BIT:
      asiCfg0 |= 0x20;
      break;
    case WORD_32_BIT:
      asiCfg0 |= 0x30;
      break;
  }
  
  // Write configurations
  if (!updateRegisterBits(REG_ASI_CFG0, 0xF0, asiCfg0)) {
    return false;
  }
  return true;
}

bool PCMD3180::setASIPolarities(ASIPolarity fsyncPolarity, ASIPolarity bclkPolarity) {
  uint8_t asiCfg0 = 0;
  
  // Set FSYNC polarity
  switch (fsyncPolarity) {
    case ASI_POLARITY_STANDARD:
      break;
    case ASI_POLARITY_INVERTED:
      asiCfg0 |= 0x08;
      break;
    default:
      return false;
  }

  // Set BCLK polarity
  switch (bclkPolarity) {
    case ASI_POLARITY_STANDARD:
      break;
    case ASI_POLARITY_INVERTED:
      asiCfg0 |= 0x04;
      break;
    default:
      return false;
  }
  
  // Write configurations
  if (!updateRegisterBits(REG_ASI_CFG0, 0x0C, asiCfg0)) {
    return false;
  }
  return true;
}

bool PCMD3180::setASITXEdge(ASITXEdge edge) {
  return updateRegisterBits(REG_ASI_CFG0, 0x02, (edge == ASI_TX_EDGE_INVERTED) ? 0x02 : 0x00);
}

bool PCMD3180::setASITXFill(ASITXFill fill) {
  return updateRegisterBits(REG_ASI_CFG0, 0x01, (fill == ASI_TX_FILL_HI_Z) ? 0x01 : 0x00);
}

/*
ASI_CFG1 register methods
*/
bool PCMD3180::setASITXLSB(ASITXLSB lsb) {
  return updateRegisterBits(REG_ASI_CFG1, 0x80, (lsb == ASI_TX_LSB_HALF_CYCLE) ? 0x80 : 0x00);
}

bool PCMD3180::setASIBusKeeper(ASIBusKeeper keeper) {
  uint8_t cfg;
  switch (keeper) {
    case ASI_BUS_KEEPER_DISABLED:
      cfg = 0x00;
      break;
    case ASI_BUS_KEEPER_ALWAYS:
      cfg = 0x20;
      break;
    case ASI_BUS_KEEPER_LSB_ONE_CYCLE:
      cfg = 0x40;
      break;
    case ASI_BUS_KEEPER_LSB_ONE_HALF_CYCLES:
      cfg = 0x60;
      break;
    default:
      return false;
  }
  return updateRegisterBits(REG_ASI_CFG1, 0x60, cfg);
}

bool PCMD3180::setASITXOffset(uint8_t offset) {
  if (offset > 31) {
    return false;
  }
  return updateRegisterBits(REG_ASI_CFG1, 0x1F, offset);
}

/*
ASI_CFG2 register methods
*/
bool PCMD3180::setASIErrorDetection(bool enableErrorDetection, bool enableAutoResumeOnRecovery) {
  uint8_t asiCfg2 = 0;

  if (!enableErrorDetection) {
    asiCfg2 |= 0x20;
  }
  if (!enableAutoResumeOnRecovery) {
    asiCfg2 |= 0x10;
  }

  // Write configurations
  if (!updateRegisterBits(REG_ASI_CFG2, 0x30, asiCfg2)) {
    return false;
  }
  return true;
}

bool PCMD3180::setASIDaisyChained(bool enableDaisyChain) {
  return updateRegisterBits(REG_ASI_CFG2, 0x80, enableDaisyChain ? 0x80 : 0x00);
}


/*
ASI_CH1 to ASI_CH8 register methods
*/
bool PCMD3180::setASISlotAssignment(uint8_t channel, uint8_t slot) {
  if ((slot > 63) || (channel < 1 || channel > 8)) {
    return false;
  }
  uint8_t asiSlotReg = REG_ASI_CH1 + channel - 1;
  return updateRegisterBits(asiSlotReg, 0x3F, slot);
}

bool PCMD3180::setASIOutputLine(uint8_t channel, ASIPin asiPin) {
  if (channel < 1 || channel > 8) {
    return false;
  }
  uint8_t asiSlotReg = REG_ASI_CH1 + channel - 1;

  uint8_t cfg = 0x40;
  if (asiPin == ASIPIN_PRIMARY) {
    cfg = 0x00;  
  }
  return updateRegisterBits(asiSlotReg, 0x40, cfg);
}

/*
MST_CFG methods
*/
bool PCMD3180::setMasterMode(MasterMode masterMode) {
  bool master = (masterMode == MODE_MASTER);
  return updateRegisterBits(REG_MST_CFG0, 0x80, master ? 0x80 : 0x00);
}


bool PCMD3180::setMasterConfig(bool enableGatedFsyncAndBclk, FSMode fsMode) {
  uint8_t cfg = 0;
  if (enableGatedFsyncAndBclk) {
    cfg |= 0x10;
  }
  if (fsMode == FSYNC_MODE_44100) {
    cfg |= 0x08;
  }
  return updateRegisterBits(REG_MST_CFG0, 0x18, cfg);
}

bool PCMD3180::setASIClock(FSRate fsyncRate, BCLKRatio bclkRatio) {
  uint8_t cfg = 0;
  switch (fsyncRate) {
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

  switch (bclkRatio) {
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
Auto clock configuration (MST_CFG0) and slave mode clock source (CLK_SRC)
*/
bool PCMD3180::setAutoClockConfigEnabled(bool autoClockConfigEnabled) {
  return updateRegisterBits(REG_MST_CFG0, 0x40, autoClockConfigEnabled ? 0x00 : 0x40);
}

bool PCMD3180::setAutoClockPLLEnabled(bool pllEnabled) {
  return updateRegisterBits(REG_MST_CFG0, 0x20, pllEnabled ? 0x00 : 0x20);
}

bool PCMD3180::setDisabledPLLClockSource(PLLSlaveClockSource clkSource) {
  uint8_t cfg = 0;
  if (clkSource == PLLSLAVECLKSRC_MCLK) {
    cfg = 0x80;
  }
  return updateRegisterBits(REG_CLK_SRC, 0x80, cfg);
}

/*
MCLK configuration (MST_CFG0 and CLK_SRC)
*/
bool PCMD3180::setMCLKFrequency(MCLKFrequency freq) {
  if (freq > MCLK_FREQ_24576_KHZ) {
    return false;
  }
  if (!updateRegisterBits(REG_MST_CFG0, 0x07, (uint8_t)freq)) {
    return false;
  }
  // MCLK_FREQ_SEL_MODE = 0: MCLK frequency is taken from MCLK_FREQ_SEL
  return updateRegisterBits(REG_CLK_SRC, 0x40, 0x00);
}

bool PCMD3180::setMCLKRatio(MCLKRatio ratio) {
  if (ratio > MCLKRATIO_2304) {
    return false;
  }
  // MCLK_FREQ_SEL_MODE = 1: MCLK frequency is specified as a multiple of FSYNC in MCLK_RATIO_SEL
  return updateRegisterBits(REG_CLK_SRC, 0x78, 0x40 | ((uint8_t)ratio << 3));
}


/*
PDMCLK_CFG register methods
*/
bool PCMD3180::setPDMClock(PDMClock clk) {
  uint8_t pdmclkVal = 0;
  
  switch (clk) {
    case PDMCLK_2822_KHZ:
      pdmclkVal = 0X00;
      break;
    case PDMCLK_1411_KHZ:
      pdmclkVal = 0X01;
      break;
    case PDMCLK_705_KHZ:
      pdmclkVal = 0X02;
      break;
    case PDMCLK_5644_KHZ:
      pdmclkVal = 0X03;
      break;
  }
  
  return writeRegister(REG_PDMCLK_CFG, 0x40 | pdmclkVal);
}

/*
PDMIN_CFG register methods
*/
bool PCMD3180::setPortEdgeLatchMode(uint8_t port, EdgeLatchMode mode) {
  if (port < 1 || port > 4) {
    return false;
  }
  uint8_t mask = 0x80 >> (port - 1);
  uint8_t cfg = ((mode == EDGE_MODE_EVEN_POSITIVE) ? 0x00 : 0x80) >> (port - 1);
  return updateRegisterBits(REG_PDMIN_CFG, mask, cfg);
}

/*
GPIO_CFG0  register methods
*/
bool PCMD3180::setGPIOConfig(GPIOMode mode, DriveMode driveMode) {
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

  switch (driveMode) {
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
bool PCMD3180::setGPOMode(uint8_t port, GPOMode mode, DriveMode driveMode) {
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

  switch (driveMode) {
    case DRIVE_MODE_HI_Z:
      cfg |= 0;
      break;
    case DRIVE_MODE_ACTIVE_LOW_ACTIVE_HIGH:
      cfg |= 1;
      break;
    default:
      return false;
  }

  uint8_t gpoModeReg = REG_GPO_CFG0 + (port - 1);
  return writeRegister(gpoModeReg, cfg);
}

/*
GPO_VAL register methods
*/
bool PCMD3180::setGPIOVal(bool driveHigh) {
  return updateRegisterBits(REG_GPO_VAL, 0x80, driveHigh ? 0x80 : 0x00);
}

bool PCMD3180::setGPOVal(uint8_t port, bool driveHigh) {
  if (port < 1 || port > 4) {
    return false;
  }
  uint8_t cfg = driveHigh ? 0x40 : 0x00;
  cfg = cfg >> (port - 1);
  uint8_t mask = 0x40 >> (port - 1);
  return updateRegisterBits(REG_GPO_VAL, mask, cfg);
}

/*
GPI_CFG0 to GPI_CFG1 register methods
*/
bool PCMD3180::setGPIMode(uint8_t port, GPIMode mode) {
  if (port < 1 || port > 4) {
    return false;
  }
  uint8_t modeVal;
  switch (mode) {
    case GPI_MODE_DISABLED:
      modeVal = 0x00;
      break;
    case GPI_MODE_GPI:
      modeVal = 0x01;
      break;
    case GPI_MODE_MCLK:
      modeVal = 0x02;
      break;
    case GPI_MODE_ASI:
      modeVal = 0x03;
      break;
    case GPI_MODE_PDMDIN1:
      modeVal = 0x04;
      break;
    case GPI_MODE_PDMDIN2:
      modeVal = 0x05;
      break;
    case GPI_MODE_PDMDIN3:
      modeVal = 0x06;
      break;
    case GPI_MODE_PDMDIN4:
      modeVal = 0x07;
      break;
    default:
      return false;
  }
  uint8_t gpiModeReg = REG_GPI_CFG0 + (port - 1) / 2; // Port 1 and 2 uses register 2B, port 3 and 4 uses 2C
  uint8_t mask = 0b00000111;
  if ((port == 1) || (port == 3)) {
    modeVal <<= 4; // The mode value is written to bits 6-4 for port 1 and 3, and bits 2-0 for port 2 and 4
    mask <<= 4;
  }
  return updateRegisterBits(gpiModeReg, mask, modeVal);
}

/*
INT_CFG and INT_MASK0 register methods
*/
bool PCMD3180::setInterruptConfig(INTMode mode, INTLatchReadMode latchMode, INTPolarity polarity) {
  uint8_t cfg = 0;
  switch (mode) {
    case INT_MODE_ASSERT_CONSTANT:
      cfg |= 0x00;
      break;
    case INT_MODE_ASSERT_2MS_PULSED:
      cfg |= 0x02 << 5;
      break;
    case INT_MODE_ASSERT_2MS_ONCE:
      cfg |= 0x03 << 5;
      break;
    default:
      return false;
  }

  if (latchMode == INT_LATCH_MODE_READ_ONLY_UNMASKED){
    cfg |= 0x04;
  }

  if (polarity == INT_POLARITY_ACTIVE_HIGH) {
    cfg |= 0x80;
  }

  return writeRegister(REG_INT_CFG, cfg);
}

bool PCMD3180::setInterruptMasks(bool maskAsiClockError, bool maskPllLockInterrupt) {
  uint8_t cfg = 0x00;
  if (maskAsiClockError){
    cfg |= 0x80;
  }

  if (maskPllLockInterrupt){
    cfg |= 0x40;
  }

  return updateRegisterBits(REG_INT_MASK0, 0xC0, cfg);
}

/*
BIAS_CFG register methods
*/
bool PCMD3180::configureBias(MICBIASMode micbiasMode, VREFMode vrefMode) {
  uint8_t cfg = 0x00;
  switch (micbiasMode) {
    case MICBIAS_MODE_AVDD:
      cfg |= 0x60;
      break;
    case MICBIAS_MODE_VREF:
      cfg |= 0x00;
      break;
    default:
      return false;
  };

  switch (vrefMode) {
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

  // Only channels 1-4 have a selectable input source; channels 5-8 (ports 3 and 4) are always PDM
  if (port > 2) {
    return true;
  }

  // CHx_INSRC, bits 6:5: 2 = digital microphone PDM input, 0 = input source not enabled
  uint8_t insrc = enable ? 0x40 : 0x00;
  uint8_t firstChannelReg = REG_CH1_CFG0 + 10 * (port - 1);
  if (!updateRegisterBits(firstChannelReg, 0x60, insrc)) {
    return false;
  }
  return updateRegisterBits(firstChannelReg + 5, 0x60, insrc);
}

bool PCMD3180::setDigitalVolume(uint8_t channel, uint8_t volume) {
  if (channel < 1 || channel > 8) {
    return false;
  }
  
  // Calculate register address for channel volume
  uint8_t volReg = REG_CH1_CFG2 + ((channel - 1) * 5);
  
  return writeRegister(volReg, volume);
}

bool PCMD3180::setGainCalibration(uint8_t channel, uint8_t gainCalibration) {
  if (channel < 1 || channel > 8) {
    return false;
  }
  
  if (gainCalibration > 15) {
    return false;
  }

  // Calculate register address for channel gain calibration
  uint8_t gainReg = REG_CH1_CFG3 + ((channel - 1) * 5);
  
  return writeRegister(gainReg, gainCalibration << 4);
}

bool PCMD3180::setPhaseCalibration(uint8_t channel, uint8_t phaseCalibration) {
  if (channel < 1 || channel > 8) {
    return false;
  }

  // Calculate register address for channel phase calibration
  uint8_t phaseReg = REG_CH1_CFG4 + ((channel - 1) * 5);
  
  return writeRegister(phaseReg, phaseCalibration);
}

/*
DSP_CFG0 register methods
*/
bool PCMD3180::setDecimationFilterMode(FilterMode mode) {
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

bool PCMD3180::setChannelSummationMode(SummationMode mode) {
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

bool PCMD3180::setHighpassFilterMode(HPFilterMode mode) {
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
bool PCMD3180::setDigitalVolumeConfig(bool gangedDigitalVolume) {
  return updateRegisterBits(REG_DSP_CFG1, 0x80, gangedDigitalVolume ? 0x80 : 0x00);
}

bool PCMD3180::setBiquadConfig(uint8_t numBiquads) {
  if (numBiquads > 3) {
    return false;
  }
  uint8_t cfg = numBiquads << 5;
  return updateRegisterBits(REG_DSP_CFG1, 0x60, cfg);
}

bool PCMD3180::setSoftStepEnabled(bool softStepEnabled) {
  return updateRegisterBits(REG_DSP_CFG1, 0x10, softStepEnabled ? 0x00 : 0x10);
}

/*
IN_CH_EN register methods
*/
bool PCMD3180::enableChannels(uint8_t chMask) {
  // IN_CH_EN register - bit 7 = CH1, bit 6 = CH2, etc.
  return writeRegister(REG_IN_CH_EN, chMask);
}

/*
ASI_OUT_CH_EN register methods
*/
bool PCMD3180::enableOutputASIChannels(uint8_t chMask) {
  // ASI_OUT_CH_EN register - bit 7 = CH1, bit 6 = CH2, etc.
  return writeRegister(REG_ASI_OUT_CH_EN, chMask);
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

bool PCMD3180::setDynamicPowerUpConfig(bool enableDynamicPowerUp, DynamicPowerMode mode) {
  uint8_t cfg = enableDynamicPowerUp ? 0x10 : 0x00;

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
  if (_shdnzPin == 0xFF) {
    return false;
  }
  digitalWrite(_shdnzPin, HIGH);
  delay(1);
  // Releasing SHDNZ resets all registers to their defaults
  _sleepCfg = 0x00;
  return true;
}

bool PCMD3180::hardwarePowerDown() {
  if (_shdnzPin == 0xFF) {
    return false;
  }
  digitalWrite(_shdnzPin, LOW);
  delay(50);
  return true;
}




bool PCMD3180::writeRegister(uint8_t reg, uint8_t value) {
  if (!_wire) {
    return false;
  }
  
  _wire->beginTransmission(_i2cAddr);
  _wire->write(reg);
  _wire->write(value);
  
  return (_wire->endTransmission() == 0);
}

bool PCMD3180::readRegister(uint8_t reg, uint8_t &value) {
  if (!_wire) {
    return false;
  }
  
  _wire->beginTransmission(_i2cAddr);
  _wire->write(reg);
  
  if (_wire->endTransmission(false) != 0) {
    return false;
  }
  
  if (_wire->requestFrom(_i2cAddr, (uint8_t)1) != 1) {
    return false;
  }
  
  value = _wire->read();
  return true;
}

bool PCMD3180::updateRegisterBits(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t currentVal;
  
  if (!readRegister(reg, currentVal)) {
    return false;
  }
  
  uint8_t newVal = (currentVal & ~mask) | (value & mask);
  
  return writeRegister(reg, newVal);
}

/*
Programmable coefficient methods (pages 2-4)
*/
bool PCMD3180::writeCoefficients(uint8_t page, uint8_t reg, const int32_t *values, uint8_t count) {
  // Coefficients are on pages 2-4 at registers 0x08-0x7F; stay within one page
  if (!_wire || page < 2 || page > 4 || reg < 0x08 || count == 0 || reg + 4 * count - 1 > 0x7F) {
    return false;
  }
  if (!writeRegister(REG_PAGE_SELECT, page)) {
    return false;
  }

  // Each coefficient is written as four bytes, most significant byte first (datasheet 7.2)
  _wire->beginTransmission(_i2cAddr);
  _wire->write(reg);
  for (uint8_t i = 0; i < count; i++) {
    uint32_t value = (uint32_t)values[i];
    _wire->write((uint8_t)(value >> 24));
    _wire->write((uint8_t)(value >> 16));
    _wire->write((uint8_t)(value >> 8));
    _wire->write((uint8_t)value);
  }
  bool success = (_wire->endTransmission() == 0);

  // Always return to page 0, which the rest of the library uses
  if (!writeRegister(REG_PAGE_SELECT, 0)) {
    return false;
  }
  return success;
}

bool PCMD3180::readCoefficients(uint8_t page, uint8_t reg, int32_t *values, uint8_t count) {
  if (!_wire || page < 2 || page > 4 || reg < 0x08 || count == 0 || reg + 4 * count - 1 > 0x7F) {
    return false;
  }
  if (!writeRegister(REG_PAGE_SELECT, page)) {
    return false;
  }

  bool success = false;
  _wire->beginTransmission(_i2cAddr);
  _wire->write(reg);
  if (_wire->endTransmission(false) == 0 &&
      _wire->requestFrom(_i2cAddr, (uint8_t)(4 * count)) == 4 * count) {
    for (uint8_t i = 0; i < count; i++) {
      uint32_t value = 0;
      for (uint8_t b = 0; b < 4; b++) {
        value = (value << 8) | (uint8_t)_wire->read();
      }
      values[i] = (int32_t)value;
    }
    success = true;
  }

  if (!writeRegister(REG_PAGE_SELECT, 0)) {
    return false;
  }
  return success;
}

bool PCMD3180::writeCoefficient(uint8_t page, uint8_t reg, int32_t value) {
  return writeCoefficients(page, reg, &value, 1);
}

bool PCMD3180::readCoefficient(uint8_t page, uint8_t reg, int32_t &value) {
  return readCoefficients(page, reg, &value, 1);
}

bool PCMD3180::setBiquadCoefficients(uint8_t biquad, int32_t n0, int32_t n1, int32_t n2, int32_t d1, int32_t d2) {
  if (biquad < 1 || biquad > 12) {
    return false;
  }
  // Biquads 1-6 are on page 2 and 7-12 on page 3, 20 bytes each starting at 0x08
  uint8_t page = (biquad <= 6) ? 2 : 3;
  uint8_t reg = 0x08 + 20 * ((biquad - 1) % 6);
  const int32_t values[5] = {n0, n1, n2, d1, d2};
  return writeCoefficients(page, reg, values, 5);
}

bool PCMD3180::setMixerCoefficient(uint8_t mixer, uint8_t inputChannel, int32_t value) {
  if (mixer < 1 || mixer > 4 || inputChannel < 1 || inputChannel > 4) {
    return false;
  }
  // Page 4: mixer m, input channel c at 0x08 + 16 * (m - 1) + 4 * (c - 1)
  uint8_t reg = 0x08 + 16 * (mixer - 1) + 4 * (inputChannel - 1);
  return writeCoefficients(4, reg, &value, 1);
}

bool PCMD3180::setHPFCoefficients(int32_t n0, int32_t n1, int32_t d1) {
  // Page 4: IIR_N0 at 0x48, IIR_N1 at 0x4C, IIR_D1 at 0x50
  const int32_t values[3] = {n0, n1, d1};
  return writeCoefficients(4, 0x48, values, 3);
}

bool PCMD3180::isConnected() {
  if (!_wire) {
    return false;
  }
  
  _wire->beginTransmission(_i2cAddr);
  return (_wire->endTransmission() == 0);
}