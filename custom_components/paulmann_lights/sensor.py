import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from . import PaulmannLights
from .const import (
    CONF_FIRMWARE_REVISION,
    CONF_HARDWARE_REVISION,
    CONF_IEEE_CERTIFICATION,
    CONF_MANUFACTURER,
    CONF_MODEL,
    CONF_PAULMANN_LIGHTS_ID,
    CONF_PNP_ID,
    CONF_SERIAL_NUMBER,
    CONF_SOFTWARE_REVISION,
    CONF_SYSTEM_ID,
)

SENSOR_SCHEMA = text_sensor.text_sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC
).extend({cv.GenerateID(CONF_PAULMANN_LIGHTS_ID): cv.use_id(PaulmannLights)})

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SYSTEM_ID): SENSOR_SCHEMA,
        cv.Optional(CONF_MODEL): SENSOR_SCHEMA,
        cv.Optional(CONF_SERIAL_NUMBER): SENSOR_SCHEMA,
        cv.Optional(CONF_FIRMWARE_REVISION): SENSOR_SCHEMA,
        cv.Optional(CONF_HARDWARE_REVISION): SENSOR_SCHEMA,
        cv.Optional(CONF_SOFTWARE_REVISION): SENSOR_SCHEMA,
        cv.Optional(CONF_MANUFACTURER): SENSOR_SCHEMA,
        cv.Optional(CONF_IEEE_CERTIFICATION): SENSOR_SCHEMA,
        cv.Optional(CONF_PNP_ID): SENSOR_SCHEMA,
    }
)


async def _register_sensor(config, key, setter_name):
    if not (conf := config.get(key)):
        return

    sens = await text_sensor.new_text_sensor(conf)
    parent = await cg.get_variable(conf[CONF_PAULMANN_LIGHTS_ID])
    cg.add(getattr(parent, setter_name)(sens))


async def to_code(config):
    await _register_sensor(config, CONF_SYSTEM_ID, "set_system_id_sensor")
    await _register_sensor(config, CONF_MODEL, "set_model_sensor")
    await _register_sensor(config, CONF_SERIAL_NUMBER, "set_serial_number_sensor")
    await _register_sensor(config, CONF_FIRMWARE_REVISION, "set_firmware_revision_sensor")
    await _register_sensor(config, CONF_HARDWARE_REVISION, "set_hardware_revision_sensor")
    await _register_sensor(config, CONF_SOFTWARE_REVISION, "set_software_revision_sensor")
    await _register_sensor(config, CONF_MANUFACTURER, "set_manufacturer_sensor")
    await _register_sensor(config, CONF_IEEE_CERTIFICATION, "set_ieee_certification_sensor")
    await _register_sensor(config, CONF_PNP_ID, "set_pnp_id_sensor")
