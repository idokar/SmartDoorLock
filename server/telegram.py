from pyrogram import Client, filters
from pyrogram.methods.utilities.idle import idle
from pyrogram.errors import PeerIdInvalid, MessageDeleteForbidden
from pyrogram.types import InlineKeyboardMarkup, InlineKeyboardButton
import configparser
import mqtt
import db

_config = configparser.ConfigParser()
_config.read('config.ini')

OWNER = _config['chats'].getint('owner')
LOG_CHAT = _config['chats'].getint('log_channel')

bot = Client('IOT_final')

device_whit_for_save = []


def run_bot():
    """
    main function to connect the bot to telegram.
    """
    try:
        bot.run(idle())
        send_message('--**Server is done**--')
    except ConnectionError:
        return send_message('--**Server is done**--')


def send_message(msg: str):
    """
    send message to the LOG channel.
    :param msg: the message.
    """
    try:
        bot.send_message(LOG_CHAT, msg)
    except (ConnectionError, PeerIdInvalid) as e:
        print(e)


def get_new_device_kb(bt_id):
    """
    return a keyboard for unknown device.
    :param bt_id: the device bluetooth_id.
    :return: a keyboard object.
    """
    return InlineKeyboardMarkup([
        [InlineKeyboardButton('Ignore ❌', callback_data='ignore'),
         InlineKeyboardButton('Open the door 🚪', callback_data='open')],
        [InlineKeyboardButton('Add device', callback_data=f'add: {bt_id}')]
    ])


def _device_settings(d):
    """
    generate a setting and DATA for a device.
    :param d: the device bluetooth_id.
    :return: dict with the keyboard and the text message.
    """
    with db.db_session:
        d = db.Device[d]
        return {'text': 'Please select what to do with the device:\n'
                        f'**ID:** {d.bluetooth_id}\n**Name:** {d.name}\n'
                        f'**Registered:** {d.registered_time:%d/%m/%Y}\n' +
                        (f'**Until:** {d.until:%d/%m/%Y}' if d.until else ''),
                'reply_markup': InlineKeyboardMarkup([
                    [InlineKeyboardButton('Remove ⛔️', callback_data=f'remove {d.bluetooth_id}')],
                    [InlineKeyboardButton('+1 day', callback_data=f'until 1 {d.bluetooth_id}'),
                     InlineKeyboardButton('+3 days', callback_data=f'until 3 {d.bluetooth_id}'),
                     InlineKeyboardButton('+1 week', callback_data=f'until 7 {d.bluetooth_id}')],
                    [InlineKeyboardButton('♾', callback_data=f'until 0 {d.bluetooth_id}')]])}


@bot.on_callback_query(filters.user(OWNER))
def callback_handler(c, q):
    """
    handler for buttons cliks.
    :param c: telegram client.
    :param q: query.
    """
    if q.data == 'ignore':
        return q.message.delete()
    if q.data == 'open':
        mqtt.open_door()
        q.message.reply_markup.inline_keyboard.pop(0)
        return q.message.edit_reply_markup(q.message.reply_markup)
    elif q.data.startswith('add'):
        global device_whit_for_save
        q.message.reply('Please send me a name for the device:')
        device_whit_for_save.append(q.data.split()[-1])
    elif q.data.startswith('new'):
        db.add_device(*q.data.split()[1:])
        q.message.reply('**Device added successfully**')
    elif q.data.startswith('remove'):
        with db.db_session:
            d = db.get_bt_device(q.data.split()[-1])
            if d:
                d.delete()
    elif q.data.startswith('until'):
        data = q.data.split()[1:]
        db.update_until_time(int(data[0]), data[1])
    else:
        d = db.get_bt_device(q.data)
        if d:
            d = d.bluetooth_id
            q.message.reply(**_device_settings(d))
    try:
        q.message.delete()
    except MessageDeleteForbidden:
        pass


@bot.on_message(filters.private & filters.user(OWNER) & filters.text, group=1)
def get_device_name(c, m):
    """
    handler to set a name for a device.
    :param c: telegram client.
    :param m: message.
    """
    if m.text.startswith('/'):
        return
    global device_whit_for_save
    if device_whit_for_save:
        if not m.text.isalnum():
            return m.reply('**The name should contain only A-Z a-z 0-9**')
        bt_id = device_whit_for_save.pop()
        m.reply(f'the name you chose for `{bt_id}` is: `{m.text}`',
                reply_markup=InlineKeyboardMarkup([
                    [InlineKeyboardButton('Cancel ❌', callback_data='ignore'),
                     InlineKeyboardButton('Accept ✅', callback_data=f'new {bt_id} {m.text}')],
                    [InlineKeyboardButton('Edit ✏️', callback_data=f'add: {bt_id}')]
                ]))


# @bot.on_message(filters.private & filters.command('start') & filters.user(OWNER))
# def start_bot(c, m):
#     print(m)


@bot.on_message(filters.private & filters.command(['lock', 'unlock', 'auto', 'open']) & filters.user(OWNER))
def force_lock_or_open(c, m):
    """
    commands handler.
    :param c: telegram client.
    :param m: message.
    """
    if m.command[0] == 'unlock':
        mqtt.publish('unlock')
        send_message(f'**--Status:-- door is Unlocked**')
    elif m.command[0] == 'lock':
        mqtt.publish('lock')
        send_message(f'**--Status:-- door is locked**')
    elif m.command[0] == 'open':
        mqtt.publish('open_door')
    else:
        mqtt.publish('normal')
        send_message(f'**--Status:-- the system is back to normal**')
    m.stop_propagation()


@bot.on_message(filters.private & filters.command('list') & filters.user(OWNER))
def list_devices(c, m):
    """
    display a list with all the devices on the DB.
    :param c: telegram client.
    :param m: message.
    """
    msg = 'To edit one of the devices just press on the button with the device name.\n'
    kb = []
    ls = []
    for i, item in enumerate(db.list_devices().items(), 1):
        msg += f'**{i}.** `{item[1][0]}`' + (
            f' - until:  {item[1][1].date():%d/%m/%Y}\n' if item[1][1] else '\n')
        ls.append(InlineKeyboardButton(f'{i}. {item[1][0]}', callback_data=item[0]))
        if not i % 2:
            kb.append(ls)
            ls = []
    if ls:
        kb.append(ls)
    if kb:
        m.reply(msg, quote=True, reply_markup=InlineKeyboardMarkup(kb))
    else:
        m.reply('**~ empty ~**')
    m.stop_propagation()
