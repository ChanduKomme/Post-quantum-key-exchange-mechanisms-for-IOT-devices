#include <bl602_i2c.h>
#include <hal_i2c.h>
#include "suas_i2c.h"

/* Provided by the SDK loopset component. */
extern int loopset_i2c_hook_on_looprt(void);

void suas_i2c_init(void)
{
    /* Hook I2C into the BL602 loop runtime before enabling the peripheral. */
    loopset_i2c_hook_on_looprt();

    /* Same pins and clock used by the original suas_drivers component:
       - hal_i2c_init(0, 15)
       - I2C_ClockSet(0, 100000)
     */
    hal_i2c_init(0, 15);
    I2C_ClockSet(0, SUAS_I2C_STANDARD_SPEED);
}
