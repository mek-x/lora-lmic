#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include <unistd.h>

#include "sx1276sim.h"
#include "mocks.h"
#include "udp.h"

#define SIM_DBG(m, s, ...) fprintf(stderr, "[SIM] " m ": " s "\n", ##__VA_ARGS__)
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*(a)))

#define OPMODE_MASK      0x07
#define OPMODE_SLEEP     0x00
#define OPMODE_STANDBY   0x01
#define OPMODE_TX        0x03
#define OPMODE_RX        0x05
#define OPMODE_RX_SINGLE 0x06

#define IRQ_LORA_RXTOUT_MASK 0x80
#define IRQ_LORA_RXDONE_MASK 0x40
#define IRQ_LORA_TXDONE_MASK 0x08

#define RegFifo                                    0x00 // common
#define RegOpMode                                  0x01 // common
#define RegFrfMsb                                  0x06 // common
#define RegFrfMid                                  0x07 // common
#define RegFrfLsb                                  0x08 // common
#define RegPaConfig                                0x09 // common
#define RegPaRamp                                  0x0A // common
#define RegLna                                     0x0C // common
#define LORARegFifoAddrPtr                         0x0D
#define LORARegFifoTxBaseAddr                      0x0E
#define LORARegIrqFlagsMask                        0x11
#define LORARegIrqFlags                            0x12
#define LORARegModemConfig1                        0x1D
#define LORARegModemConfig2                        0x1E
#define LORARegSymbTimeoutLsb                      0x1F
#define LORARegPayloadLength                       0x22
#define LORARegRssiWideband                        0x2C
#define LORARegPayloadMaxLength                    0x23
#define LORARegModemConfig3                        0x26
#define LORARegInvertIQ                            0x33
#define LORARegSyncWord                            0x39
#define RegDioMapping1                             0x40 // common
#define RegVersion                                 0x42 // common
#define RegPaDac                                   0x5A // common

typedef enum {
    WAITING,
    PROCESSING_READ,
    PROCESSING_WRITE,
    PROCESSING_FIFO,
} spi_state_t;

static spi_state_t state = WAITING; // initial SPI state machine state

typedef struct _register_t{
    const uint8_t reg;
    uint8_t val;
    uint8_t (*callback)(bool write, void *reg);
} register_t;

uint8_t handleMode(bool w, void *reg);
uint8_t handleIntReg(bool w, void *reg);

register_t registers[] = {
//   OffsetName,                DefValue,   Callback
    {RegFifo,                   0x00,       NULL},
    {RegOpMode,                 0x01,       handleMode},
    {RegFrfMsb,                 0x6C,       NULL},
    {RegFrfMid,                 0x80,       NULL},
    {RegFrfLsb,                 0x00,       NULL},
    {RegPaConfig,               0x4F,       NULL},
    {RegPaRamp,                 0x09,       NULL},
    {RegLna,                    0x20,       NULL},
    {LORARegFifoAddrPtr,        0x08,       NULL},
    {LORARegFifoTxBaseAddr,     0x02,       NULL},
    {LORARegIrqFlagsMask,       0x00,       NULL},
    {LORARegIrqFlags,           0x15,       handleIntReg},
    {LORARegModemConfig1,       0x00,       NULL},
    {LORARegModemConfig2,       0x00,       NULL},
    {LORARegSymbTimeoutLsb,     0x00,       NULL},
    {LORARegPayloadLength,      0x00,       NULL},
    {LORARegRssiWideband,       0x55,       getRssi},
    {LORARegPayloadMaxLength,   0x00,       NULL},
    {LORARegModemConfig3,       0x03,       NULL},
    {LORARegInvertIQ,           0x40,       NULL},
    {LORARegSyncWord,           0xF5,       NULL},
    {RegDioMapping1,            0x00,       NULL},
    {RegVersion,                0x12,       NULL},
    {RegPaDac,                  0x84,       NULL},
};

static uint8_t fifoBuffer[256];
static uint8_t fifoIdx = 0;

static bool interrupt = false;

int getRegisterIdx(uint8_t off) {
    for (int i = 0; i < ARRAY_SIZE(registers); i++) {
        if (registers[i].reg == off) {
            return i;
        }
    }

    return -1;
}

