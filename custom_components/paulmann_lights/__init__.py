import esphome.codegen as cg
from esphome.components import ble_client, esp32_ble, esp32_ble_tracker
from esphome.components.esp32_ble import BTLoggers
import esphome.config_validation as cv

from .const import (
    CONF_CONNECTION_RETRIES,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_NAME,
    CONF_PASSWORD,
    CONF_UPDATE_INTERVAL,
    DEFAULT_CONNECTION_RETRIES,
    DEFAULT_NAME,
    DEFAULT_PASSWORD,
    DEFAULT_UPDATE_INTERVAL,
)

AUTO_LOAD = ["light", "number", "text_sensor"]
CODEOWNERS = ["@bingete"]
DEPENDENCIES = ["esp32_ble_tracker"]
MULTI_CONF = True

paulmann_lights_ns = cg.esphome_ns.namespace("paulmann_lights")
PaulmannLights = paulmann_lights_ns.class_("PaulmannLights", ble_client.BLEClient)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PaulmannLights),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_NAME, default=DEFAULT_NAME): cv.string,
            cv.Optional(CONF_PASSWORD, default=DEFAULT_PASSWORD): cv.string_strict,
            cv.Optional(
                CONF_CONNECTION_RETRIES, default=DEFAULT_CONNECTION_RETRIES
            ): cv.int_range(min=1, max=10),
            cv.Optional(CONF_UPDATE_INTERVAL, default=DEFAULT_UPDATE_INTERVAL): cv.update_interval,
        }
    ).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA),
    esp32_ble.consume_connection_slots(1, "paulmann_lights"),
)


async def to_code(config):
    esp32_ble.register_bt_logger(BTLoggers.GATT)
    cg.add_define("USE_ESP32_BLE_UUID")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_client(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_name(config[CONF_NAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_connection_retries(config[CONF_CONNECTION_RETRIES]))
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_auto_connect(True))
