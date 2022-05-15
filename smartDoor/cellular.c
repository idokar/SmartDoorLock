#include "cellular.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#define SEND_FAILUR "Cellular: failed to send command"
#define RECV_FAILUR "Cellular: failed to receive data"
#define RECV_WRONG_RESPONSE "Cellular: incorrect response"
#define STOP_TRANSPARENT "+++", 3
#define AT "AT\r\n", 4
#define ECHO_OFF "ATE0\r\n", 6
#define CSQ "AT+CSQ\r\n", 8
#define CCID "AT+CCID\r\n", 9
#define CGSN "AT+CGSN\r\n", 9
#define CREG "AT+CREG?\r\n", 10
#define SMONI "AT^SMONI\r\n", 10
#define COPS "AT+COPS=?\r\n", 11
#define SISO_COMM "AT^SISO=0\r\n", 11
#define SIST_COMM "AT^SIST=0\r\n", 11
#define SISC_COMM "AT^SISC=0\r\n", 11
#define SISS_CONNID "AT^SISS=0,\"conId\",\"0\"\r\n", 23
#define SICS_CONTYPE "AT^SICS=0,conType,\"GPRS0\"\r\n", 27
#define SICS_APN "AT^SICS=0,apn,\"postm2m.lu\"\r\n" , 28
#define SCFG "AT^SCFG=\"Tcp/WithURCs\",\"on\"\r\n", 29
#define SISS_SRVTYPE "AT^SISS=0,\"SrvType\",\"Socket\"\r\n", 30
#define FORAMT_SICS_INACT "AT^SICS=0,inactTO,\"%d\"\r\n"
#define SISS_SOCKTCP_FORMAT "AT^SISS=0,\"address\",\"socktcp://%s:%d;etx;timer=%d\"\r\n"
#define ROUNDS 7
#define SHORT_TIME 3000
#define MEDIUM_TIME 60000
#define LONG_TIME 120000
#define MIN(x, y) ((x) < (y)) ? (x):(y)


char transparentMode = 0;

typedef enum {
    all = 0,
    echo_and_scfg,
    scfg,
    wait_to_start,
    turn_echo_off,
    none_op
} Options;


/**
 * function to run init commands, e.g. set echo off (ATE0), check +PBREADY
 * @return 0 on success else -1
 */
int initialization_options(Options op) {
    char buf[55] = {0};
    switch (op) {
        case all:
            PRINT_DEBUG("Cellular: waiting for +PBREADY turn on the modem")
            if (SerialRecv(buf, 50, MEDIUM_TIME) == -1) {
                PRINT_DEBUG(RECV_FAILUR)
                return -1;
            }
            if(!strstr(buf, "+PBREADY")) {
                PRINT_DEBUG(RECV_WRONG_RESPONSE " +PBREADY")
                return -1;
            }
        case echo_and_scfg:
            bzero(buf, 55);
            SerialFlushInputBuff();
            if (SerialSend(ECHO_OFF) == -1) {
                PRINT_DEBUG(SEND_FAILUR ": ATE0")
                return -1;
            }
            if (SerialRecv(buf, 50, SHORT_TIME) == -1) {
                PRINT_DEBUG(RECV_FAILUR)
                return -1;
            }
            if(!strstr(buf, "OK")) {
                PRINT_DEBUG(RECV_WRONG_RESPONSE " from ATE0")
                return -1;
            }
        case scfg:
            bzero(buf,55);
            SerialFlushInputBuff();
            if(SerialSend(SCFG) == -1) {
                PRINT_DEBUG(SEND_FAILUR ": AT^SCFG")
                return -1;
            }
            if (SerialRecv(buf, 50, SHORT_TIME) == -1)  {
                PRINT_DEBUG(RECV_FAILUR)
                return -1;
            }
            if(!strstr(buf, "OK")) {
                PRINT_DEBUG(RECV_WRONG_RESPONSE " from AT^SCFG")
                return -1;
            }
            break;
        case wait_to_start:
            PRINT_DEBUG("Cellular: waiting for +PBREADY turn on the modem")
            if (SerialRecv(buf, 50, MEDIUM_TIME) == -1) {
                PRINT_DEBUG(RECV_FAILUR)
                return -1;
            }
            if(!strstr(buf, "+PBREADY")) {
                PRINT_DEBUG(RECV_WRONG_RESPONSE " +PBREADY")
                return -1;
            }
            break;
        case turn_echo_off:
            SerialFlushInputBuff();
            if (SerialSend(ECHO_OFF) == -1) {
                PRINT_DEBUG(SEND_FAILUR ": ATE0")
                return -1;
            }
            if (SerialRecv(buf, 50, SHORT_TIME) == -1) {
                PRINT_DEBUG(RECV_FAILUR)
                return -1;
            }
            if(!strstr(buf, "OK")) {
                PRINT_DEBUG(RECV_WRONG_RESPONSE " from ATE0")
                return -1;
            }
            break;
        default: break;
    }
    PRINT_DEBUG("Cellular: initialized")
    return 0;
}


