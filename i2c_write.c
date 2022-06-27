/* implements a non-blocking i2c_write() function using the samd21 or samd51's sercom peripheral and
 an interrupt handler. we could use DMA, but the i2c peripheral really only wants to be used with
 DMA if transactions are 255 or fewer bytes in length, and we have transactions of up to 1025 bytes
 in the case of the 128x64 oled */
#include "i2c_write.h"
#include "samd.h"

/* board-specific settings, i.e. which sercom and which pins and pinmux settings to use. procedure
 for adding a board here is to look at its variant.cpp file from the respective arduino core, and
 determine which sercom is used for the i2c sda/scl pins, and which port and pin numbers they are in
 the chip's internal numbering system, and what pinmux setting attaches them to the given sercom.
 then, copy and paste from these examples and modify as appropriate */

#ifdef SERCOM
/* allow compiler invocation to override these macros */
#elif defined(ADAFRUIT_FEATHER_M0)
/* tested */
#define SERCOM SERCOM3
#define SERCOM_GCLK_ID_CORE SERCOM3_GCLK_ID_CORE
#define SERCOM_IRQn SERCOM3_IRQn
#define SERCOM_HANDLER SERCOM3_Handler
#define SDA_PINMUX 0, 22, 2
#define SCL_PINMUX 0, 23, 2

#elif defined(ARDUINO_QTPY_M0)
/* tested */
#define SERCOM SERCOM1
#define SERCOM_GCLK_ID_CORE SERCOM1_GCLK_ID_CORE
#define SERCOM_IRQn SERCOM1_IRQn
#define SERCOM_HANDLER SERCOM1_Handler
#define SDA_PINMUX 0, 16, 2
#define SCL_PINMUX 0, 17, 2

#elif defined(ARDUINO_TRINKET_M0)
/* TODO: NOT tested. SHOULD work with trinket silkscreen pins 0 and 2 for sda and scl */
#define SERCOM SERCOM2
#define SERCOM_GCLK_ID_CORE SERCOM2_GCLK_ID_CORE
#define SERCOM_IRQn SERCOM2_IRQn
#define SERCOM_HANDLER SERCOM2_Handler
#define SDA_PINMUX 0, 8, 3
#define SCL_PINMUX 0, 9, 3

#elif defined(ADAFRUIT_FEATHER_M4_EXPRESS)
/* tested */
#define SERCOM SERCOM2
#define SERCOM_GCLK_ID_CORE SERCOM2_GCLK_ID_CORE
#define SERCOM_IRQn SERCOM2_0_IRQn
#define SERCOM_HANDLER SERCOM2_0_Handler
#define SDA_PINMUX 0, 12, 2
#define SCL_PINMUX 0, 13, 2

#endif

static void pinmux(const unsigned port, const unsigned pin, const unsigned func) {
    PORT->Group[port].PMUX[pin >> 1].reg = pin % 2 ?
    ((PORT->Group[port].PMUX[pin >> 1].reg) & PORT_PMUX_PMUXE(0xF)) | PORT_PMUX_PMUXO(func) :
    ((PORT->Group[port].PMUX[pin >> 1].reg) & PORT_PMUX_PMUXO(0xF)) | PORT_PMUX_PMUXE(func);
    PORT->Group[port].PINCFG[pin].reg |= PORT_PINCFG_PMUXEN | PORT_PINCFG_DRVSTR;
}

