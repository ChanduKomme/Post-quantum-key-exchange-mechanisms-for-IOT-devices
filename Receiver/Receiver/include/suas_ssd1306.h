#ifndef __SUAS_SSD1306_H
#define __SUAS_SSD1306_H

// Set I2C address of SSD1306
#ifndef SSD1306_DEVICE_ADDRESS
#define SSD1306_DEVICE_ADDRESS 0x3C
#endif

void suas_ssd1306_set_cursor(uint8_t page, uint8_t column);
void suas_ssd1306_clear();
void suas_ssd1306_print_char(char c);
void suas_ssd1306_print_text(char *text);
void suas_ssd1306_init();
#endif