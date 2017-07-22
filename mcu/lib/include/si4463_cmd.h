#define RFM_CMD_NOP                        0x00
#define RFM_CMD_PART_INFO                  0x01
#define RFM_CMD_POWER_UP                   0x02
#define RFM_CMD_PATCH_IMAGE                0x04
#define RFM_CMD_FUNC_INFO                  0x10
#define RFM_CMD_SET_PROPERTY               0x11
#define RFM_CMD_GET_PROPERTY               0x12
#define RFM_CMD_GPIO_PIN_CFG               0x13
#define RFM_CMD_GET_ADC_READING            0x14
#define RFM_CMD_FIFO_INFO                  0x15
#define RFM_CMD_PACKET_INFO                0x16
#define RFM_CMD_IRCAL                      0x17
#define RFM_CMD_PROTOCOL_CFG               0x18
#define RFM_CMD_GET_INT_STATUS             0x20
#define RFM_CMD_GET_PH_STATUS              0x21
#define RFM_CMD_GET_MODEM_STATUS           0x22
#define RFM_CMD_GET_CHIP_STATUS            0x23
#define RFM_CMD_START_TX                   0x31
#define RFM_CMD_START_RX                   0x32
#define RFM_CMD_REQUEST_DEVICE_STATE       0x33
#define RFM_CMD_CHANGE_STATE               0x34
#define RFM_CMD_RX_HOP                     0x36
#define RFM_CMD_READ_BUF                   0x44
#define RFM_CMD_FAST_RESPONSE_A            0x50
#define RFM_CMD_FAST_RESPONSE_B            0x51
#define RFM_CMD_FAST_RESPONSE_C            0x53
#define RFM_CMD_FAST_RESPONSE_D            0x57
#define RFM_CMD_WRITE_TX_FIFO              0x66
#define RFM_CMD_READ_RX_FIFO               0x77

#define RFM_STATE_DONTCHANGE 0
#define RFM_STATE_SLEEP      1
#define RFM_STATE_SPI        2
#define RFM_STATE_READY      3
#define RFM_STATE_TX_TUNE    5
#define RFM_STATE_RX_TUNE    6
#define RFM_STATE_TX         7
#define RFM_STATE_RX         8

#define RFM_INT_GRP_CHIP                (1 << 26)
#define RFM_INT_GRP_MODEM               (1 << 25)
#define RFM_INT_GRP_PH                  (1 << 24)

#define RFM_INT_PH_MATCH                (1 << 23)
#define RFM_INT_PH_MISS                 (1 << 22)
#define RFM_INT_PH_SENT                 (1 << 21)
#define RFM_INT_PH_RX                   (1 << 20)
#define RFM_INT_PH_CRC_ERROR            (1 << 19)
#define RFM_INT_PH_TX_FIFO_ALMOST_EMPTY (1 << 17)
#define RFM_INT_PH_RX_FIFO_ALMOST_FULL  (1 << 16)

#define RFM_INT_MODEM_POSTAMBLE_DETECT  (1 << 14)
#define RFM_INT_MODEM_INVALID_SYNC      (1 << 13)
#define RFM_INT_MODEM_RSSI_JUMP         (1 << 12)
#define RFM_INT_MODEM_RSSI              (1 << 11)
#define RFM_INT_MODEM_INVALID_PREAMBLE  (1 << 10)
#define RFM_INT_MODEM_PREAMBLE_DETECT   (1 <<  9)
#define RFM_INT_MODEM_SYNC_DETECT       (1 <<  8)

#define RFM_INT_CHIP_CAL                (1 <<  6)
#define RFM_INT_CHIP_UF_OF_ERROR        (1 <<  5)
#define RFM_INT_CHIP_STATE_CHANGED      (1 <<  4)
#define RFM_INT_CHIP_CMD_ERROR          (1 <<  3)
#define RFM_INT_CHIP_READY              (1 <<  2)
#define RFM_INT_CHIP_LOW_BATT           (1 <<  1)
#define RFM_INT_CHIP_WUT                (1 <<  0)

// Incomplete, see api spec for more information
#define RFM_GPIO_DONTCHANGE 0
#define RFM_GPIO_TRISTATE   1
#define RFM_GPIO_LOW        2
#define RFM_GPIO_HIGH       3
#define RFM_GPIO_INPUT      4
