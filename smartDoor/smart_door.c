/**
 * LEDS DEFINES
 */
#ifndef LED_INSTANCE0
#define LED_INSTANCE0    sl_led_led0
#define LED_INSTANCE1    sl_led_led1
#endif

#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include "em_common.h"
#include "sl_bluetooth.h"
#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif // SL_COMPONENT_CATALOG_PRESENT
#ifdef SL_CATALOG_CLI_PRESENT
#include "sl_cli.h"
#endif // SL_CATALOG_CLI_PRESENT
#include <smart_door.h>
#include <string.h>
#include "sl_system_init.h"
#include "sl_system_process_action.h"
#include "sl_simple_button_instances.h"
#include "timer.h"
#include "serial_io.h"
#include "MQTTClient.h"
#include "sl_simple_led_instances.h"

/* MQTT DEFINES */
#define DEFAULT_BROKER_HOST "broker.mqttdashboard.com"//"18.158.198.79"
#define DEFUALT_BROKER_PORT 1883
#define DEFAULT_KEEP_ALIVE_SEC 720
#define DEFAULT_MQTT_QOS 1
#define CLIENT_ID "hujiIotMichIdo "
#define TOPIC_SEND "smart_door_lock/iot/device_send"
#define TOPIC_RECV "smart_door_lock/iot/device_recv"
#define OPEN_DOOR_CMD "open_door"
#define LOCK_DOOR_CMD "lock"
#define UNLOCK_DOOR_CMD "unlock"
#define NORMAL_DOOR_STAT "normal"
#define LWT "{\n    \"DisconnectedGracefully\":false\n}"
#define CLEAN_SEASON 0
#define LWT_STAT 1
#define RETAIN 0
#define MQTT_MAX_PACKET_SZ 512
#define TIMEOUT 15000
#define FAIL (-1)

static byte mReadBuf[MQTT_MAX_PACKET_SZ] = {0};
static byte mSendBuf[MQTT_MAX_PACKET_SZ] = {0};
static volatile word16 mPacketIdLast;

/**
 * WARNING: not all blutooth devices work the same, for information about
 * bluetooth addresses and privacy in bluetooth low energy use the next links:
 * - https://www.bluetooth.com/blog/bluetooth-technology-protecting-your-privacy/
 * - https://www.novelbits.io/bluetooth-address-privacy-ble/
 */

/**
 * BLUETOOTH DEFINES
 */
#define CONN_INTERVAL_MIN             80   //100ms
#define CONN_INTERVAL_MAX             80   //100ms
#define CONN_RESPONDER_LATENCY        0    //no latency
#define CONN_TIMEOUT                  100  //1000ms
#define CONN_MIN_CE_LENGTH            0
#define CONN_MAX_CE_LENGTH            0xffff
#define SCAN_INTERVAL                 16   //10ms
#define SCAN_WINDOW                   16   //10ms
#define SCAN_PASSIVE                  0
#define CHECK_BIT(var,pos) ( (((var) & (pos)) > 0 ) ? (1) : (0) )
#define BT_LST_SIZE 5
#define BT_DEVICE_LIFETIME 34000
#define SENT_LST_SIZE 32

typedef enum doorStatus{
    closed = 0,//!< closed
    open,      //!< open
    unlocked,  //!< unlocked
    locked     //!< locked
} doorStatus;

typedef struct door{
    doorStatus stat;
    uint32_t open_time;
} door;

door dr_iot={0};

typedef struct bt_device{
    uint32_t timestamp;
    uint8_t addr[6];
} bt_device;

typedef struct _bt_device_lst{
    bt_device device_lst[BT_LST_SIZE];
    uint8_t i;
} _bt_device_lst;

_bt_device_lst bt_lst = {0};
bt_device sent_lst[SENT_LST_SIZE] = {0};


/**
 * @param mqttCtx :MQTTCtx object to init with data
 */
void mqtt_init_ctx(MQTTCtx* mqttCtx) {
    mqttCtx->host = DEFAULT_BROKER_HOST;
    mqttCtx->port = DEFUALT_BROKER_PORT;
    mqttCtx->qos = DEFAULT_MQTT_QOS;
    mqttCtx->clean_session = CLEAN_SEASON;
    mqttCtx->keep_alive_sec = DEFAULT_KEEP_ALIVE_SEC;
    mqttCtx->client_id = CLIENT_ID;
    mqttCtx->topic_name = TOPIC_SEND;
    mqttCtx->cmd_timeout_ms = TIMEOUT;
    bzero(mSendBuf, MQTT_MAX_PACKET_SZ);
    mqttCtx->tx_buf = mSendBuf;
    bzero(mReadBuf, MQTT_MAX_PACKET_SZ);
    mqttCtx->rx_buf = mReadBuf;
    mqttCtx->client.ctx=&mqttCtx;
    mqttCtx->topics[0].qos = DEFAULT_MQTT_QOS;
}


