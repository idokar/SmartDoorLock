#include "em_usart.h"
#include "serial_io.h"
#include "timer.h"

#define CIRCULAR_BUF_SIZE 256


static USART_TypeDef* uart;

volatile struct circular_buf {
    uint8_t data[CIRCULAR_BUF_SIZE];  /* data buffer */
    uint32_t rdI;  /* read index */
    uint32_t wrI;  /* write index */
    uint32_t pendingBytes;  /* count of how many bytes are not yet handled */
    bool overflow;  /* buffer overflow indicator */
} rxBuf = {0}, txBuf = {0};


/**
 * function that sends data using circular buf.
 * @param data: pointer to string.
 * @param data_len: lenth of the string.
 */
void UartPutData(const uint8_t * data, uint32_t data_len) {
    if (data_len > CIRCULAR_BUF_SIZE) {
        return;
    }
    if ((txBuf.pendingBytes + data_len) > CIRCULAR_BUF_SIZE) {
        while ((txBuf.pendingBytes + data_len) > CIRCULAR_BUF_SIZE) {}
    }
    uint32_t i = 0;
    while (i < data_len) {
        txBuf.data[txBuf.wrI] = *(data + i);
        txBuf.wrI = (txBuf.wrI + 1) & (CIRCULAR_BUF_SIZE - 1);
        txBuf.pendingBytes++;
        i++;
    }
    USART_IntEnable(uart, USART_IF_TXBL);
}


/**
 * @param data: the buffer that receives the input.
 * @param data_len: maximum bytes to read into dataPtr .
 * @param timeout_ms: read operation timeout milliseconds.
 * @return: amount of bytes read into buf, -1 on error.
 */
uint32_t UartGetData(uint8_t * data, uint32_t data_len, unsigned int timeout_ms) {
    unsigned int wait_start = cur_time();
    if (rxBuf.pendingBytes < 1) {
        while (rxBuf.pendingBytes < 1) {
            if (cur_time() - wait_start >= timeout_ms) {
                return -1;
            }
        }
    }
    if (data_len > rxBuf.pendingBytes) {
        data_len = rxBuf.pendingBytes;
    }
    uint32_t i = 0;
    while (i < data_len) {
        *(data + i) = rxBuf.data[rxBuf.rdI];
        rxBuf.rdI = (rxBuf.rdI + 1) & (CIRCULAR_BUF_SIZE -1);
        rxBuf.pendingBytes--;
        i++;
    }
    return i;
}


/**
 * @brief Initialises the serial connection.
 * @param port: the port to connected to. e.g: /dev/ttyUSB0, /dev/ttyS1 for Linux and COM8, COM10, COM53 for Windows.
 * @param baud: the baud rate of the communication. For example: 9600, 115200
 * @return 0 if succeeded in opening the port and -1 otherwise.
 */
int SerialInit(char* port, unsigned int baud) {
    (void) port;
    uart = USART0;
    our_timer_init();
    CMU_ClockEnable(cmuClock_HFPER, true);
    CMU_ClockEnable(cmuClock_USART0, true);
    CMU_ClockEnable(cmuClock_GPIO, true);

    /* To avoid false start, configure TX pin as initial high */
    GPIO_PinModeSet((GPIO_Port_TypeDef)AF_USART0_TX_PORT(_USART_ROUTELOC0_TXLOC_LOC0),
                    AF_USART0_TX_PIN(_USART_ROUTELOC0_TXLOC_LOC0), gpioModePushPull, 1);
    GPIO_PinModeSet((GPIO_Port_TypeDef)AF_USART0_RX_PORT(_USART_ROUTELOC0_RXLOC_LOC0),
                    AF_USART0_RX_PIN(_USART_ROUTELOC0_RXLOC_LOC0), gpioModeInput, 0);

    USART_InitAsync_TypeDef uart_init = USART_INITASYNC_DEFAULT;

    /* Prepare struct for initializing UART in asynchronous mode*/
    uart_init.enable       = usartDisable;   /* Don't enable UART upon intialization */
    uart_init.refFreq      = 0;              /* Provide information on reference frequency. When set to 0, the reference frequency is */
    uart_init.baudrate     = baud;           /* Baud rate */
    uart_init.oversampling = usartOVS16;     /* Oversampling. Range is 4x, 6x, 8x or 16x */
    uart_init.databits     = usartDatabits8; /* Number of data bits. Range is 4 to 10 */
    uart_init.parity       = usartNoParity;  /* Parity mode */
    uart_init.stopbits     = usartStopbits1; /* Number of stop bits. Range is 0 to 2 */
    uart_init.mvdis        = false;          /* Disable majority voting */
    uart_init.prsRxEnable  = false;          /* Enable USART Rx via Peripheral Reflex System */
    uart_init.prsRxCh      = usartPrsRxCh0;  /* Select PRS channel if enabled */
    USART_InitAsync(uart, &uart_init);

    uart->ROUTEPEN |= USART_ROUTEPEN_RXPEN | USART_ROUTEPEN_TXPEN;
    uart->ROUTELOC0 = (USART0->ROUTELOC0 & ~(_USART_ROUTELOC0_TXLOC_MASK | _USART_ROUTELOC0_RXLOC_MASK)) |
                      (_USART_ROUTELOC0_TXLOC_LOC0 << _USART_ROUTELOC0_TXLOC_SHIFT) |
                      (_USART_ROUTELOC0_RXLOC_LOC0 << _USART_ROUTELOC0_RXLOC_SHIFT);

    /* Prepare UART Rx and Tx interrupts */
    USART_IntClear(uart, _USART_IF_MASK);
    USART_IntEnable(uart, USART_IF_RXDATAV);
    NVIC_ClearPendingIRQ(USART0_RX_IRQn);
    NVIC_ClearPendingIRQ(USART0_TX_IRQn);
    NVIC_EnableIRQ(USART0_RX_IRQn);
    NVIC_EnableIRQ(USART0_TX_IRQn);

    /* Enable UART */
    USART_Enable(uart, usartEnable);
    return 0;
}


