// I2C hardware abstraction layer and related headers for real-time operations
#include <aos/kernel.h>
#include <aos/yloop.h>
#include <hal_i2c.h>
#include <loopset_i2c.h>

// Standard library includes
#include <stdio.h>

// Own HAL
#include <suas_i2c.h>

// Callback function to handle I2C interrupts
static void suas_event_cb_i2c_interrupt(input_event_t *event,
                                        [[gnu::unused]] void *private_data) {
  switch (event->code) {
    case CODE_I2C_END:
      printf("[I2C][EVT] Transfer ended interrupt %lld\r\n", aos_now_ms());
      break;
    case CODE_I2C_ARB:
      printf("[I2C][EVT] Arbitration interrupt %lld\r\n", aos_now_ms());
      break;
    case CODE_I2C_NAK:
      printf("[I2C][EVT] Transfer not acknowledged interrupt  %lld\r\n",
             aos_now_ms());
      break;
    case CODE_I2C_FER:
      printf("[I2C][EVT] Transfer FIFO overflow/underflow interrupt %lld\r\n",
             aos_now_ms());
      break;
    default:
      printf("[I2C][EVT] Unknown error code %u, %lld\r\n", event->code,
             aos_now_ms());
  }
}

// I2C Initialization function
void suas_i2c_init() {
  // Add I2C hook to real-time looping
  loopset_i2c_hook_on_looprt();

  // Register I2C interrupt (event) filter; callback function
  aos_register_event_filter(EV_I2C, suas_event_cb_i2c_interrupt, NULL);

  // Register I2C channel: Channel 0 (pin 3: SDA, pin 4: SCL)
  hal_i2c_init(/* Channel */ 0, /*  Period (adjusted when setting clock */ 15);

  // Set I2C clock
  I2C_ClockSet(/* Channel */ 0, /* Speed */ SUAS_I2C_STANDARD_SPEED);
}