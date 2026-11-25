import esphome.codegen as cg
from esphome.components import climate_ir, switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_INTERNAL,
    CONF_NAME,
    CONF_RESTORE_MODE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_BUG,
)
from esphome.cpp_helpers import register_parented

AUTO_LOAD = ["climate", "climate_ir", "switch"]
DEPENDENCIES = ["remote_transmitter"]
CODEOWNERS = ["@slashrsm"]

ns = cg.esphome_ns.namespace("mitsubishi_bh00")
MitsubishiBh00Climate = ns.class_("MitsubishiBh00Climate", climate_ir.ClimateIR)
MitsubishiBh00DebugSwitch = ns.class_(
    "MitsubishiBh00DebugSwitch", switch.Switch, cg.Parented.template(MitsubishiBh00Climate)
)

CONF_DEBUG = "ir_debug"


def _as_entity(value, placeholder):
    value = {} if not isinstance(value, dict) else dict(value)
    value.setdefault(CONF_NAME, placeholder)
    return value


def _validate_debug(value):
    value = _as_entity(value, "Mitsubishi BH00 IR debug")
    value.setdefault(CONF_RESTORE_MODE, "RESTORE_DEFAULT_OFF")
    return switch.switch_schema(
        MitsubishiBh00DebugSwitch,
        icon=ICON_BUG,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )(value)


def _name_debug(config):
    config[CONF_DEBUG][CONF_NAME] = f"{config[CONF_NAME]} IR debug"
    config[CONF_DEBUG][CONF_INTERNAL] = False
    return config


CONFIG_SCHEMA = cv.All(
    climate_ir.climate_ir_with_receiver_schema(MitsubishiBh00Climate).extend(
        {
            cv.Optional(CONF_DEBUG, default={}): _validate_debug,
        }
    ),
    _name_debug,
)


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    debug = await switch.new_switch(config[CONF_DEBUG])
    await register_parented(debug, var)
    cg.add(var.set_debug_switch(debug))
