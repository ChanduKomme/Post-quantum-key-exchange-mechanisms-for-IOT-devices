/* FreeRTOS includes */
#include <FreeRTOS.h>
#include <task.h>

/* Standard library requirement for memcpy */
#include <string.h>

/* Our own headers for the HAL */
#include <hal_i2c.h>
#include <suas_i2c.h>
#include <suas_ssd1306.h>
#include <suas_ssd1306_font.h>

/* Define our own command to send the data */
#define suas_ssd1306_write(data, len) \
  hal_i2c_write_block(SSD1306_DEVICE_ADDRESS, data, len, 0, 0)

/* Set cursor to a specific page and column */
void suas_ssd1306_set_cursor(uint8_t page, uint8_t column) {
  const char set_cursor_commands[] = {
      0x00,                      // Enable command mode
      0xB0 + page,               // Set page start address
      0x00 + (column & 0x0F),    // Set lower column
      0x10 + ((column >> 4) & 0x0F)  // Set higher column
  };

  // Send data
  suas_ssd1306_write(set_cursor_commands, sizeof(set_cursor_commands) /
                                              sizeof(set_cursor_commands[0]));
}

/* Clears the display by turning all bits off */
void suas_ssd1306_clear() {
  for (uint8_t page = 0; page < 8; page++) {
    // Set current page
    suas_ssd1306_set_cursor(page, 0);

    // Send 128 zeroes to turn off all pixels
    char empty_data[9] = {0};  // Mode + 8 zeroes
    empty_data[0] = 0x40;      // Set data mode

    // Send the data 16 times so that we sent 8 * 16 = 128 zeroes
    for (uint8_t i = 0; i < 16; i++) {
      suas_ssd1306_write(empty_data,
                         sizeof(empty_data) / sizeof(empty_data[0]));
    }
  }
}

/* Print a specific character on the screen */
void suas_ssd1306_print_char(char c) {
  // Extern font array
  extern const char font[256][5];

  // Select required character
  const char *bitmap = font[(uint8_t)c];

  // Create character buffer
  char data[6] = {0};
  data[0] = 0x40;  // Data mode
  memcpy(data + 1, bitmap,
         5);  // Copy character information into data[1] to data[5]

  // Send text
  suas_ssd1306_write(data, sizeof(data) / sizeof(data[0]));
}

/* Print text on the screen */
void suas_ssd1306_print_text(char *text) {
  // Just print each character individually
  for (uint8_t i = 0; i < strlen(text); i++) {
    suas_ssd1306_print_char(text[i]);
  }
}

/* Initialize the display */
void suas_ssd1306_init() {
  suas_i2c_init();

  // Wait until all hardware is ready
  vTaskDelay(100);

  // Initialization commands can be found in manual, p. 64.
  // Furthermore we set the memory addressing mode and deactivate scroll
  const char initialization_commands[] = {
      0x00,  // Enable command mode
      0xAE,  // Turn display off
      0xA8,  // Set multiplex ratio 1/2
      0x3F,  // Set multiplex ratio 2/2
      0xD3,  // Set display offset 1/2
      0x00,  // Set display offset 2/2
      0x40,  // Set display start line
      0xA1,  // Set segment remap
      0xC8,  // Set com output scan direction
      0xDA,  // Set com pins 1/2
      0x12,  // Set com pins 2/2
      0x81,  // Set contrast control 1/2
      0x7F,  // Set contrast control 2/2
      0xA4,  // Disable entire display on
      0xA6,  // Set normal display on (1 = turned on pixel, 0 = turned off
             // pixel)
      0xD5,  // Set oscillator frequency 1/2
      0x80,  // Set oscillator frequency 2/2
      0x8D,  // Enable charge pump regulator 1/2
      0x14,  // Enable charge pump regulator 2/2
      0x20,  // Set memory addressing mode 1/2
      0x00,  // Set memory addressing mode to horizontal 2/2
      0x2E,  // Deactivate scroll
      0xAF   // Turn display on
  };

  // Write data
  suas_ssd1306_write(
      initialization_commands,
      sizeof(initialization_commands) / sizeof(initialization_commands[0]));

  // Clear screen
  suas_ssd1306_clear();
}
