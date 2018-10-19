#define SX127x_WriteReg               0x80

#define SX127x_RegFifo                0x00 // FIFO read/write access
#define SX127x_RegOpMode              0x01 // Operating mode & LoRa / FSK selection
#define SX127x_RegFrfMsb              0x06 // RF Carrier Frequency, Most Significant Bits
#define SX127x_RegFrfMid              0x07 // RF Carrier Frequency, Intermediate Bits
#define SX127x_RegFrfLsb              0x08 // RF Carrier Frequency, Least Significant Bits
#define SX127x_RegPaConfig            0x09 // PA selection and Output Power control
#define SX127x_RegPaRamp              0x0A // Control of PA ramp time, low phase noise PLL
#define SX127x_RegOcp                 0x0B // Over Current Protection control
#define SX127x_RegLna                 0x0C // LNA settings
#define SX127x_RegFifoAddrPtr         0x0D // FIFO SPI pointer
#define SX127x_RegFifoTxBaseAddr      0x0E // Start Tx data
#define SX127x_RegFifoRxBaseAddr      0x0F // Start Rx data
#define SX127x_RegFifoRxCurrentAddr   0x10 // Start address of last packet received
#define SX127x_RegIrqFlagsMask        0x11 // Optional IRQ flag mask
#define SX127x_RegIrqFlags            0x12 // IRQ flags
#define SX127x_RegRxNbBytes           0x13 // Number of received bytes
#define SX127x_RegRxHeaderCntValueMsb 0x14 // Number of valid headers received
#define SX127x_RegRxHeaderCntValueLsb 0x15
#define SX127x_RegRxPacketCntValueMsb 0x16 // Number of valid packets received
#define SX127x_RegRxPacketCntValueLsb 0x17
#define SX127x_RegModemStat           0x18 // Live LoRa modem status
#define SX127x_RegPktSnrValue         0x19 // Espimation of last packet SNR
#define SX127x_RegPktRssiValue        0x1A // RSSI of last packet
#define SX127x_RegRssiValue           0x1B // Current RSSI
#define SX127x_RegHopChannel          0x1C // FHSS start channel
#define SX127x_RegModemConfig1        0x1D // Modem PHY config 1
#define SX127x_RegModemConfig2        0x1E // Modem PHY config 2
#define SX127x_RegSymbTimeoutLsb      0x1F // Receiver timeout value
#define SX127x_RegPreambleMsb         0x20 // Size of preamble
#define SX127x_RegPreambleLsb         0x21 // RegPayloadLength
#define SX127x_RegPayloadLength       0x22 // LoRa payload length
#define SX127x_RegMaxPayloadLength    0x23 // LoRaTM maximum payload length
#define SX127x_RegHopPeriod           0x24 // FHSS Hop period
#define SX127x_RegFifoRxByteAd        0x25 // Address of last byte written in FIFO
#define SX127x_RegModemConfig3        0x26 // Modem PHY config 3
#define SX127x_RegFeiMsb              0x28 // Estimated frequency error
#define SX127x_RegFeiMid              0x29
#define SX127x_RegFeiLsb              0x2A
#define SX127x_RegRssiWideband        0x2C // Wideband RSSI measurement
#define SX127x_RegDetectOptimize      0x31 // LoRa detection Optimize for SF6
#define SX127x_RegInvertIQ            0x33 // Invert LoRa I and Q signals
#define SX127x_RegDetectionThreshold  0x37 // LoRa detection threshold for SF6
#define SX127x_RegSyncWord            0x39 // LoRa Sync Word

#define SX127x_RegOpMode_LongRangeMode      0x80
#define SX127x_RegOpMode_AccessSharedReg    0x40
#define SX127x_RegOpMode_LowFrequencyModeOn 0x08
#define SX127x_RegOpMode_Mode_Mask          0x07
#define SX127x_RegOpMode_Mode_SLEEP         0x00
#define SX127x_RegOpMode_Mode_STDBY         0x01
#define SX127x_RegOpMode_Mode_FSTX          0x02
#define SX127x_RegOpMode_Mode_TX            0x03
#define SX127x_RegOpMode_Mode_FSRX          0x04
#define SX127x_RegOpMode_Mode_RXCONTINUOUS  0x05
#define SX127x_RegOpMode_Mode_RXSINGLE      0x06
#define SX127x_RegOpMode_Mode_CAD           0x07


#define SX127x_RegPaConfig_PaSelect         0x80
#define SX127x_RegPaConfig_MaxPower_Mask    0x70
#define SX127x_RegPaConfig_MaxPower_Pos     0x04

#define SX127x_RegPaConfig_OutputPower_Mask 0x0f
#define SX127x_RegPaConfig_OutputPower_Pos  0x00

#define SX127x_RegModemStat_RxCodingRate_Mask              0xe0
#define SX127x_RegModemStat_RxCodingRate_Pos               0x05
#define SX127x_RegModemStat_ModemStatus_ModemClear         0x10
#define SX127x_RegModemStat_ModemStatus_HeaderInfoValid    0x08
#define SX127x_RegModemStat_ModemStatus_RxOngoing          0x04
#define SX127x_RegModemStat_ModemStatus_SignalSynchronized 0x02
#define SX127x_RegModemStat_ModemStatus_SignalDetected     0x01

#define SX127x_RegIrqFlags_RxTimeout         0x80
#define SX127x_RegIrqFlags_RxDone            0x40
#define SX127x_RegIrqFlags_PayloadCRCError   0x20
#define SX127x_RegIrqFlags_ValidHeader       0x10
#define SX127x_RegIrqFlags_TxDone            0x80
#define SX127x_RegIrqFlags_CadDone           0x04
#define SX127x_RegIrqFlags_FhssChangeChannel 0x02
#define SX127x_RegIrqFlags_CadDetected       0x01

#define SX127x_RegModemConfig1_Bw_Mask              0xf0
#define SX127x_RegModemConfig1_Bw_Pos               0x04
#define SX127x_RegModemConfig1_CodingRate_Mask      0x0e
#define SX127x_RegModemConfig1_CodingRate_Pos       0x01
#define SX127x_RegModemConfig1_ImplicitHeaderModeOn 0x01

#define SX127x_RegModemConfig2_SpreadingFactor_Mask 0xf0
#define SX127x_RegModemConfig2_SpreadingFactor_Pos  0x04
#define SX127x_RegModemConfig2_TxContinuousMode     0x08
#define SX127x_RegModemConfig2_RxPayloadCrcOn       0x04
#define SX127x_RegModemConfig2_SymbTimeout98_Mask   0x03
#define SX127x_RegModemConfig2_SymbTimeout98_Pos    0x00

#define SX127x_RegModemConfig3_LowDataRateOptimize 0x08
#define SX127x_RegModemConfig3_AgcAutoOn           0x04

#define SX127x_RegDetectionOptimize_SF7_To_SF12    0x03
#define SX127x_RegDetectionOptimize_SF6            0x05
