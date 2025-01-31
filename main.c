#include "ssd1306.h"
#include <stdio.h>

void setup(void) {
    /* blank the screen */
    screen_clear();

    /* draw into the buffer */
    screen_write_text("hello\n");

    /* initiate actual drawing of the buffer onto the screen */
    screen_refresh();
}

void loop(void) {
    /* wait for previous draw to finish, before we start messing with the buffer */
    while (screen_is_still_writing()) {
        /* note this would be an unsafe use of wfi in the absence of other regular
         sources of interrupts. wfe is safer, but this code runs on cortex m0+ */
        asm volatile("wfi");
        return;
    }

    /* draw some text into the buffer, without having cleared it */
    static unsigned long frame = 0;
    screen_printf("frame %lu\n", frame++);

    /* initiate a redraw of the screen */
    screen_refresh();
}
