/* use arduino-provided micros() for this demo, or #define it to time_us_32() from the pico sdk, or
 implement it some other how. if used as-is, this is the only part that relies on arduino */
extern unsigned long micros(void);

#include "ssd1306.h"
#include <stdio.h>

static const unsigned long interval = 500000;
static unsigned long prev = 0;

void setup(void) {
    oled_clear();
    oled_write_text("hello\n");
    oled_refresh();

    prev = micros();
}

static unsigned long elapsed_prev = 0;
static unsigned long frame = 0;

void loop(void) {
    const unsigned long now = micros();
    if (now - prev < interval) {
        asm volatile("wfi");
        return;
    }
    prev += interval;

    /* due to the above logic, we get here once every interval microseconds on average */

    /* wait for previous draw to finish, before we start messing with the buffer */
    while (oled_is_still_writing()) asm volatile("wfi");
    //    oled_clear();

    /* draw some text into the buffer, without having cleared it */
    char buf[64];
    sprintf(buf, "frame %lu: %luus\n", frame++, elapsed_prev);
    oled_write_text(buf);

    oled_refresh();

    elapsed_prev = micros() - now;
}