/**
 * whatever is needed to start working with the cellular modem (e.g. the serial port).
 * Returns 0 on success, and -1 on failure
 */
int CellularInit(char *port) {
    PRINT_DEBUG("Cellular: initial connection")
    if(SerialInit(port, 115200) == -1) {
        PRINT_DEBUG("Cellular: init fails")
        return -1;
    }
    return initialization_options(echo_and_scfg);
}


/**
 * Deallocate / close whatever resources CellularInit() allocated
 */
void CellularDisable(void) {
    SerialDisable();
    PRINT_DEBUG("Cellular: disabled")
}


/**
 * Checks if the modem is responding to AT commands
 * Return 0 if it does, returns -1 otherwise
 */
int CellularCheckModem(void) {
    PRINT_DEBUG("Cellular: checking modem response")
    char buf[11] = {0};
    SerialFlushInputBuff();
    if(SerialSend(AT) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT")
        return -1;
    }
    int rc = SerialRecv(buf, 10, SHORT_TIME);
    if(rc == -1) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    if(strstr(buf,"OK")) {
        PRINT_DEBUG("Cellular: found the correct response")
        return 0;
    }

    PRINT_DEBUG("Cellular: received incorrect response")
    return -1;
}


/**
 * Checks if the modem is responding to AT commands
 * Return 0 if it does, returns -1 otherwise
 * Tries multiple times until succeeds or fails
 */
int CellularWaitUntilModemResponds(void) {
    PRINT_DEBUG("Cellular: wait for modem response")
    for (int i = 0; i < ROUNDS; i++) {
        if (!CellularCheckModem()) {
            PRINTF_DEBUG("Cellular: response after %d rounds\n", i + 1);
            return 0;
        }
    }
    PRINT_DEBUG("Cellular: cellular is not response")
    return -1;
}


/**
 * Returns -1 if the modem did not respond or respond with an error.
 * Returns 0 if the command was successful and the registration status was obtained from
 * the modem. In that case, the status parameter will be populated with the numeric value
 * of the <regStatus> field of the “+CREG” AT command
 */
int CellularGetRegistrationStatus(int *status) {
    PRINT_DEBUG("Cellular: get registration status")
    if (status == NULL) {
        PRINT_DEBUG("Cellular: function input is null")
        return -1;
    }
    SerialFlushInputBuff();
    if(SerialSend(CREG) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT+CREG?")
        return -1;
    }
    char buf[30] = {0};
    int rc = SerialRecv(buf, 25, SHORT_TIME);
    if (rc < 11) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    int count = 0;
    while(buf[count++] != ',' && count < 25){}
    *status = buf[count] - '0';
    if (*status < 0 || *status > 5) {
        PRINTF_DEBUG("Cellular: wrong status: %d\n", *status)
        return -1;
    }
    PRINTF_DEBUG("Cellular: got status: %d\n", *status)
    return 0;
}