/**
 * this function runs when receiving data while reading
 * @param client : MqttClient object
 * @param msg : MqttMessage object
 * @param msg_new : first data callback
 * @param msg_done : last data callback
 * @return : 0
 */
static int mqtt_message_cb(MqttClient *client, MqttMessage *msg, byte msg_new, byte msg_done) {
    byte buf[MQTT_MAX_PACKET_SZ + 1];
    word32 len = 0;
    (void) client;
    len = msg->buffer_len;
    if (len > MQTT_MAX_PACKET_SZ) {
        len = MQTT_MAX_PACKET_SZ;
    }
    memcpy(buf, msg->buffer, len);
    buf[len] = '\0';
    if((dr_iot.stat != locked && strcmp(buf, OPEN_DOOR_CMD) == 0) ||
       (dr_iot.stat == unlocked && strcmp(buf, NORMAL_DOOR_STAT) == 0)) {
        dr_iot.open_time = cur_time();
        dr_iot.stat = open;
        return 0;
      }
      else if(strcmp(buf, UNLOCK_DOOR_CMD) == 0) {
          dr_iot.stat = unlocked;
          return 0;
      }
      else if(dr_iot.stat == locked && strcmp(buf, NORMAL_DOOR_STAT) == 0){
          dr_iot.stat = closed;
          return 0;
      }
      else if(strcmp(buf, LOCK_DOOR_CMD) == 0) {
          dr_iot.stat = locked;
          return  0;
      }
      return MQTT_CODE_SUCCESS;
}


/**
 * @return : id of the packet
 */
word16 mqtt_get_packetid(void) {
    if (mPacketIdLast >= MAX_PACKET_ID) {
        mPacketIdLast = 0;
    }
    return ++mPacketIdLast;
}


/**
 * sets the parameters in mqt->connect and connects
 * @param mqt : MQTTCtx object
 * @return : -1 if encountered with an error else 0
 */
int connect_mqtt(MQTTCtx *mqt) {
    mqt->connect.keep_alive_sec = mqt->keep_alive_sec;
    mqt->connect.clean_session = mqt->clean_session;
    mqt->connect.client_id = mqt->client_id;
    int rc = MqttClient_NetConnect(&mqt->client, mqt->host, mqt->port, 5000, mqt->use_tls, NULL);
    if (rc != MQTT_CODE_SUCCESS) {
        return FAIL;
    }
    PRINTF_DEBUG("MQTT Sock Conn: %s (%d)\n", MqttClient_ReturnCodeToString(rc), rc)
    return 0;
}


/**
 * adds lwt to the connections
 * @param mqt : MQTTCtx object
 * @return : -1 if encountered with an error else 0
 */
int lwt_connect(MQTTCtx *mqt) {
    mqt->enable_lwt = LWT_STAT;
    mqt->connect.enable_lwt = mqt->enable_lwt;
    mqt->connect.lwt_msg = &mqt->lwt_msg;
    if (mqt->enable_lwt) {
        mqt->lwt_msg.qos = DEFAULT_MQTT_QOS;
        mqt->lwt_msg.retain = RETAIN;
        mqt->lwt_msg.topic_name = TOPIC_SEND;
        mqt->lwt_msg.buffer = (byte *) LWT;
        mqt->lwt_msg.total_len = (word16) XSTRLEN(LWT);
    }
    int rc = MqttClient_Connect(&mqt->client, &mqt->connect);
    if (rc != MQTT_CODE_SUCCESS) {
        return FAIL;
    }
    return 0;
}


/**
 * subscribe to topics
 * @param mqt : MQTTCtx object
 * @return : -1 if encountered with an error else 0
 */
int subscribes(MQTTCtx *mqt) {
    mqt->subscribe.packet_id = mqtt_get_packetid();
    mqt->topics[0].topic_filter = TOPIC_RECV;
    mqt->subscribe.topic_count = sizeof(mqt->topics) / sizeof(MqttTopic);
    mqt->subscribe.topics = mqt->topics;
    int rc = MqttClient_Subscribe(&mqt->client, &mqt->subscribe);
    if (rc != MQTT_CODE_SUCCESS) {
        return FAIL;
    }
    PRINTF_DEBUG("MQTT Subscribe: %s (%d)\n", MqttClient_ReturnCodeToString(rc), rc)
    return 0;
}


/**
 * publish a message to the given topic
 * @param mqt : MQTTCtx object
 * @param topic : the topic we want to publish our msg
 * @param msg : the message we want to publish
 * @return : -1 if encountered with an error else 0
 */
