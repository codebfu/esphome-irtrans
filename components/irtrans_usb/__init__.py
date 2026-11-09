import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, text_sensor, uart
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

DEPENDENCIES = ["uart", "api"]
AUTO_LOAD = ["json", "text_sensor", "binary_sensor"]
CODEOWNERS = ["@codebfu"]

CONF_LEARN_TIMEOUT = "learn_timeout"
CONF_STATUS = "status"
CONF_LAST_PAYLOAD = "last_payload"
CONF_CONNECTED = "connected"

irtrans_usb_ns = cg.esphome_ns.namespace("irtrans_usb")
IRTransUSB = irtrans_usb_ns.class_("IRTransUSB", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(IRTransUSB),
            cv.Optional(CONF_LEARN_TIMEOUT, default="10s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_LAST_PAYLOAD): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    # CustomAPIDevice is a mixin; ensure custom_services are enabled by the user YAML.
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_learn_timeout(config[CONF_LEARN_TIMEOUT]))

    if CONF_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_STATUS])
        cg.add(var.set_status_sensor(sens))

    if CONF_LAST_PAYLOAD in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LAST_PAYLOAD])
        cg.add(var.set_last_payload_sensor(sens))

    if CONF_CONNECTED in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_CONNECTED])
        cg.add(var.set_connected_sensor(sens))
