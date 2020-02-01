#include <stdlib.h>

#include "mocks.h"


uint8_t getRssi(bool write, void *reg) {
    return (uint8_t)rand();
}