int publish_msg(MQTTCtx mqt,const char *topic,const char *msg) {
    mqt.publish.qos = mqt.qos;
    mqt.publish.topic_name = topic;
    mqt.publish.packet_id = mqtt_get_packetid();
    mqt.publish.buffer = (byte*)msg;
    mqt.publish.total_len = (word16)XSTRLEN(msg);
    int rc = MqttClient_Publish(&mqt.client, &mqt.publish);
    PRINTF_DEBUG("MQTT Pub: Topic: %s\nMessage:\n%s\n%s (%d)\n",
                 mqt.publish.topic_name,msg, MqttClient_ReturnCodeToString(rc), rc)
    return (rc != MQTT_CODE_SUCCESS) ? -1:0;
}


/**
 * waits for a message
 * @param mqt : MQTTCtx object
 * @return : -1 if encountered with an error else 0
 */
int read_msg() {
    int rc;
    rc = MqttClient_WaitMessage(&mqt.client, mqt.cmd_timeout_ms);
    if(rc == MQTT_CODE_SUCCESS) {
        PRINT_DEBUG("read from sub topics\n")
        return 0;
    }
    if(rc != MQTT_CODE_ERROR_TIMEOUT) {
        PRINTF_DEBUG("error num: %d\n", rc)
        return rc;
    }
    rc = MqttClient_Ping_ex(&mqt.client, &mqt.ping);
    if (rc != MQTT_CODE_SUCCESS) {
        PRINTF_DEBUG("connection err: %d\n", rc)
        return FAIL;
    }
    return 0;
}


/**
 * sets paths to all topics
 * @param mqt: MQTTCtx struct
 */
void bz_mqttCtx(MQTTCtx *mqt) {
    bzero(&mqt->connect, sizeof(MqttConnect));
    bzero(&mqt->publish, sizeof(MqttPublish));
    bzero(&mqt->subscribe, sizeof(MqttSubscribe));
    bzero(&mqt->client, sizeof(MqttClient));
    bzero(&mqt->lwt_msg, sizeof(MqttMessage));
    bzero(&mqt->net, sizeof(MqttNet));
}


/**
 * disconnects the relevant connections
 * @param mqt: MQTTCtx object
 */
void on_fail(MQTTCtx *mqt) {
    MqttClient_Disconnect_ex(&mqt->client, &mqt->disconnect);
    MqttClientNet_DeInit(&mqt->net);
}


/**
 * connects to broker and publish /read as instructed in the submission
 * @return: 0 on success otherwise -1
 */
int run_mqtt() {

    bz_mqttCtx(&mqt);
    mqtt_init_ctx(&mqt);
    MqttClientNet_Init(&(mqt.net), &mqt);
    int rc = MqttClient_Init(&mqt.client, &mqt.net, mqtt_message_cb, mqt.tx_buf, MQTT_MAX_PACKET_SZ,
                             mqt.rx_buf, MQTT_MAX_PACKET_SZ, mqt.cmd_timeout_ms);
    if(rc < 0) {
        PRINT_DEBUG("Mqtt client init failed")
        return FAIL;
    }
    sl_led_turn_on(&LED_INSTANCE1);
    if (connect_mqtt(&mqt) == FAIL) {
        PRINT_DEBUG("Mqtt failed to connect")
        on_fail(&mqt);
        return FAIL;
    }
    sl_led_turn_off(&LED_INSTANCE1);

    if (lwt_connect(&mqt) == FAIL) {
        PRINT_DEBUG("Mqtt lwt failed")
        on_fail(&mqt);
        return FAIL;
    }
    mqt.topic_name = TOPIC_RECV;
    if (subscribes(&mqt) == FAIL) {
        PRINT_DEBUG("Mqtt failed to subscribe")
        on_fail(&mqt);
        return FAIL;
    }
    mqt.topic_name = TOPIC_SEND;
    publish_msg(mqt,TOPIC_SEND,"connected");

    return 0;
}


/**
 * initialize the application
 * @return 0 on success else -1
 */
int app_init(void) {
    sl_system_init();
    sl_system_process_action();
    return 0;
}


/**
 * checks if addr in device, if yes then the interval between
 * its creation to the cur time.
 * @param device: strcut that represent bluetooth device.
 * @param addr: bluetooth address
 * @return: 1 if bluetooth differ or time passed too long else 0
 */
int is_available(bt_device* device, const uint8_t* addr) {
    for (int i = 0; i < 6; i++) {
        if (device->addr[i] != addr[i]) {
            return 1;
        }
    }
    return cur_time() - device->timestamp >= BT_DEVICE_LIFETIME;
}


/**
 * checks if we need to add this device to
 * @param address: bd_addr from bluetooth scanning event handler
 * @return 0 on success else -1
 */
