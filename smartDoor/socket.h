#ifndef EX5_SOCKET_H
#define EX5_SOCKET_H

#include "cellular.h"

/**
* Initializes the socket.
* Host: The destination address
* as DNS: en8wtnrvtnkt5.x.pipedream.net,
* or as IPv4: 35.169.0.97.
* Port: The communication endpoint, int, e.g.: 80.
* Returns 0 on success, -1 on failure
*/
int SocketInit(char *host, int port);

/**
* Connects to the socket
* (establishes TCP connection to the pre-defined host and port).
* Returns 0 on success, -1 on failure
*/
int SocketConnect(void);

/**
* Writes len bytes from the payload buffer
* to the established connection.
* Returns the number of bytes written on success, -1 on failure
*/
int SocketWrite(unsigned char *payload, unsigned int len);

/**
* Reads up to max_len bytes from the established connection
* to the provided buf buffer,
* for up to timeout_ms (doesn’t block longer than that,
* even if not all max_len bytes were received).
* Returns the number of bytes read on success, -1 on failure
*/
int SocketRead(unsigned char *buf, unsigned int max_len, unsigned int timeout_ms);

/**
* Closes the established connection.
* Returns 0 on success, -1 on failure
*/
int SocketClose(void);

/**
* Frees any resources that were allocated by SocketInit
*/
void SocketDeInit(void);


#endif //EX5_SOCKET_H
