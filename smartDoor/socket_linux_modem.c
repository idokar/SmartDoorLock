#include "socket.h"
#include <string.h>
#include <stdio.h>


static OPERATOR_INFO oplist[25] = {0};
static int opsFound = 0;
static char opsBuf[2048];


/**
* Initializes the socket.
* Host: The destination address
* as DNS: en8wtnrvtnkt5.x.pipedream.net,
* or as IPv4: 35.169.0.97.
* Port: The communication endpoint, int, e.g.: 80.
* Returns 0 on success, -1 on failure
*/
int SocketInit(char *host, int port) {
    if (host == NULL || port < 0 || port > 65535) {
        PRINT_DEBUG("Socket Linux Modem: input is not valid")
    }
    bzero(oplist,25 * sizeof(OPERATOR_INFO));
    opsFound = 0;
    if (CellularInit(NULL) == -1) {
        return -1;
    }
    if(CellularWaitUntilModemResponds() == -1) {
        PRINT_DEBUG("Socket Linux Modem: failed to send AT commands");
        return -1;
    }
    if(CellularGetOperators(oplist, 25, &opsFound) == -1) {
        PRINT_DEBUG("Socket Linux Modem: can not get operators");
        return -1;
    }
    for (int i = 0; i < opsFound; i++) {
        if (!CellularSetOperator(SET_OPT_MODE_MANUAL, oplist[i].operator_code)) {
            if (!CellularWaitUntilRegistered()) {
                PRINTF_DEBUG("Socket Linux Modem: connected to operator '%s' successfully\n", oplist[i].operator_name)
                break;
            }
        }
    }
    if (CellularSetupInternetConnectionProfile(20) == -1) {
        PRINT_DEBUG("Socket Linux Modem: setup internet connection profile failed");
        return -1;
    }
    if (CellularSetupInternetServiceProfile(host, port, 80) == -1) {
        PRINT_DEBUG("Socket Linux Modem: setup internet service profile failed");
        return -1;
    }
    return 0;
}


/**
* Connects to the socket
* (establishes TCP connection to the pre-defined host and port).
* Returns 0 on success, -1 on failure
*/
int SocketConnect(void) {
    if(CellularConnect() == -1) {
        PRINT_DEBUG("Socket Linux Modem: could not connect")
        return -1;
    }
    return 0;
}


/**
* Writes len bytes from the payload buffer
* to the established connection.
* Returns the number of bytes written on success, -1 on failure
*/
int SocketWrite(unsigned char *payload, unsigned int len) {
    if (payload == NULL) {
        PRINT_DEBUG("Socket Linux Modem: invalid input")
        return -1;
    }
    int rc = CellularWrite(payload, len);
    if (rc == -1) {
        PRINT_DEBUG("Socket Linux Modem: could not write")
        return -1;
    }
    return rc;
}


/**
* Reads up to max_len bytes from the established connection
* to the provided buf buffer,
* for up to timeout_ms (doesn’t block longer than that,
* even if not all max_len bytes were received).
* Returns the number of bytes read on success, -1 on failure
*/
int SocketRead(unsigned char *buf, unsigned int max_len, unsigned int timeout_ms) {
    if (buf == NULL) {
        PRINT_DEBUG("Socket Linux Modem: invalid input")
        return -1;
    }
    int rc = CellularRead(buf, max_len, timeout_ms);
    if (rc == -1) {
        PRINT_DEBUG("Socket Linux Modem: could not read")
        return -1;
    }
    return rc;
}


/**
* Closes the established connection.
* Returns 0 on success, -1 on failure
*/
int SocketClose(void) {
    if(CellularClose() == -1) {
        PRINT_DEBUG("Socket Linux Modem: could not close connection")
        return -1;
    }
    return 0;
}


/**
* Frees any resources that were allocated by SocketInit
*/
void SocketDeInit(void) {
    CellularClose();
    CellularDisable();
}
