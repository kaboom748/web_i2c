"""web_i2c: an in-browser I2C debug terminal. The page served by the ESP
exposes buttons and fields (address, register, read/write/scan); the ESP runs
the REAL I2C transactions on its SDA/SCL pins and returns the results
(ACK/NACK, bytes read) to the browser.

The browser never speaks I2C: it sends command text over WebSocket, the ESP
does the I2C. Built to unstick a stubborn I2C display or sensor without
reflashing on every try. Works on ESP8266 and ESP32.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_FREQUENCY, CONF_SDA, CONF_SCL, CONF_SCAN
from esphome.core import CORE
from esphome.components.socket import consume_sockets, SocketType

DEPENDENCIES = ["network", "i2c"]
AUTO_LOAD = ["socket"]

CONF_PORT = "port"

web_i2c_ns = cg.esphome_ns.namespace("web_i2c")
# WebI2C is also an i2c::I2CBus (a transparent decorator), so other components
# can point their i2c_id at it; it relays every transaction to the real bus.
WebI2C = web_i2c_ns.class_("WebI2C", cg.Component, i2c.I2CBus)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WebI2C),
            # the I2C bus to drive (defined by an i2c: block in the YAML)
            cv.GenerateID(i2c.CONF_I2C_ID): cv.use_id(i2c.I2CBus),
            # TCP port of the embedded WebSocket server
            cv.Required(CONF_PORT): cv.port,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    consume_sockets(1, "web_i2c", SocketType.TCP_LISTEN),
    consume_sockets(2, "web_i2c"),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_port(config[CONF_PORT]))
    # connect the I2C bus
    bus = await cg.get_variable(config[i2c.CONF_I2C_ID])
    cg.add(var.set_i2c_bus(bus))
    # surface the referenced bus's configuration (frequency + pins + pull-ups)
    # to the frontend. The runtime I2CBus pointer exposes no getters, so we read
    # the i2c: config at build time and pass the values as our own members.
    freq, sda, scl, scan, sdapu, sclpu = 0, -1, -1, True, -1, -1
    buses = CORE.config.get("i2c", [])
    if isinstance(buses, dict):
        buses = [buses]
    for bc in buses:
        if bc.get(CONF_ID) == config[i2c.CONF_I2C_ID]:
            freq = int(bc.get(CONF_FREQUENCY, 0))
            s, c = bc.get(CONF_SDA), bc.get(CONF_SCL)
            sda = s if isinstance(s, int) else -1
            scl = c if isinstance(c, int) else -1
            scan = bool(bc.get(CONF_SCAN, True))
            if "sda_pullup_enabled" in bc:
                sdapu = 1 if bc.get("sda_pullup_enabled") else 0
            if "scl_pullup_enabled" in bc:
                sclpu = 1 if bc.get("scl_pullup_enabled") else 0
            break
    cg.add(var.set_bus_info(freq, sda, scl, scan, sdapu, sclpu))
    # count how many components point their i2c_id at us (the tap). This is the
    # authoritative "attached devices" signal, exact even for a wired-but-silent
    # device. Standard i2c devices carry CONF_I2C_ID; walk the resolved config.
    def _count_tap(node, tap_id):
        n = 0
        if isinstance(node, dict):
            v = node.get(i2c.CONF_I2C_ID)
            if v is not None and v == tap_id:
                n += 1
            for val in node.values():
                n += _count_tap(val, tap_id)
        elif isinstance(node, (list, tuple)):
            for it in node:
                n += _count_tap(it, tap_id)
        return n

    tap_devices = _count_tap(CORE.config, config[CONF_ID])
    cg.add(var.set_tap_count(tap_devices))
