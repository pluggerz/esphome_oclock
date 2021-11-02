from esphome.const import CONF_BAUD_RATE, CONF_BRIGHTNESS, CONF_DEVICE_CLASS, CONF_DISABLED_BY_DEFAULT, CONF_ID, CONF_INITIAL_VALUE, CONF_LOGGER, CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_NAME, CONF_RANDOM, CONF_STEP, CONF_TYPE
from esphome.core import CORE
from esphome import core
from esphome.voluptuous_schema import _Schema
from esphome.components import time, switch, number
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.cpp_generator import MockObj, Pvariable

CONF_TIME_ID = "time_id"
CONF_COUNT_START = "count_start"
CONF_SLAVES = "slaves"
CONF_HANDLE0 = "H0"
CONF_HANDLE1 = "H1"
CONF_ANIMATION_SLAVE_ID = "animation_slave_id"
CONF_HOUR = "hour"
CONF_MINUTE = "minute"


AUTO_LOAD = [
    "time",
    "number",
    "switch",
]

DEPENDENCIES = ["time", "switch"]
CODEOWNERS = ["@hvandenesker"]


oclock_ns = cg.esphome_ns.namespace("oclock")

NumberControl = oclock_ns.class_("NumberControl", number.Number, cg.Component)

OClockStubController = oclock_ns.class_("OClockStubController", cg.Component)


def allUnique(x):
    seen = set()
    return not any(i in seen or seen.add(i) for i in x)


def cv_slave_id_check(id):
    assert id.startswith("S")
    ret = int(id[1:])
    print("cv_slave_id_check:", ret)
    return ret


def cv_slave_conf_check(id, defSlaveConf):
    d = dict()
    if CONF_HANDLE0 in defSlaveConf:
        d[CONF_HANDLE0] = defSlaveConf[CONF_HANDLE0]
    else:
        d[CONF_HANDLE0] = 0

    if CONF_HANDLE1 in defSlaveConf:
        d[CONF_HANDLE1] = defSlaveConf[CONF_HANDLE1]
    else:
        d[CONF_HANDLE1] = 0

    d[CONF_ANIMATION_SLAVE_ID] = int(id)
    return d


def cv_slaves_check(conf):
    d = dict()
    defSlaveConf = conf["*"]
    print("defSlaveConf", defSlaveConf)
    for slaveId, slaveConf in conf.items():
        if (slaveId != "*"):
            d[cv_slave_id_check(slaveId)] = cv_slave_conf_check(
                slaveConf, defSlaveConf)
    return d


HOUR_SCHEMA = number.NUMBER_SCHEMA.extend(cv.COMPONENT_SCHEMA).extend(
    {
        cv.GenerateID(): cv.declare_id(NumberControl),
        cv.Optional(CONF_NAME, default="hour"): cv.string_strict,
        cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
        cv.Optional(CONF_MAX_VALUE, default=23): cv.float_,
        cv.Optional(CONF_STEP, default=1): cv.float_,
        cv.Optional(CONF_INITIAL_VALUE, default=0): cv.float_,
    })


MINUTE_SCHEMA = number.NUMBER_SCHEMA.extend(cv.COMPONENT_SCHEMA).extend(
    {
        cv.GenerateID(): cv.declare_id(NumberControl),
        cv.Optional(CONF_NAME, default="minute"): cv.string_strict,
        cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
        cv.Optional(CONF_MAX_VALUE, default=31): cv.float_,
        cv.Optional(CONF_STEP, default=1): cv.float_,
        cv.Optional(CONF_INITIAL_VALUE, default=0): cv.float_,
    })


BRIGHTNESS_SCHEMA = number.NUMBER_SCHEMA.extend(cv.COMPONENT_SCHEMA).extend(
    {
        cv.GenerateID(): cv.declare_id(NumberControl),
        cv.Optional(CONF_NAME, default="brightness"): cv.string_strict,
        cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
        cv.Optional(CONF_MAX_VALUE, default=31): cv.float_,
        cv.Optional(CONF_STEP, default=1): cv.float_,
        cv.Optional(CONF_INITIAL_VALUE, default=16): cv.float_,
    })

CONFIG_SCHEMA = _Schema(
    {
        cv.GenerateID(): cv.declare_id(OClockStubController),
        cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Optional(CONF_COUNT_START, -1): cv.int_range(min=-1, max=64),
        cv.Required(CONF_SLAVES): cv_slaves_check,
        cv.Optional(CONF_BRIGHTNESS, default={}): BRIGHTNESS_SCHEMA,
        cv.Optional(CONF_HOUR, default={}): HOUR_SCHEMA,
        cv.Optional(CONF_MINUTE, default={}): MINUTE_SCHEMA
    }
)


