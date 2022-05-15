#include "print.h"


#define MAX_STRING_SIZE 1024
int LINE_SIZE, NUM_OF_LINES;

static unsigned char _curLine = 0;
unsigned char _isInit = 0;
static GLIB_Context_t glibContext;


/**
 * call this function before printing to the screen, and provide font size. (default = small)
 */
void lcd_init(FontSize size) {
    if (_isInit) return;
    _isInit = 1;
    uint32_t status;
    /* Enable the memory lcd */
    status = sl_board_enable_display();
    EFM_ASSERT(status == SL_STATUS_OK);
    /* Initialize the DMD support for memory lcd display */
    status = DMD_init(0);
    EFM_ASSERT(status == DMD_OK);
    /* Initialize the glib context */
    status = GLIB_contextInit(&glibContext);
    EFM_ASSERT(status == GLIB_OK);
    glibContext.backgroundColor = White;
    glibContext.foregroundColor = Black;
    /* Fill lcd with background color */
    GLIB_clear(&glibContext);
    /* Use Narrow font */
    switch (size) {
      case big:
        GLIB_setFont(&glibContext, (GLIB_Font_t*) &GLIB_FontNumber16x20);
        break;
      case medume:
        GLIB_setFont(&glibContext, (GLIB_Font_t*) &GLIB_FontNormal8x8);
        break;
      default:
        GLIB_setFont(&glibContext, (GLIB_Font_t*) &GLIB_FontNarrow6x8);
    }
    NUM_OF_LINES = glibContext.clippingRegion.xMax /
        (glibContext.font.fontHeight + glibContext.font.lineSpacing);
    LINE_SIZE = (glibContext.clippingRegion.yMax /
                    glibContext.font.fontWidth) - 1;
    if (size == big) LINE_SIZE++;
}


/**
 * Function that erase the screen.
 */
void cleanScreen() {
    _curLine = 0;
    GLIB_clear(&glibContext);
}


/**
 * function that receives a string and a character and removes all rm occurrences in the given string
 * @NOTE: this function is editing the given string meaning it should be allowed to access
 *        the string chars and change them.
 * @param string: string to update.
 * @param rm: char to remove.
 * @return: the length of the edited string.
 */
int remove_char(char* string, char rm) {
    int size = strlen(string);
    for (int i=0, j=0; i < size; i++, j++) {
        while (string[j] == rm) {
            j++;
            size--;
        }
        string[i] = string[j];
    }
    string[size] = '\0';
    return size;
}


/**
 * Implementation of std puts for the EFM32PG12B LCD screen.
 * this function do not brake the line in '\n'
 */
void lcd_puts(const char* string) {
    uint32_t status;
    status = GLIB_drawStringOnLine(&glibContext, string, _curLine++,
                                   GLIB_ALIGN_LEFT, 5, 5, true);
    EFM_ASSERT(status == GLIB_OK);
}


/**
 * Implementation of std printf for the EFM32PG12B LCD screen.
 *    MAX_STRING_SIZE = 1024
 */
void lcd_printf(const char *format, ...) {
    char output_buf[MAX_STRING_SIZE] = {0};
    va_list args;
    va_start(args, format);
    int size = vsnprintf(output_buf, MAX_STRING_SIZE, format, args);
    va_end(args);
    if (size <= 0) return;
    remove_char(output_buf, '\r');
    char *line = strtok(output_buf,"\n");
    char *last_space = line;
    char line_buf[LINE_SIZE + 1];
    int space_idx = 0;
    while (line) {
        int line_size = strlen(line);
        if (_curLine >= NUM_OF_LINES) cleanScreen();
        if (line_size > LINE_SIZE) {
            for (int i = 0; i < line_size;) {
                if (_curLine >= NUM_OF_LINES) cleanScreen();
                bzero(line_buf, LINE_SIZE + 1);
                for (int j = 1; j <= min(LINE_SIZE, line_size - i); j++) {
                    if (line[i + j - 1] == ' ' || line[i + j] == '\0') {
                        last_space = line + i + j;
                    }
                }
                space_idx = last_space - (line + i);
                strncpy(line_buf, line + i, (space_idx > 0) ? space_idx:LINE_SIZE);
                lcd_puts(line_buf);
                i += (space_idx > 0) ? space_idx:LINE_SIZE;
            }
        }
        else {
            lcd_puts(line);
        }
        line = strtok(NULL,"\n");
    }
    DMD_updateDisplay();
}
