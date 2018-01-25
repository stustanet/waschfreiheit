#pragma once

#include "net/lora.h"

// RF configuration
// See the SX127X datasheet for details.
#define SX127X_CONFIG_LORA_SPREAD_MAX    12
#define SX127X_CONFIG_LORA_SPREAD_MIN    7
#define SX127X_CONFIG_LORA_CODERATE_MAX  3

// TX Power in db (max 10db (10 mW) allowed)
#define SX127X_CONFIG_LORA_POWER_MAX     10

// Freqency limitations to stay within regulatury limitations
#define SX127X_CONFIG_LORA_FREQUENCY_MIN 433200000
#define SX127X_CONFIG_LORA_FREQUENCY_MAX 434600000
