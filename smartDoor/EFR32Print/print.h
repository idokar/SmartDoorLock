#ifndef PRINT_H_
#define PRINT_H_

#include <stdio.h>
#include <stdarg.h>
#include <strings.h>
#include <string.h>
#include "em_assert.h"
#include "glib.h"
#include "dmd.h"
#include "sl_board_control.h"

#define min(x, y) (((x) < (y)) ? (x):(y))


typedef enum FontSize {
  small,
  medume,
  big /* Numbers only */
} FontSize;

/**
 * call this function before printing to the screen, and provide font size. (default = small)
 */
void lcd_init(FontSize);

/**
 * Function that erase the screen.
 */
void cleanScreen();

/**
 * function that receives a string and a character and removes all rm occurrences in the given string
 * @NOTE: this function is editing the given string meaning it should be allowed to access
 *        the string chars and change them.
 * @param string: string to update.
 * @param rm: char to remove.
 * @return: the length of the edited string.
 */
int remove_char(char* string, char rm);

/**
 * Implementation of std puts for the EFM32PG12B LCD screen.
 * this function do not brake the line in '\n'
 */
void lcd_puts(const char*);

/**
 * Implementation of std printf for the EFM32PG12B LCD screen.
 *    MAX_STRING_SIZE = 1024
 */
void lcd_printf(const char *, ...);

#endif /* PRINT_H_ */