/**
 * Returns -1 if the modem did not respond or respond with an error or couldn't register.
 * Returns 0 if the command was successful and the registration status was obtained from
 * the modem and the modem is registered (1 or 5).
 */
int CellularWaitUntilRegistered(void) {
    PRINT_DEBUG("Cellular: check registration")
    int status = 0;
    for (int i = 0; i < ROUNDS; i++) {
        if (CellularGetRegistrationStatus(&status) == 0) {
            if (status == 1 || status == 5) {
                PRINT_DEBUG("Cellular: registered")
                return 0;
            }
        }
    }
    PRINT_DEBUG("Cellular: wrong registration")
    return -1;
}


/**
 * function to pars one operator at a time.
 * @param start pointer to the first argument to pars.
 * @param len string len
 * @param op_info
 */
int pars_op(char *start, int len, OPERATOR_INFO *op_info) {
    char *status = strtok(start, ",");
    char *opname = strtok(NULL, ",");
    strtok(NULL, ",");
    char *opnum = strtok(NULL, ",\"");
    char *technology = strtok(NULL, ",\")");

    if (!status || !opname || !opnum || !technology ||
    (technology + strlen(technology) - start) > len) return -1;

    switch (*status) {
        case '0':
            op_info->operator_status = UNKNOWN_OPERATOR;
            break;
        case '1':
            op_info->operator_status = OPERATOR_AVAILABLE;
            break;
        case '2':
            op_info->operator_status = CURRENT_OPERATOR;
            break;
        case '3':
            op_info->operator_status = OPERATOR_FORBIDDEN;
            break;
        default:
            return -1;
    }
    switch (*technology) {
        case '0':
            memcpy(op_info->access_technology, "2G", 2);
            break;
        case '2':
            memcpy(op_info->access_technology, "3G",2);
            break;
        default:
            return -1;
    }
    op_info->operator_code = (int) strtol(opnum, NULL, 10);
    memcpy(op_info->operator_name, opname + 1, strlen(opname) - 2);
    return 0;
}


/**
 * Forces the modem to search for available operators (see “+COPS=?” command). Returns -1
 * if an error occurred or no operators found. Returns 0 and populates opList and opsFound if
 * the command succeeded.
 * opList is a pointer to the first item of an array of type OPERATOR_INFO, which is allocated
 * by the caller of this function. The array contains a total of maxops items. numOpsFound is
 * allocated by the caller and set by the function. numOpsFound will contain the number of
 * operators found and populated into the opList
 */
int CellularGetOperators(OPERATOR_INFO *opList, int maxops, int *numOpsFound) {
    PRINT_DEBUG("Cellular: getting operators")
    if(maxops == 0) return 0;
    if (opList == NULL || maxops < 0 || numOpsFound == NULL) {
        PRINT_DEBUG("Cellular: invalid input")
        return -1;
    }
    char buf[1024] = {0}, *ptr;
    SerialFlushInputBuff();
    if(SerialSend(COPS) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT+COPS=?")
        return -1;
    }

    if(SerialRecv(buf, 1023, LONG_TIME) == -1) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    int count = 0;
    while(buf[count++] != ' ' && count < 1023) {}

    ptr = buf + count;
    *numOpsFound = maxops;
    for (int i = 0; i < maxops; ++i) {
        count = 0;
        while (ptr[count++] != ')' && ((ptr + count) - buf) < 1024){}
        if (((ptr + count) - buf) >= 1024) {
            *numOpsFound = i;
            break;
        }
        if (pars_op(++ptr, count, opList + i)) {
            *numOpsFound = 0;
            PRINT_DEBUG("Cellular: error while parsing operators")
            return -1;
        }
        ptr += count;
    }
    PRINT_DEBUG("Cellular: got operators")
    return 0;
}


