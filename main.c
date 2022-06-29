#include "ssd1306.h"
#include <stdio.h>

void setup(void) {
    screen_clear();
    screen_write_text("hello\n");
    screen_refresh();

}

void loop(void) {
    /* wait for previous draw to finish, before we start messing with the buffer */
    while (screen_is_still_writing()) {
        asm volatile("wfi");
        return;
    }

    /* draw some text into the buffer, without having cleared it */
    char buf[64];
    static unsigned long frame = 0;
    sprintf(buf, "frame %lu\n", frame++);
    screen_write_text(buf);

    screen_refresh();
}
