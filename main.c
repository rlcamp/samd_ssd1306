#include "ssd1306.h"
#include <stdio.h>

void setup(void) {
    oled_clear();
    oled_write_text("hello\n");
    oled_refresh();

}

void loop(void) {
    /* wait for previous draw to finish, before we start messing with the buffer */
    while (oled_is_still_writing()) {
        asm volatile("wfi");
        return;
    }

    /* draw some text into the buffer, without having cleared it */
    char buf[64];
    static unsigned long frame = 0;
    sprintf(buf, "frame %lu\n", frame++);
    oled_write_text(buf);

    oled_refresh();
}
