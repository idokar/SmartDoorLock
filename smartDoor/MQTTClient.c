#include "MQTTClient.h"
#include "wolfmqtt/mqtt_types.h"


typedef struct _SocketContext {
    char *host;
    MQTTCtx* mqttCtx;
} SocketContext;

static char IMEI[64];
static SocketContext g_sock;


/**
 * Connects the socket to the broker, to the given host and port
 * Returns 0 on success, and a negative number otherwise (one of MqttPacketResponseCodes)
 * timeout_ms defines the timeout in milliseconds
 */
static int NetConnect(void *context, const char* host, word16 port, int timeout_ms) {
    PRINT_DEBUG("MQTTClient: init socket")

    SocketContext *sock = (SocketContext *)context;
    sock->host = host;

    if (SocketInit(host, port) < 0) {
        PRINT_DEBUG("MQTTClient: Failed init socket")
        return MQTT_CODE_ERROR_SYSTEM;
    }

    CellularGetIMEI(IMEI, 64);
    sock->mqttCtx->app_name = IMEI;
    PRINT_DEBUG("MQTTClient: init socket successfully")
    PRINT_DEBUG("MQTTClient: Connecting to host")
    if (SocketConnect() < 0) {
        PRINT_DEBUG("HTTP: Failed connecting to host")
        return MQTT_CODE_ERROR_NETWORK;
    }
    PRINT_DEBUG("MQTTClient: Connected successfully")
    return MQTT_CODE_SUCCESS;
}


/**
 * Performs a network (socket) read from the connected broker,
 * to the given buffer buf, and reads buf_len bytes.
 * Returns number of read bytes on success, and a negative number otherwise (one of MqttPacketResponseCodes)
 * timeout_ms defines the timeout in milliseconds.
 */
static int NetRead(void *context, byte* buf, int buf_len, int timeout_ms) {
    int n;
    bzero(buf, buf_len);
    n = SocketRead(buf, buf_len, timeout_ms);
    if (n == 0) {
        PRINT_DEBUG("MQTTClient: socket timeout")
        return MQTT_CODE_ERROR_TIMEOUT;
    }
    if (n < 0) {
        PRINT_DEBUG("MQTTClient: Failed reading")
        return MQTT_CODE_ERROR_NETWORK;
    }
    return n;
}


/**
 * Performs a network (socket) write to the connected broker,
 * from the given buffer buf, with size of buf_len.
 * Returns the number of sent bytes on success, and a negative number otherwise (one of MqttPacketResponseCodes)
 * timeout_ms defines the timeout in milliseconds
 */
static int NetWrite(void *context, const byte* buf, int buf_len, int timeout_ms) {
    if (SocketWrite(buf, buf_len) < buf_len) {
        PRINT_DEBUG( "MQTTClient: Failed writing")
        return MQTT_CODE_ERROR_NETWORK;
    }
    return buf_len;
}


/**
 * Closes the network (socket) connection to the connected broker.
 * Returns 0, and a negative number otherwise (one of MqttPacketResponseCodes)
 */
static int NetDisconnect(void *context) {
    if (SocketClose() < 0) {
        PRINT_DEBUG("MQTTClient: Disconnection failed")
        return MQTT_CODE_ERROR_SYSTEM;
    }
    SocketDeInit();
    return MQTT_CODE_SUCCESS;
}


/**
 * Initializes the Net interface for communication
 */
int MqttClientNet_Init(MqttNet* net, MQTTCtx* mqttCtx) {
    if (net) {
        bzero(net,sizeof(MqttNet));
        net->connect = NetConnect;
        net->read = NetRead;
        net->write = NetWrite;
        net->disconnect = NetDisconnect;
        bzero(&g_sock,sizeof(g_sock));
        net->context = &g_sock;
        g_sock.mqttCtx = mqttCtx;
    }
    return MQTT_CODE_SUCCESS;
}


/**
 * De-Initializes all that was allocated by MqttClientNet
 */
int MqttClientNet_DeInit(MqttNet* net) {
    if (net) {
        if (net->context) {
            bzero(&g_sock,sizeof(g_sock));
        }
        bzero(net,sizeof(MqttNet));
        SocketDeInit();
    }
    return MQTT_CODE_SUCCESS;
}
