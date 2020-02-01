#pragma once

#include <stdint.h>
#include <stdbool.h>

void mocksInit();

uint8_t getRssi(bool write, void *reg);