async def cg_add_switch(id, expression):
    switchId = core.ID(id, is_declaration=False, type=switch.Switch)

    switchConf = {
        CONF_ID: switchId,
        CONF_NAME: str(switchId),
        CONF_DISABLED_BY_DEFAULT: False,
    }
    var: Pvariable = cg.Pvariable(
        switchConf[CONF_ID], expression)
    await switch.register_switch(var, switchConf)


def to_code_slave(physicalSlaveId, slaveConf):
    print(f"S{physicalSlaveId} maps to: ${slaveConf}")
    cg.add(cg.RawExpression(
        f"oclock::master.slave({physicalSlaveId}).animator_id = {slaveConf[CONF_ANIMATION_SLAVE_ID]}"))
    cg.add(cg.RawExpression(
        f"oclock::master.slave({physicalSlaveId}).handles[0].magnet_offset = {slaveConf[CONF_HANDLE0]}"))
    cg.add(cg.RawExpression(
        f"oclock::master.slave({physicalSlaveId}).handles[1].magnet_offset = {slaveConf[CONF_HANDLE1]}"))


async def add_number(conf):
    var = cg.new_Pvariable(conf[CONF_ID])
    await cg.register_component(var, conf)
    await number.register_number(
        var,
        conf,
        min_value=conf[CONF_MIN_VALUE],
        max_value=conf[CONF_MAX_VALUE],
        step=conf[CONF_STEP],
    )
    cg.add(var.publish_state(conf[CONF_INITIAL_VALUE]))
    return var


async def to_code(config):
    brightnessVar = await add_number(config[CONF_BRIGHTNESS])
    cg.add(brightnessVar.set_listener(cg.RawExpression("[](int value){ oclock::queue(new oclock::requests::SetBrightnessRequest(value));}")))
    
    hourVar = await add_number(config[CONF_HOUR])
    cg.add(hourVar.set_listener(cg.RawExpression(
        "[](int value){oclock::time_tracker::testTimeTracker.set_hour(value); }")
    ))

    minuteVar = await add_number(config[CONF_MINUTE])
    cg.add(minuteVar.set_listener(
        cg.RawExpression(
            "[](int value){oclock::time_tracker::testTimeTracker.set_minute(value); }")
    ))

    # https://github.com/esphome/AsyncTCP/blob/master/library.json
    cg.add_library(name="apa102-arduino", version=None,
                   repository="pluggerz/apa102-arduino.git")

    if CONF_LOGGER in CORE.config:
        baudrate = CORE.config[CONF_LOGGER][CONF_BAUD_RATE]
        if (baudrate != 0):
            raise cv.Invalid(f"Make sure logger.baudrate = 0 !")

    var = cg.new_Pvariable(config[CONF_ID], config[CONF_COUNT_START])
    if CONF_TIME_ID in config:
        cg.add(var.set_time(await cg.get_variable(config[CONF_TIME_ID])))

    for slaveId, slaveConf in config[CONF_SLAVES].items():
        to_code_slave(slaveId, slaveConf)

    await cg_add_switch("oclock_speed_check032", cg.RawExpression("new oclock::SpeedCheckSwitch32()"))
    await cg_add_switch("oclock_speed_check064", cg.RawExpression("new oclock::SpeedCheckSwitch64()"))
    await cg_add_switch("oclock_speed_adapt", cg.RawExpression("new oclock::SpeedAdaptTestSwitch()"))
    
    await cg_add_switch("oclock_track_test_time", cg.RawExpression("new oclock::TrackTestTime()"))
    await cg_add_switch("oclock_track_hassio_time", cg.RawExpression("new oclock::TrackHassioTime()"))
    await cg_add_switch("oclock_track_internal_time", cg.RawExpression("new oclock::TrackInternalTime()"))
    
    await cg_add_switch("oclock_reset", cg.RawExpression("new oclock::ResetSwitch()"))
    await cg_add_switch("oclock_request_positions", cg.RawExpression("new oclock::RequestPositions()"))
    await cg_add_switch("oclock_switch_led_background", cg.RawExpression("new oclock::LedModeConfigSwitch()"))
    await cg_add_switch("oclock_dump_logs", cg.RawExpression("new oclock::DumpLogsSwitch()"))
    await cg_add_switch("oclock_dump_config", cg.RawExpression("new oclock::DumpConfigSwitch()"))
    await cg_add_switch("oclock_dump_config_slaves", cg.RawExpression("new oclock::DumpConfigSlavesSwitch()"))

    await cg.register_component(var, config)