static void init(void) {
    /* one time init */
    static char initted = 0;
    if (initted) return;
    initted = 1;

    /* set up pinmux using a board-specific macro for all arguments */
    pinmux(SDA_PINMUX);
    pinmux(SCL_PINMUX);

    /* set up interrupt */
    NVIC_ClearPendingIRQ(SERCOM_IRQn);
    NVIC_SetPriority(SERCOM_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_EnableIRQ(SERCOM_IRQn);

#ifdef __SAMD51__
    GCLK->PCHCTRL[SERCOM_GCLK_ID_CORE].bit.CHEN = 0;
    while (GCLK->PCHCTRL[SERCOM_GCLK_ID_CORE].bit.CHEN);

    GCLK->PCHCTRL[SERCOM_GCLK_ID_CORE].reg = GCLK_PCHCTRL_GEN_GCLK1_Val | (1 << GCLK_PCHCTRL_CHEN_Pos);
    while (!GCLK->PCHCTRL[SERCOM_GCLK_ID_CORE].bit.CHEN);
#else
    /* set up clock */
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
    while (GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY);
#endif

    /* reset the sercom */
    SERCOM->I2CM.CTRLA.bit.SWRST = 1;
    while (SERCOM->I2CM.CTRLA.bit.SWRST || SERCOM->I2CM.SYNCBUSY.bit.SWRST);

    /* master */
    /* TODO: figure out what else is needed for i2c in standby on samd21, works fine on samd51 */
    SERCOM->I2CM.CTRLA.reg = SERCOM_I2CM_CTRLA_MODE(0x5u) | SERCOM_I2CM_CTRLA_RUNSTDBY;

    /* assume system clock is 48 MHz */
    const unsigned long sysclock = 48000000UL, baudrate = 100000UL;

    /* samd21 cargo cult rise time stuff */
    SERCOM->I2CM.BAUD.bit.BAUD = sysclock / (2 * baudrate) - 5 - (((sysclock / 1000000) * 125) / (2 * 1000));

    /* enable the sercom */
    SERCOM->I2CM.CTRLA.bit.ENABLE = 1;
    while (SERCOM->I2CM.SYNCBUSY.bit.ENABLE);

    /* set the bus state to idle */
    SERCOM->I2CM.STATUS.bit.BUSSTATE = 1;
    while (SERCOM->I2CM.SYNCBUSY.bit.SYSOP);
}

static const uint8_t * data_being_sent = NULL, * data_stop = NULL;

int i2c_is_still_writing(void) {
    return !!(*((const uint8_t * volatile *)&data_being_sent));
}

void SERCOM_HANDLER(void) {
    /* if we got here for some reason other than what we expect, do nothing */
    if (!SERCOM->I2CM.INTFLAG.bit.MB) return;

    /* if the last byte we sent was the final byte...*/
    if (data_being_sent == data_stop) {
        /* disable interrupt */
        SERCOM->I2CM.INTENCLR.reg = SERCOM_I2CM_INTENCLR_MB;

        /* send stop command and wait for sync */
        SERCOM->I2CM.CTRLB.reg |= SERCOM_I2CM_CTRLB_CMD(0x3);
        while (SERCOM->I2CM.SYNCBUSY.bit.SYSOP);

        /* inform the main thread, if blocked, that we are done sending */
        data_being_sent = NULL;
    }
    /* otherwise just send the next byte */
    else SERCOM->I2CM.DATA.bit.DATA = *(data_being_sent++);

    /* clear the interrupt flag. yes, you should raise an eyebrow about this, and gcc does too */
    SERCOM->I2CM.INTFLAG.bit.MB = 1;
    while (SERCOM->I2CM.INTFLAG.bit.MB);
}

void i2c_write(uint8_t address, const void * data, size_t size) {
    /* invoke possible one-time init */
    init();

    /* possibly wait until last invocation is done sending */
    while (i2c_is_still_writing()) __WFI();

    /* wait for bus state to be idle or owner */
    while (!(1 == SERCOM->I2CM.STATUS.bit.BUSSTATE) && !(2 == SERCOM->I2CM.STATUS.bit.BUSSTATE));

    /* enqueue the data */
    data_being_sent = data;
    data_stop = data_being_sent + size;

    /* enable can-send-next-byte interrupt */
    SERCOM->I2CM.INTENSET.reg = SERCOM_I2CM_INTENSET_MB;

    /* set address, which kicks off the interrupt handler */
    SERCOM->I2CM.ADDR.bit.ADDR = address << 1;
}
