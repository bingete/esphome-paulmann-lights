import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_OUTPUT_ID

from . import PaulmannLights, paulmann_lights_ns
from .const import CONF_PAULMANN_LIGHTS_ID

PaulmannLightOutput = paulmann_lights_ns.class_("PaulmannLightOutput", light.LightOutput)

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(PaulmannLightOutput),
        cv.GenerateID(CONF_PAULMANN_LIGHTS_ID): cv.use_id(PaulmannLights),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    parent = await cg.get_variable(config[CONF_PAULMANN_LIGHTS_ID])
    cg.add(var.set_parent(parent))
    cg.add(parent.set_light_output(var))