int add_bt_device(bd_addr address) {
    int idx = bt_lst.i;
    bt_lst.i = (bt_lst.i + 1) % BT_LST_SIZE;
    for (int i = 0; i < BT_LST_SIZE; i++) {
        bool contains = ((cur_time() - bt_lst.device_lst[i].timestamp) < BT_DEVICE_LIFETIME);
        for (int j = 0; j < 6; j++) {
            contains &= (bt_lst.device_lst[i].addr[j] == address.addr[j]);
        }
        if (!is_available(bt_lst.device_lst + i, address.addr)) {
            return -1;
        }
    }
    for (int i = 0; i < 6; i++) {
        bt_lst.device_lst[idx].addr[i] = address.addr[i];
    }
    bt_lst.device_lst[idx].timestamp = cur_time();
    return idx;
}


/**
 * sends bluetooth devices to MQTT
 */
void send_device() {
    int last_index = -1;
    for (int i = 0; i < BT_LST_SIZE; i++) {
        bt_device cur = bt_lst.device_lst[i];
        int idx = cur.addr[0] & (SENT_LST_SIZE - 1);
        if (idx != last_index && cur.timestamp != 0 && is_available(sent_lst + idx, cur.addr)) {
            last_index = idx;
            bzero(bt_lst.device_lst + i, sizeof(bt_device));
            char buf[18] = {0};
            snprintf(buf, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
                     cur.addr[5], cur.addr[4], cur.addr[3],
                     cur.addr[2], cur.addr[1], cur.addr[0]);
            publish_msg(mqt,TOPIC_SEND,buf);
            memcpy(sent_lst + idx, &cur, sizeof(bt_device));
        }
    }
}


/**
 * occurs if there was a bluetooth event caught by the system.
 * @param evt: bluetooth event
 */
void sl_bt_on_event(sl_bt_msg_t* evt) {
    sl_status_t sc;
    // Handle stack events
    switch (SL_BT_MSG_ID(evt->header)) {
        // -------------------------------
        // This event indicates the device has started and the radio is ready.
        // Do not call any stack command before receiving this boot event!
        case sl_bt_evt_system_boot_id:
            // Set passive scanning on 1Mb PHY
            // Set scan interval and scan window
            sc = sl_bt_scanner_set_timing(sl_bt_gap_1m_phy, SCAN_INTERVAL, SCAN_WINDOW);
            // Set the default connection parameters for subsequent connections
            sc = sl_bt_connection_set_default_parameters(
                CONN_INTERVAL_MIN, CONN_INTERVAL_MAX, CONN_RESPONDER_LATENCY,
                CONN_TIMEOUT, CONN_MIN_CE_LENGTH, CONN_MAX_CE_LENGTH);
            // Start scanning - looking for thermometer devices
            sc = sl_bt_scanner_start(sl_bt_gap_1m_phy , sl_bt_scanner_discover_observation);
            break;
        // -------------------------------
        // This event is generated when an advertisement packet or a scan response
        // is received from a responder
        case sl_bt_evt_scanner_scan_report_id:
            if(evt->data.evt_scanner_scan_report.rssi > -50) {
                add_bt_device(((sl_bt_evt_scanner_scan_report_t*)&(evt->data))->address);
            }
            break;
        default:
            break;
    }
}


/**
 * scans for bluetooth devices in case the door is closed
 */
void bt_scan(){
  if(dr_iot.stat == closed){
      sl_bt_step();
      send_device();
  }
}


/**
 * controls the leds based on the statues of the door
 * @param handle: sleeptimer handler
 * @param data: additional data.
 */
static void periodic_timeout(sl_sleeptimer_timer_handle_t *handle, void *data) {
    if(dr_iot.stat == open && cur_time() - dr_iot.open_time < 30000) {
        sl_led_turn_on(&LED_INSTANCE0);
        return;
    }
    if(dr_iot.stat == locked) {
        sl_led_turn_off(&LED_INSTANCE0);
        return;
    }
    if(dr_iot.stat == unlocked) {
        sl_led_turn_on(&LED_INSTANCE0);
        return;
    }
    dr_iot.stat = closed;
    sl_led_turn_off(&LED_INSTANCE0);
}


/**
 * main application routine that controls the bluetooth discovery and door.
 */
void run_app(void) {
    sl_sleeptimer_timer_handle_t periodic_time;
    int ret = 0;
    do {
        ret = run_mqtt();
    } while (ret);
    sl_sleeptimer_start_periodic_timer(
        &periodic_time, 10000, periodic_timeout, NULL, 0,
        SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);
    while(1) {
        if(dr_iot.stat == closed) {
            for(int i = 0; i < 10; i++) {
                sl_bt_step();
            }
            send_device();
        }
        read_msg(mqt);
    }
}
