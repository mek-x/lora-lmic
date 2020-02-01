#pragma once

#include <stdint.h>
#include <stdbool.h>

void sim_setNss(uint8_t v);
bool sim_isInt();
uint8_t sim_spiTransaction(uint8_t value);
