import telegram
import mqtt
import db


def on_mqtt_message(_, telegram_client: telegram.Client, message):
    """
    MQTT message handler.
    :param telegram_client: instance of telegram connection.
    :param message: the MQTT message.
    """
    if message.topic != mqtt.topic_subscribe:
        return
    msg = message.payload.decode()
    if msg.startswith('connected'):
        return telegram.send_message('--**The door lock device is connected**--')
    elif msg.startswith('disconnected'):
        return telegram.send_message('--**The door lock device is disconnected**--‼')
    device = db.get_bt_device(msg)
    if device:
        with db.db_session:
            name = db.Device[msg].name
        telegram.send_message(f'The door unlocked now by: `{name}`')
        mqtt.open_door()
    else:
        telegram.send_message(f'Unknown device is near the door ({msg})')
        telegram_client.send_message(
            telegram.OWNER,
            f'New device is near the door `{msg}`\nWhat would you like to do?',
            reply_markup=telegram.get_new_device_kb(msg))


if __name__ == '__main__':
    db.start()
    telegram.bot.start()
    mqtt.connect()
    mqtt.client.on_message = on_mqtt_message
    mqtt.client.loop_start()
    telegram.run_bot()
    mqtt.client.disconnect()
    mqtt.client.loop_stop()
    telegram.bot.stop()