/**
 * @brief Receives data from serial connection.
 * @param buf: the buffer that receives the input.
 * @param max_len: maximum bytes to read into buf (buf must be equal or greater than max_len).
 * @param timeout_ms: read operation timeout milliseconds.
 * @return amount of bytes read into buf, -1 on error.
*/
int SerialRecv(unsigned char *buf, unsigned int max_len, unsigned int timeout_ms) {
    uint32_t start = cur_time();
    uint32_t read_size = 0;
    unsigned int total_read = 0;
    int rc;
    while(total_read < max_len) {
        uint32_t interval = cur_time() - start;
        if (interval >= timeout_ms) {
            return total_read;
        }
        read_size = (max_len - total_read > CIRCULAR_BUF_SIZE) ? CIRCULAR_BUF_SIZE:(max_len - total_read);
        rc = UartGetData(buf + total_read, read_size, timeout_ms - interval);
        if (rc < 0) {
            return total_read;
        }
        total_read += rc;
        if (!rc) {
            return total_read;
        }
    }
    return total_read;
}


/**
 * @brief Sends data through the serial connection.
 * @param buf: the buffer that contains the data to send
 * @param size: number of bytes to send
 * @return amount of bytes written into buf, -1 on error
 */
int SerialSend(char *buf, unsigned int size) {
    uint32_t read_size = 0;
    unsigned int remain_size = size;
    while(remain_size > 0) {
        read_size = remain_size;
        if (read_size > CIRCULAR_BUF_SIZE) {
            read_size = CIRCULAR_BUF_SIZE;
        }
        remain_size -= read_size;
        UartPutData(buf, read_size);
        buf += read_size;
    }
    while(txBuf.pendingBytes);
    return (int) size;
}


/**
 * Empties the input buffer and resets the writing and reading location.
 */
void SerialFlushInputBuff(void) {
    rxBuf.rdI = rxBuf.wrI;
    rxBuf.pendingBytes = 0;
    rxBuf.overflow = false;
}


/**
 * Disable the serial connection of the uart.
 * return: 0 if succeeded in closing the port and -1 otherwise.
 */
int SerialDisable(void) {
    USART_IntDisable(uart, USART_IF_RXDATAV);
    USART_IntDisable(uart, USART_IF_TXBL);
    USART_Enable(uart, usartDisable);
    CMU_ClockEnable(cmuClock_USART0, false);
    return 0;
}


/**
 * UART2 RX IRQ Handler
 */
void USART0_RX_IRQHandler(void) {
    if (uart->STATUS & USART_STATUS_RXDATAV) {
        uint8_t rxData = USART_Rx(uart);
        rxBuf.data[rxBuf.wrI] = rxData;
        rxBuf.wrI = (rxBuf.wrI + 1) & (CIRCULAR_BUF_SIZE - 1);  // Efficient modulo for power of 2 numbers
        rxBuf.pendingBytes++;
        if (rxBuf.pendingBytes > CIRCULAR_BUF_SIZE) {
            rxBuf.overflow = true;
        }
        USART_IntClear(USART0, USART_IF_RXDATAV);
    }
}


/**
 * UART2 TX IRQ Handler
 */
void USART0_TX_IRQHandler(void) {
    USART_IntGet(USART0);
    if (uart->STATUS & USART_STATUS_TXBL) {
        if (txBuf.pendingBytes > 0) {
            USART_Tx(uart, txBuf.data[txBuf.rdI]);
            txBuf.rdI = (txBuf.rdI + 1) & (CIRCULAR_BUF_SIZE - 1);  // Efficient modulo for power of 2 numbers
            txBuf.pendingBytes--;
        }
        if (txBuf.pendingBytes == 0) {
            USART_IntDisable(uart, USART_IF_TXBL);
        }
    }
}
