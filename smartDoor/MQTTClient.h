#ifndef EX1_MQTTCLIENT_H
#define EX1_MQTTCLIENT_H

#include "wolfmqtt/mqtt_socket.h"
#include "wolfmqtt/mqtt_packet.h"
#include "wolfmqtt/mqtt_client.h"
#include "socket.h"


/* MQTT Client state */
typedef enum _MQTTCtxState {
    WMQ_BEGIN = 0,
    WMQ_NET_INIT,
    WMQ_INIT,
    WMQ_TCP_CONN,
    WMQ_MQTT_CONN,
    WMQ_SUB,
    WMQ_PUB,
    WMQ_WAIT_MSG,
    WMQ_PING,
    WMQ_UNSUB,
    WMQ_DISCONNECT,
    WMQ_NET_DISCONNECT,
    WMQ_DONE
} MQTTCtxState;

/* MQTT Client context */
/* This is used for the examples as reference */
/* Use of this structure allow non-blocking context */
typedef struct _MQTTCtx {
    MQTTCtxState stat;

    void* app_ctx; /* For storing application specific data */

    /* client and net containers */
    MqttClient client;
    MqttNet net;

    /* temp mqtt containers */
    MqttConnect connect;
    MqttMessage lwt_msg;
    MqttSubscribe subscribe;
    MqttUnsubscribe unsubscribe;
    MqttTopic topics[1];
    MqttPublish publish;
    MqttDisconnect disconnect;
    MqttPing ping;

    /* configuration */
    MqttQoS qos;
    const char* app_name;
    const char* host;
    const char* username;
    const char* password;
    const char* topic_name;
    const char* message;
    const char* pub_file;
    const char* client_id;
    char * operators; //added
    byte *tx_buf, *rx_buf;
    int return_code;
    int use_tls;
    int retain;
    int enable_lwt;
    word32 cmd_timeout_ms;
    word16 keep_alive_sec;
    word16 port;
    byte    clean_session;
    byte    test_mode;
    unsigned int dynamicTopic:1;
    unsigned int dynamicClientId:1;

} MQTTCtx;

/**
 * Initializes the Net interface for communication
 */
int MqttClientNet_Init(MqttNet* net, MQTTCtx* mqttCtx);

/**
 * De-Initializes all that was allocated by MqttClientNet
 */
int MqttClientNet_DeInit(MqttNet* net);

#endif //EX1_MQTTCLIENT_H
