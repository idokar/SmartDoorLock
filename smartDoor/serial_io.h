#ifndef SERIAL_IO_H
#define SERIAL_IO_H

//#define DEBUG
#ifdef DEBUG
#define PRINT_DEBUG(MSG) lcd_printf("%s\n", MSG);
#define PRINTF_DEBUG(FORMAT, ...) lcd_printf(FORMAT, ##__VA_ARGS__);
#else
#define PRINT_DEBUG(MSG)
#define PRINTF_DEBUG(FORMAT, ...)
#endif


/**
 * @brief initialize the serial connection.
 * @param port: the port to connected to. e.g: /dev/ttyUSB0, /dev/ttyS1 for Linux and COM8, COM10, COM53 for Windows.
 * @param baud: the baud rate of the communication. For example: 9600, 115200
 * @return 0 if succeeded in opening the port and -1 otherwise.
 */
int SerialInit(char* port, unsigned int baud);

/**
 * @brief Receives data from serial connection.
 * @param buf: the buffer that receives the input.
 * @param max_len: maximum bytes to read into buf (buf must be equal or greater than max_len).
 * @param timeout_ms: read operation timeout milliseconds.
 * @return amount of bytes read into buf, -1 on error.
*/
int SerialRecv(unsigned char *buf, unsigned int max_len, unsigned int timeout_ms);

/**
 * @brief Sends data through the serial connection.
 * @param buf: the buffer that contains the data to send
 * @param size: number of bytes to send
 * @return amount of bytes written into buf, -1 on error
 */
int SerialSend(char *buf, unsigned int size);

/**
 * Empties the input buffer and resets the writing and reading location.
 */
void SerialFlushInputBuff(void);

/**
 * Disable the serial connection of the uart.
 * return: 0 if succeeded in closing the port and -1 otherwise.
 */
int SerialDisable(void);

#endif // SERIAL_IO_H