/**
 * Forces the modem to register/deregister with a network. Returns 0 if the
 * command was successful, returns -1 otherwise.
 * If mode=0, sets the modem to automatically register with an operator (ignores the
 * operatorCode parameter).
 * If mode=1, forces the modem to work with a specific operator, given in operatorCode.
 * If mode=2, deregisters from the network (ignores the operatorCode parameter).
 * See the “+COPS=<mode>,…” command for more details
 */
int CellularSetOperator(int mode, int operatorCode) {
    PRINT_DEBUG("Cellular: setting operator")
    char buf[50] = {0};
    char* tmp;
    switch (mode) {
        case SET_OPT_MODE_AUTO:
            tmp = "AT+COPS=0\r\n";
            break;
        case SET_OPT_MODE_MANUAL:
            snprintf(buf,50,"AT+COPS=1,2,\"%d\"\r\n",operatorCode);
            tmp = buf;
            break;
        case SET_OPT_MODE_DEREG:
            tmp = "AT+COPS=2\r\n";
            break;
        default:
            PRINT_DEBUG("Cellular: incorrect mode")
            return -1;
    }
    SerialFlushInputBuff();
    if(SerialSend(tmp, strlen(tmp)) == -1) {
        PRINT_DEBUG("Cellular: error in sending command to modem")
        return -1;
    }
    bzero(buf, 50);
    if(SerialRecv(buf, 10, LONG_TIME) == -1) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    if(strstr(buf,"OK")) {
        PRINT_DEBUG("Cellular: connected")
        return 0;
    }
    PRINT_DEBUG("Cellular: command fails")
    return -1;
}


/**
 * Returns -1 if the modem did not respond or respond with an error (note, CSQ=99 is also an error!)
 * Returns 0 if the command was successful and the signal quality was obtained from
 * the modem. In that case, the csq parameter will be populated with the numeric
 * value between -113dBm and -51dBm
 */
int CellularGetSignalQuality(int *csq) {
    PRINT_DEBUG("Cellular: get signal quality")
    if (csq == NULL) {
        PRINT_DEBUG("Cellular: invalid input")
        return -1;
    }
    SerialFlushInputBuff();
    if(SerialSend(CSQ) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT+CSQ")
        return -1;
    }
    char buf[50] = {0};
    if (SerialRecv(buf, 49, SHORT_TIME) == -1) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    int count = 0;
    while(buf[count++] != ' ' && count < 49) {}
    int rssi = (int) strtol(buf + count, NULL, 10);
    PRINTF_DEBUG("Cellular: rssi is: %d\n", rssi)
    if(rssi == 99){
        PRINT_DEBUG("Cellular: error not known or not detectable")
        return -1;
    }
    if (rssi < 2 || rssi > 31) {
        PRINT_DEBUG("Cellular: rssi value in invalid")
        return -1;
    }
    *csq = -113 + rssi * 2;
    PRINT_DEBUG("Cellular: got signal quality")
    return 0;
}


/**
 * Returns -1 if the modem did not respond or respond with an error.
 * Returns 0 if the command was successful and the ICCID was obtained from the modem.
 * iccid is a pointer to a char buffer, which is allocated by the caller of this function.
 * The buffer size is maxlen chars.
 * The obtained ICCID will be placed into the iccid buffer as a null-terminated string
 */
int CellularGetICCID(char* iccid, int maxlen) {
    PRINT_DEBUG("Cellular: get CCID")
    if (iccid == NULL || maxlen < 0) {
        PRINT_DEBUG("Cellular: function input is not valid")
        return -1;
    }
    SerialFlushInputBuff();
    if(SerialSend(CCID) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT+CCID")
        return -1;
    }
    char buf[50] = {0};
    int rc = SerialRecv(buf, 49, SHORT_TIME);
    if (rc == -1) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    int count = 0;
    while(buf[count++] != ' ' && count < 49){}
    int i = count;
    for(; i < MIN(maxlen , rc - count) + count; i++) {
        if(isalnum(buf[i])) {
            iccid[i - count] = buf[i];
        }
        else {
            i++;
            break;
        }
    }
    iccid[i - count] = '\0';
    PRINT_DEBUG("Cellular: got CCID")
    return 0;
}


