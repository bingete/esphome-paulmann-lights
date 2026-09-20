import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ICON,
    CONF_ID,
    ENTITY_CATEGORY_CONFIG,
)

from . import PaulmannLights, paulmann_lights_ns
from .const import (
    CONF_CONTROLLER_ENABLE,
    CONF_PAULMANN_LIGHTS_ID,
    CONF_TIMER,
    CONF_WORKING_MODE,
)

PaulmannControlNumber = paulmann_lights_ns.class_("PaulmannControlNumber", number.Number)
ControlType = paulmann_lights_ns.enum("ControlType")

CONTROL_SCHEMA = number.number_schema(
    PaulmannControlNumber,
    entity_category=ENTITY_CATEGORY_CONFIG,
    icon="mdi:tune",
).extend({cv.GenerateID(CONF_PAULMANN_LIGHTS_ID): cv.use_id(PaulmannLights)})

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TIMER): CONTROL_SCHEMA.extend(
            {
                cv.Optional(CONF_ICON, default="mdi:timer-outline"): cv.icon,
            }
        ),
        cv.Optional(CONF_WORKING_MODE): CONTROL_SCHEMA.extend(
            {
                cv.Optional(CONF_ICON, default="mdi:lightbulb-auto"): cv.icon,
            }
        ),
        cv.Optional(CONF_CONTROLLER_ENABLE): CONTROL_SCHEMA.extend(
            {
                cv.Optional(CONF_ICON, default="mdi:toggle-switch"): cv.icon,
            }
        ),
    }
)


async def _build_number(config, key, control_type, min_value, max_value, step, parent_setter):
    if not (conf := config.get(key)):
        return

    var = cg.new_Pvariable(conf[CONF_ID])
    await number.register_number(
        var,
        conf,
        min_value=min_value,
        max_value=max_value,
        step=step,
    )
    parent = await cg.get_variable(conf[CONF_PAULMANN_LIGHTS_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_control_type(control_type))
    cg.add(getattr(parent, parent_setter)(var))


async def to_code(config):
    await _build_number(
        config,
        CONF_TIMER,
        ControlType.CONTROL_TYPE_TIMER,
        0,
        255,
        1,
        "set_timer_number",
    )
    await _build_number(
        config,
        CONF_WORKING_MODE,
        ControlType.CONTROL_TYPE_WORKING_MODE,
        0,
        10,
        1,
        "set_working_mode_number",
    )
    await _build_number(
        config,
        CONF_CONTROLLER_ENABLE,
        ControlType.CONTROL_TYPE_CONTROLLER_ENABLE,
        0,
        1,
        1,
        "set_controller_enable_number",
    )
