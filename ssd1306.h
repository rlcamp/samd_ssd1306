#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* draws the buffer to the screen, nonblocking */
void screen_refresh(void);

/* swap buffers and erase the new buffer  */
void screen_clear(void);

/* loop on this if you need to wait for the previous write to finish */
int screen_is_still_writing(void);

/* draw characters into the buffer, scrolling the screen if necessary */
void screen_write_text(const char * string);

/* turn a single pixel on or off */
void screen_set_pixel(const size_t x, const size_t y, const char on);

#ifdef __cplusplus
}
#endif