/**
 * Returns -1 if the modem did not respond or respond with an error.
 * Returns 0 if the command was successful and the IMEI was obtained from the modem.
 * imei is a pointer to a char buffer, which is allocated by the caller of this function.
 * The buffer size is maxlen chars.
 * The obtained IMEI will be placed into the imei buffer as a null-terminated string
 */
int CellularGetIMEI(char* imei, int maxlen) {
    PRINT_DEBUG("Cellular: get IMEI")
    if (imei == NULL || maxlen < 0) {
        PRINT_DEBUG("Cellular: function input is not valid")
        return -1;
    }
    SerialFlushInputBuff();
    if(SerialSend(CGSN) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT+CGSN")
        return -1;
    }
    char buf[50] = {0};
    int rc = 0;
    if ((rc = SerialRecv(buf, 49, SHORT_TIME)) == -1) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    int count = 0;
    while(!isalnum(buf[count++]) || count > rc) {}
    int i = --count;
    for(; i < MIN(maxlen + count, rc); i++) {
        if(isalnum(buf[i])) {
            imei[i - count] = buf[i];
        }
        else {
            i++;
            break;
        }
    }
    imei[i - count] = '\0';
    PRINT_DEBUG("Cellular: got IMEI")
    return 0;
}


/**
 * function that extracts the info from the simo command from the right location in case its 2g or3g
 * @param sigInfo: struct to put the data in
 * @param buf: the read input
 * @param rc: the size of buffer that was read
 * @return: -1 on failure and 0 on success
 */
int signal_parse(SIGNAL_INFO *sigInfo, char *buf, int rc){
    int count = 0;
    while(buf[count++] != ' ' && count < rc) {}
    char *ptr = buf + count;
    if(strlen(ptr) < 1) {
        return -1;
    }
    char *p;
    char tmp[2] = {0};
    memcpy(tmp, ptr,2);
    ptr = strtok(ptr,",");
    if(strncmp(tmp,"2G",2) == 0) {
        memcpy(sigInfo ->access_technology, tmp, 2);
        for (int i =0; i < 15 ;i++) {
            ptr = strtok(NULL,",");
        }
        if(ptr == NULL) {
            return -1;
        }
        sigInfo->signal_power = strtol(ptr, &p, 10);
    }
    else if(strncmp(tmp, "3G", 2) == 0) {
        memcpy(sigInfo ->access_technology, tmp, 2);
        for (int i =0; i < 3; i++) {
            ptr = strtok(NULL, ",");
        }
        if(ptr == NULL) {
            return -1;
        }
        sigInfo->EC_n0 = strtol(ptr, &p, 10);
        ptr = strtok(NULL, ",");
        if(ptr == NULL) {
            return -1;
        }
        sigInfo->signal_power = strtol(ptr, &p, 10);
    }
    else {
        return -1;
    }
    return 0;
}


/**
 * Returns -1 if the modem did not respond, respond with an error, respond with SEARCH or NOCONN.
 * Returns 0 if the command was successful and the signal info was obtained from the modem.
 * sigInfo is a pointer to a struct, which is allocated by the caller of this function.
 * The obtained info will be placed into the sigInfo struct
 */
int CellularGetSignalInfo(SIGNAL_INFO *sigInfo) {
    PRINT_DEBUG("Cellular: get signal info")
    if (sigInfo == NULL) {
        PRINT_DEBUG("Cellular: invalid input")
        return -1;
    }
    SerialFlushInputBuff();
    if(SerialSend(SMONI) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT^SMONI")
        return -1;
    }
    char buf[1024] = {0};
    int rc = SerialRecv(buf, 1023, LONG_TIME);
    if (rc == -1) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    if(!strstr(buf,"SEARCH") || !strstr(buf,"LIMSRV") || !strstr(buf,"NOCONN")) {
        PRINTF_DEBUG(RECV_WRONG_RESPONSE " got: %s", buf)
        return -1;
    }
    if(signal_parse(sigInfo, buf, rc) == -1){
        PRINT_DEBUG("Cellular: incorrect line ")
        return -1;
    }
    return 0;
}


