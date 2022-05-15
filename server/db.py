from pony.orm import *
from datetime import datetime, timedelta

DB = Database()


class Device(DB.Entity):
    """
    Bluetooth device database entity.
    """
    bluetooth_id = PrimaryKey(str)
    name = Required(str)
    registered_time = Required(datetime, 6, sql_default='CURRENT_TIMESTAMP')
    until = Optional(datetime, 6)

    @property
    def timeout(self):
        """
        return if the device reached to its timeout.
        :return: True if the timeout is reached else False.
        """
        if not self.until:
            return False
        return datetime.now() >= self.until


def get_bt_device(bt_id):
    """
    getter to get a device from the DB.
    :param bt_id: the device bluetooth_id.
    :return: the device if is exists else None.
    """
    with db_session:
        device = Device.get(bluetooth_id=bt_id)
        if device and not device.timeout:
            return device
        elif device:
            device.delete()


def add_device(bt_id, name):
    """
    add or update a device on the DB.
    :param bt_id: the device bluetooth_id.
    :param name: a name for this device.
    """
    with db_session:
        d = Device.get(bluetooth_id=bt_id)
        if d:
            d.name = name
        else:
            Device(bluetooth_id=bt_id, name=name)


@db_session
def update_until_time(days, bt_id):
    """
    update the timeout for a device.
    :param days: num of days to add.
    :param bt_id: the device bluetooth_id.
    """
    d = Device.get(bluetooth_id=bt_id)
    if not d:
        return
    if days:
        d.until = (d.until or datetime.now()) + timedelta(days=days)
    else:
        d.until = None


@db_session
def list_devices():
    """
    :return: list of all the devices in the DB.
    """
    return {d.bluetooth_id: (d.name, d.until or '') for d in Device.select() if not d.timeout}


def start():
    """
    starts the database connection.
    call this function only once before running the program.
    """
    DB.bind(provider='sqlite', filename='Devices_DB.sqlite', create_db=True)
    DB.generate_mapping(create_tables=True)
