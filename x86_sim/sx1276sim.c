#include <stdio.h>

#include "sx1276sim.h"

#define SIM_DBG(m, s, ...) fprintf(stderr, "[SIM] " m ": " s "\n", ##__VA_ARGS__)
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*(a)))

#define RegOpMode                                  0x01 // common
#define RegVersion                                 0x42 // common

typedef enum {
    WAITING,
    PROCESSING_READ,
    PROCESSING_WRITE,
} spi_state_t;

typedef struct {
    const uint8_t reg;
    uint8_t val;
} register_t;

register_t registers[] = {
    {RegOpMode,  0x01},
    {RegVersion, 0x12},
};

uint8_t readReg(uint8_t off) {
    uint8_t val;
    for (int i = 0; i < ARRAY_SIZE(registers); i++) {
        if (registers[i].reg == off) {
            val = registers[i].val;
            SIM_DBG("REG", "read 0x%02x from 0x%02x", val, off);
            return val;
        }
    }

    SIM_DBG("REG", "bad register offset: 0x%02x", off);
    return 0xFF;
}

void writeReg(uint8_t off, uint8_t val) {
    for (int i = 0; i < ARRAY_SIZE(registers); i++) {
        if (registers[i].reg == off) {
            registers[i].val = val;
            SIM_DBG("REG", "write 0x%02x to 0x%02x", val, off);
            return;
        }
    }

    SIM_DBG("REG", "bad register write offset: 0x%02x", off);
}

void sim_set_nss(uint8_t v) {

}

uint8_t sim_spi_transaction(uint8_t value) {
    static uint8_t buf = 0x00;
    static spi_state_t state = WAITING;

    uint8_t tmp = buf;

    SIM_DBG("SPI", "0x%02x", value);

    if (state == WAITING) {
        if (value & 0x80) {
            value &= ~0x80;
            SIM_DBG("SPI", "write");
            state = PROCESSING_WRITE;
        } else {
            SIM_DBG("SPI", "read");
            state = PROCESSING_READ;
        }
        buf = value;
    } else {
        if (state == PROCESSING_READ) {
            tmp = readReg(buf);
        } else if (state == PROCESSING_WRITE) {
            writeReg(buf, value);
        }
        state = WAITING;
    }

    return tmp;
}