/**
* Initialize an internet connection profile (AT^SICS)
* with inactTO=inact_time_sec and
* conType=GPRS0 and apn="postm2m.lu". Return 0 on success,
* and -1 on failure.
*/
int CellularSetupInternetConnectionProfile(int inact_time_sec) {
    PRINT_DEBUG("Cellular: set connection profile")
    if (inact_time_sec < 0) {
        PRINT_DEBUG("Cellular: invalid input")
        return -1;
    }
    PRINT_DEBUG("Cellular: sending AT^SICS 1")
    SerialFlushInputBuff();
    if(SerialSend(SICS_CONTYPE) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT^SICS 1")
        return -1;
    }

    char buf[10] = {0};
    if(SerialRecv(buf, 9, SHORT_TIME) == -1){
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    if (!strstr(buf, "OK")) {
        PRINT_DEBUG(RECV_WRONG_RESPONSE " from AT^SICS 1")
        return -1;
    }
    char buf_send[1024] = {0};
    int f_size = snprintf(buf_send, 1023, FORAMT_SICS_INACT, inact_time_sec);
    PRINT_DEBUG("Cellular: sending AT^SICS 2")
    SerialFlushInputBuff();
    if(SerialSend(buf_send,f_size) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT^SICS 2")
        return -1;
    }

    bzero(buf,10);
    if(SerialRecv(buf, 9, SHORT_TIME) == -1) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    if (!strstr(buf,"OK")) {
        PRINT_DEBUG(RECV_WRONG_RESPONSE " from AT^SICS 2")
        return -1;
    }
    PRINT_DEBUG("Cellular: sending AT^SICS 3")
    SerialFlushInputBuff();
    if(SerialSend(SICS_APN) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT^SICS 3")
        return -1;
    }

    bzero(buf,10);
    if(SerialRecv(buf, 9, SHORT_TIME) == -1) {
        PRINT_DEBUG(RECV_FAILUR)
    }
    if (!strstr(buf,"OK")) {
        PRINT_DEBUG(RECV_WRONG_RESPONSE " from AT^SICS 3")
        return -1;
    }

    return 0;
}


/**
* Initialize an internal service profile (AT^SISS)
* with keepintvl=keepintvl_sec (the timer)
* and SrvType=Socket
* and conId=<CellularSetupInternetConnectionProfile_id>
* (if CellularSetupInternetConnectionProfile is already initialized.
* Return error, -1, otherwise)
* and Address=socktcp://IP:port;etx;time=keepintvl_sec.
* Return 0 on success, and -1 on failure.
*/
int CellularSetupInternetServiceProfile(char *IP, int port, int keepintvl_sec) {
    PRINT_DEBUG("Cellular: set internet service profile")
    if (!IP || keepintvl_sec < 0) {
        PRINT_DEBUG("Cellular: invalid input")
        return -1;
    }
    PRINT_DEBUG("Cellular: sending AT^SISS 1")
    SerialFlushInputBuff();
    if(SerialSend(SISS_SRVTYPE) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT^SISS 1")
        return -1;
    }

    char buf[10] = {0};
    if(SerialRecv(buf, 9, SHORT_TIME) == -1) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    if (!strstr(buf,"OK")) {
        PRINT_DEBUG(RECV_WRONG_RESPONSE " from AT^SISS 1")
        return -1;
    }
    PRINT_DEBUG("Cellular: sending AT^SISS 2")
    SerialFlushInputBuff();
    if(SerialSend(SISS_CONNID) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT^SISS 2")
        return -1;
    }

    bzero(buf,10);
    if(SerialRecv(buf, 9, SHORT_TIME) == -1) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }

    if (!strstr(buf,"OK")) {
        PRINT_DEBUG(RECV_WRONG_RESPONSE " AT^SISS 2")
        return -1;
    }

    char buf_send[1024] = {0};
    int f_size = snprintf(buf_send,1023, SISS_SOCKTCP_FORMAT, IP, port, keepintvl_sec);
    PRINT_DEBUG("Cellular: sending AT^SISS 3")
    SerialFlushInputBuff();
    if(SerialSend(buf_send,f_size) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT^SISS 3")
        return -1;
    }

    bzero(buf, 10);
    if(SerialRecv(buf, 9, SHORT_TIME) == -1) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    if (!strstr(buf,"OK")) {
        PRINT_DEBUG(RECV_WRONG_RESPONSE " from AT^SISS 3")
        return -1;
    }
    return 0;
}


