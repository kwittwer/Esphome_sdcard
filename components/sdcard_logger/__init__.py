import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import time
from esphome.const import CONF_ID

AUTO_LOAD = ["time"]

CONF_CS_PIN = "cs_pin"
CONF_TIME_ID = "time_id"
CONF_BASE_DIR = "base_dir"

sdcard_logger_ns = cg.esphome_ns.namespace("sdcard_logger")
SDCardLogger = sdcard_logger_ns.class_("SDCardLogger", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SDCardLogger),
        cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Optional(CONF_BASE_DIR, default="/"): cv.string_strict,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cs_pin = await cg.gpio_pin_expression(config[CONF_CS_PIN])
    cg.add(var.set_cs_pin(cs_pin))

    time_var = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time_source(time_var))

    cg.add(var.set_base_dir(config[CONF_BASE_DIR]))
