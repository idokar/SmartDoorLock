#ifndef EX9_CELLULAR_H
#define EX9_CELLULAR_H

#include "serial_io.h"

#define MAX_OPERATOR_NAME 30
#define MAX_TECH_SIZE 4
#define SET_OPT_MODE_AUTO 0
#define SET_OPT_MODE_MANUAL 1
#define SET_OPT_MODE_DEREG 2


typedef enum __OP_STATUS {
    UNKNOWN_OPERATOR = 0,
    OPERATOR_AVAILABLE = 1,
    CURRENT_OPERATOR = 2,
    OPERATOR_FORBIDDEN = 3
} OP_STATUS;

typedef struct __OPERATOR_INFO {
    char operator_name[MAX_OPERATOR_NAME]; // Long name. See <format> under +COPS
    int operator_code; // Short name. See <format> under +COPS
    char access_technology[MAX_TECH_SIZE]; // "2G" or "3G"
    OP_STATUS operator_status;
} OPERATOR_INFO;

typedef struct __SIGNAL_INFO {
    int signal_power; // In 2G: dBm. In 3G: RSCP. See ^SMONI responses
    int EC_n0; // In 3G only. See ^SMONI responses
    char access_technology[MAX_TECH_SIZE]; // "2G" or "3G"
} SIGNAL_INFO;

/**
 * Initialize whatever is needed to start working with the cellular modem (e.g. the serial port).
 * Returns 0 on success, and -1 on failure
 */
int CellularInit(char *port);

/**
 * Deallocate / close whatever resources CellularInit() allocated
 */
void CellularDisable(void);

/**
 * Checks if the modem is responding to AT commands
 * Return 0 if it does, returns -1 otherwise
 */
int CellularCheckModem(void);

/**
 * Checks if the modem is responding to AT commands
 * Return 0 if it does, returns -1 otherwise
 * Tries multiple times until succeeds or fails
 */
int CellularWaitUntilModemResponds(void);

/**
 * Returns -1 if the modem did not respond or respond with an error.
 * Returns 0 if the command was successful and the registration status was obtained from
 * the modem. In that case, the status parameter will be populated with the numeric value
 * of the <regStatus> field of the “+CREG” AT command
 */
int CellularGetRegistrationStatus(int *status);

/**
 * Returns -1 if the modem did not respond or respond with an error or couldn't register.
 * Returns 0 if the command was successful and the registration status was obtained from
 * the modem and the modem is registered (1 or 5).
 */
int CellularWaitUntilRegistered(void);

/**
 * Forces the modem to search for available operators (see “+COPS=?” command). Returns -1
 * if an error occurred or no operators found. Returns 0 and populates opList and opsFound if
 * the command succeeded.
 * opList is a pointer to the first item of an array of type OPERATOR_INFO, which is allocated
 * by the caller of this function. The array contains a total of maxops items. numOpsFound is
 * allocated by the caller and set by the function. numOpsFound will contain the number of
 * operators found and populated into the opList
 */
int CellularGetOperators(OPERATOR_INFO *opList, int maxops, int *numOpsFound);

/**
 * Forces the modem to register/deregister with a network. Returns 0 if the
 * command was successful, returns -1 otherwise.
 * If mode=0, sets the modem to automatically register with an operator (ignores the
 * operatorCode parameter).
 * If mode=1, forces the modem to work with a specific operator, given in operatorCode.
 * If mode=2, deregisters from the network (ignores the operatorCode parameter).
 * See the “+COPS=<mode>,…” command for more details
 */
int CellularSetOperator(int mode, int operatorCode);

/**
 * Returns -1 if the modem did not respond or respond with an error (note, CSQ=99 is also an error!)
 * Returns 0 if the command was successful and the signal quality was obtained from
 * the modem. In that case, the csq parameter will be populated with the numeric
 * value between -113dBm and -51dBm
 */
int CellularGetSignalQuality(int *csq);

/**
 * Returns -1 if the modem did not respond or respond with an error.
 * Returns 0 if the command was successful and the ICCID was obtained from the modem.
 * iccid is a pointer to a char buffer, which is allocated by the caller of this function.
 * The buffer size is maxlen chars.
 * The obtained ICCID will be placed into the iccid buffer as a null-terminated string
 */
int CellularGetICCID(char* iccid, int maxlen);

/**
 * Returns -1 if the modem did not respond or respond with an error.
 * Returns 0 if the command was successful and the IMEI was obtained from the modem.
 * imei is a pointer to a char buffer, which is allocated by the caller of this function.
 * The buffer size is maxlen chars.
 * The obtained IMEI will be placed into the imei buffer as a null-terminated string
 */
int CellularGetIMEI(char* imei, int maxlen);

/**
 * Returns -1 if the modem did not respond, respond with an error, respond with SEARCH or NOCONN.
 * Returns 0 if the command was successful and the signal info was obtained from the modem.
 * sigInfo is a pointer to a struct, which is allocated by the caller of this function.
 * The obtained info will be placed into the sigInfo struct
 */
int CellularGetSignalInfo(SIGNAL_INFO *sigInfo);

/**
* Connects to the socket (establishes TCP connection to the pre-
defined host and port).
* Returns 0 on success, -1 on failure.
*/
int CellularConnect(void);

/**
* Closes the established connection.
* Returns 0 on success, -1 on failure.
*/
int CellularClose();

/**
* Writes len bytes from payload buffer to the established connection
* Returns the number of bytes written on success, -1 on failure
*/
int CellularWrite(unsigned char *payload, unsigned int len);

/**
* Reads up to max_len bytes from the established connection
* to the provided buf buffer, for up to timeout_ms
* (doesn’t block longer than that, even
* if not all max_len bytes were received).
* Returns the number of bytes read on success, -1 on failure.
*/
int CellularRead(unsigned char *buf, unsigned int max_len, unsigned int timeout_ms);

/**
* Initialize an internet connection profile (AT^SICS)
* with inactTO=inact_time_sec and
* conType=GPRS0 and apn="postm2m.lu". Return 0 on success,
* and -1 on failure.
*/
int CellularSetupInternetConnectionProfile(int inact_time_sec);

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
int CellularSetupInternetServiceProfile(char *IP, int port, int keepintvl_sec);

#endif //EX9_CELLULAR_H