/**
* Connects to the socket (establishes TCP connection to the pre-
defined host and port).
* Returns 0 on success, -1 on failure.
*/
int CellularConnect(void) {
    SerialFlushInputBuff();
    if(SerialSend(SISO_COMM) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT^SISO=0")
        return -1;
    }
    char buf[20] = {0};
    if(SerialRecv(buf, 18, SHORT_TIME) == -1) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    if ((!strstr(buf,"OK") && !strstr(buf,"^SISW"))) {
        PRINT_DEBUG(RECV_WRONG_RESPONSE " from AT^SISO=0")
        return -1;
    }
    SerialFlushInputBuff();
    if(SerialSend(SIST_COMM) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT^SIST=0")
        return -1;
    }
    bzero(buf,20);
    if(SerialRecv(buf, 18, SHORT_TIME) == -1) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    if (!strstr(buf,"OK") && !strstr(buf,"CONNECT")) {
        PRINT_DEBUG(RECV_WRONG_RESPONSE " from AT^SIST=0")
        return -1;
    }
    transparentMode = 1;
    return 0;
}


/**
* Writes len bytes from payload buffer to the established connection
* Returns the number of bytes written on success, -1 on failure
*/
int CellularWrite(unsigned char *payload, unsigned int len) {
    SerialFlushInputBuff();
    int rc = SerialSend(payload, len);
    if(rc == -1){
        PRINT_DEBUG("Cellular: failed to write")
    }
    return rc;
}


/**
* Reads up to max_len bytes from the established connection
* to the provided buf buffer, for up to timeout_ms
* (doesn’t block longer than that, even
* if not all max_len bytes were received).
* Returns the number of bytes read on success, -1 on failure.
*/
int CellularRead(unsigned char *buf, unsigned int max_len, unsigned int timeout_ms) {
    int rc = SerialRecv(buf, max_len, timeout_ms);
    if(rc == -1) {
        PRINT_DEBUG(RECV_FAILUR)
    }
    return rc;
}


/**
* Closes the established connection.
* Returns 0 on success, -1 on failure.
*/
int CellularClose() {
    char buf[20] = {0};
    if (transparentMode) {
        if (SerialSend(STOP_TRANSPARENT) == -1) {
            PRINT_DEBUG("Cellular: failed to send +++")
            return -1;
        }
        if (SerialRecv(buf, 15, SHORT_TIME) == -1) {
            PRINT_DEBUG(RECV_FAILUR)
            return -1;
        }
        if (!strstr(buf, "OK") && !strstr(buf, "NO CARRIER")) {
            PRINT_DEBUG(RECV_WRONG_RESPONSE " from +++")
            return -1;
        }
        bzero(buf, 10);
        transparentMode = 0;
    }
    SerialFlushInputBuff();
    if(SerialSend(SISC_COMM) == -1) {
        PRINT_DEBUG(SEND_FAILUR ": AT^SISC=0")
        return -1;
    }
    if(SerialRecv(buf, 9, SHORT_TIME) == -1) {
        PRINT_DEBUG(RECV_FAILUR)
        return -1;
    }
    if (!strstr(buf,"OK")) {
        PRINT_DEBUG(RECV_WRONG_RESPONSE " from AT^SISC=0")
        return -1;
    }
    return 0;
}