uint8_t handleMode(bool w, void *r) {
    register_t *reg = r;

    uint8_t val = reg->val;
    if (!w) {
        return val;
    }

    int regInt = getRegisterIdx(LORARegIrqFlags);

    if ((val & OPMODE_MASK) == OPMODE_TX) {
        SIM_DBG("TX", "transmitting %d bytes", fifoIdx);
        transmit(fifoBuffer, fifoIdx);
        fifoIdx = 0;
        reg->val = (val & (~OPMODE_MASK)) | OPMODE_STANDBY;
        registers[regInt].val = IRQ_LORA_TXDONE_MASK;
        interrupt = true;
    } else if ((val & OPMODE_MASK) == (OPMODE_RX_SINGLE)) {
        SIM_DBG("RX", "receiving");
        receive(fifoBuffer + fifoIdx, ARRAY_SIZE(fifoBuffer) - fifoIdx);
        reg->val = (val & (~OPMODE_MASK)) | OPMODE_STANDBY;
        registers[regInt].val = IRQ_LORA_RXDONE_MASK;
        interrupt = true;
    } /*else if ((val & OPMODE_MASK) == (6)) {
        SIM_DBG("RX", "receiving scan");
        receive(fifoBuffer + fifoIdx, ARRAY_SIZE(fifoBuffer) - fifoIdx);
        reg->val = (val & (~OPMODE_MASK)) | OPMODE_STANDBY;
        registers[regInt].val = IRQ_LORA_RXDONE_MASK;
        interrupt = true;
    } */

    return val;
}

uint8_t handleIntReg(bool w, void *r) {
    register_t *reg = r;

    if (w) {
        reg->val ^= 0xFF;
        interrupt = false;
    }

    return reg->val;
}

static uint8_t readReg(uint8_t off) {
    int reg;
    uint8_t val;

    if ((reg = getRegisterIdx(off)) < 0) {
        SIM_DBG("REG", "bad register offset: 0x%02x", off);
        assert(0);
        return 0xff;
    }

    SIM_DBG("REG", "read 0x%02x from 0x%02x", val, off);

    if (registers[reg].callback != NULL) {
        val = registers[reg].callback(false, &registers[reg]);
    } else {
        val = registers[reg].val;
    }

    return val;
}

static void writeReg(uint8_t off, uint8_t val) {
    int reg;

    if (off == RegFifo && state != PROCESSING_FIFO) {  // access to FIFO register
        state = PROCESSING_FIFO;
    }

    if (state == PROCESSING_FIFO) { //processing fifo

        SIM_DBG("FIFO", "0x%02x", val);

        fifoBuffer[fifoIdx++] = val;

        return;
    }

    if ((reg = getRegisterIdx(off)) < 0) {
        SIM_DBG("REG", "bad register offset: 0x%02x", off);
        assert(0);
        return;
    }

    SIM_DBG("REG", "write 0x%02x to 0x%02x", val, off);

    registers[reg].val = val;

    if (registers[reg].callback != NULL) {
        registers[reg].callback(true, &registers[reg]);
    }

    return;
}

void sim_setNss(uint8_t v) {
    //SIM_DBG("PIN", "NSS set %s", v ? "high" : "low");

    state = WAITING;
}

bool sim_isInt() {

    sleep(1);

    if (!interrupt) {
        SIM_DBG("INT", "waiting for int");
    } else {
        SIM_DBG("INT", "interrupt triggered");
    }

    return interrupt;
}

uint8_t sim_spiTransaction(uint8_t value) {
    static uint8_t buf = 0x00;
    uint8_t tmp = buf;

    if (state == WAITING) {
        if (value & 0x80) {
            value &= ~0x80;
            state = PROCESSING_WRITE;
        } else {
            state = PROCESSING_READ;
        }
        buf = value;
    } else {
        if (state == PROCESSING_READ) {
            tmp = readReg(buf);
        } else if (state == PROCESSING_WRITE || state == PROCESSING_FIFO) {
            writeReg(buf, value);
        }
    }

    return tmp;
}
