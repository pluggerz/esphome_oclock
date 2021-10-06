# depending on https://materialdesignicons.com/
from esphome.const import CONF_BAUD_RATE, CONF_DEVICE_CLASS, CONF_DISABLED_BY_DEFAULT, CONF_ID, CONF_LOGGER, CONF_NAME
from esphome.core import CORE
from esphome import core
from esphome.voluptuous_schema import _Schema
from esphome.components import time, switch
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.cpp_generator import MockObj, Pvariable
CONF_TIME_ID = "time_id"


DEPENDENCIES = ["time", "switch"]
CODEOWNERS = ["@hvandenesker"]


oclock_ns = cg.esphome_ns.namespace("oclock_stub")

OClockStubController = oclock_ns.class_("OClockStubController", cg.Component)

CONFIG_SCHEMA = _Schema(
    {
        cv.GenerateID(): cv.declare_id(OClockStubController),
        cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock)
    }
)


async def to_code(config):
    # https://github.com/esphome/AsyncTCP/blob/master/library.json
    cg.add_library(name="apa102-arduino", version=None,
                   repository="pluggerz/apa102-arduino.git")

    if CONF_LOGGER in CORE.config:
        baudrate = CORE.config[CONF_LOGGER][CONF_BAUD_RATE]
        if (baudrate != 0):
            raise cv.Invalid(f"Make sure logger.baudrate = 0 !")

    var = cg.new_Pvariable(config[CONF_ID])
    if CONF_TIME_ID in config:
        cg.add(var.set_time(await cg.get_variable(config[CONF_TIME_ID])))

    resetSwitchId = core.ID(
        "oclock_reset", is_declaration=False, type=switch.Switch)

    resetSwitchConf = {
        CONF_ID: resetSwitchId,
        CONF_NAME: str(resetSwitchId),
        CONF_DISABLED_BY_DEFAULT: False,
        # CONF_DEVICE_CLASS: device_class,
    }
    resetSensorVar: Pvariable = cg.Pvariable(
        resetSwitchConf[CONF_ID], cg.RawExpression("new oclock::ResetSwitch()"))
    await switch.register_switch(resetSensorVar, resetSwitchConf)

    await cg.register_component(var, config)
