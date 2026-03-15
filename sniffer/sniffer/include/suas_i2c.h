#ifndef __SUAS_I2C_H
#define __SUAS_I2C_H

#include <bl602_i2c.h>
#include <hal_i2c.h>

#define SUAS_I2C_LOW_SPEED 4000        /* 4 kHz, nonstandard low speed */
#define SUAS_I2C_STANDARD_SPEED 100000 /* 100 kHz */
#define SUAS_I2C_FULL_SPEED 400000     /* 400 kHz */

// Macros to ease function access
#define suas_i2c_write_byte(device, data, address) \
  hal_i2c_write_block(device, data, 1, 1, address)

#define suas_i2c_read_byte(device, data, address) \
  hal_i2c_read_block(device, data, 1, 1, address)

void suas_i2c_init();

#endif