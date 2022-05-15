import time

from paho.mqtt.client import Client, connack_string, error_string
from typing import Optional
import configparser
import telegram

_config = configparser.ConfigParser()
_config.read('config.ini')

client: Optional[Client] = None
broker = _config['mqtt']['broker']
topic_publish = _config['mqtt']['publish']
topic_subscribe = _config['mqtt']['subscribe']


def _on_connect(mqtt_client, telegram_client, _, return_code):
    """
    mqtt handler that send message to telegram when the server is connected
    """
    msg = 'Server failed to connect. CONNECTION_ERROR <'
    if return_code == 0:
        msg = '--**Server is Connected**--'
    else:
        msg += connack_string(return_code) + '>'
    telegram.send_message(msg)


def connect(subscribe=topic_subscribe):
    """
    function to connect and subscribe a topic
    :param subscribe: topic to subscribe
    """
    global client
    client = Client('TgServer', userdata=telegram.bot)
    client.on_connect = _on_connect
    client.connect(broker, 1883)
    client.subscribe(subscribe, 1)


def publish(msg: str, c: Client = None, qos=1):
    """
    publish a MQTT message
    :param msg: the message
    :param c: mqtt client
    :param qos: qos
    :return: result of the sending
    """
    time.sleep(1)
    if not c:
        global client
        c = client
    msg_inf = c.publish(topic_publish, msg, qos)
    return msg_inf.rc, error_string(msg_inf.rc), msg_inf


def open_door(c: Client = None, qos=1):
    """
    funcion to send 'open_door' to the device.
    :param c: mqtt client
    :param qos: qos
    """
    if not c:
        global client
        c = client
    while publish('open_door', c, qos)[0]:
        print('the door isn`t opened. retry')
