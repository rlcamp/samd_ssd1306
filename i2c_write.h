#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* non-blocking write of the given contiguous block of data to the given address */
void i2c_write(const uint8_t address, const void *, size_t size);

/* loop on this if you need to wait until the previous write finishes */
int i2c_is_still_writing(void);

#ifdef __cplusplus
}
#endif
