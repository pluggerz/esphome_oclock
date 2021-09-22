# depending on https://materialdesignicons.com/
from logging import Logger
import logging
from os import defpath
from typing import Optional
from esphome.components.font import (
    CONF_RAW_DATA_ID,
    CONF_RAW_GLYPH_ID
)
from esphome.core import CORE
import importlib
from esphome.cpp_generator import MockObj, Pvariable
from esphome.helpers import read_file
from esphome.voluptuous_schema import _Schema
from esphome import core
from pathlib import Path
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import loader
import sys
import json
from esphome.components import sensor, binary_sensor, switch, image, font, time
from esphome.const import (
    CONF_BINARY_SENSOR,
    CONF_DEVICE_CLASS,
    CONF_DISABLED_BY_DEFAULT,
    CONF_FILE,
    CONF_FORMAT,
    CONF_GLYPHS,
    CONF_ID,
    CONF_NAME,
    CONF_RANDOM,
    CONF_SENSOR,
    CONF_SIZE,
    CONF_SOURCE,
    CONF_TIME,
    CONF_TYPE,
    CONF_VISIBLE,
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_TEMPERATURE
)

_LOGGER = logging.getLogger(__name__)


# TODO: should add auto mapping in C code via global ?
MDI_DEVICE_CLASS_BELL_SILENT = "bell"
MDI_DEVICE_CLASS_BELL_RING = "bell-ring"
MDI_DEVICE_CLASS_TEMPERATURE = "thermometer"
MDI_DEVICE_CLASS_MOTION_WALK = "walk"
MDI_DEVICE_CLASS_MOTION_RUN = "run"

CONF_GLYPH = 'glyph'
CONF_GLYPH_ON = 'glyph_on'
CONF_GLYPH_OFF = 'glyph_off'
CONF_SWITCH = 'switch'
CONF_STICKY = 'sticky'
CONF_WIDGETS = 'widgets'
CONF_GROUPS = 'groups'
CONF_ANALOG_TIME = 'analog-time'
CONF_DIGITAL_TIME = 'digital-time'

DEPENDENCIES = ["display"]
CODEOWNERS = ["@hvandenesker"]

# ADAPT FOR CODE
b_matrix_glyph_ns = cg.esphome_ns.namespace("matrix_glyphs")

Controller = b_matrix_glyph_ns.class_("Controller")
Group = b_matrix_glyph_ns.class_("Group")
SensorWidget = b_matrix_glyph_ns.class_("SensorWidget")
BinarySensorWidget = b_matrix_glyph_ns.class_("BinarySensorWidget")
DigitalTimeWidget = b_matrix_glyph_ns.class_("DigitalTimeWidget")
AnalogTimeWidget = b_matrix_glyph_ns.class_("AnalogTimeWidget")

GLYPH_ICON_SCHEMA = _Schema({
    cv.Required('id'): cv.use_id(image.Image_),
})


def find_file(file: str) -> Path:
    for p in sys.meta_path:
        print(f"possible meta_path: {p}")
        if (isinstance(p, loader.ComponentMetaFinder)):
            for f in p._finders:
                if (isinstance(f, importlib.machinery.FileFinder)):
                    print(f"possible finder: {f}")
                    path = Path(str(f.path)) / str(file)
                    print(f"path: {path}")
                    if (Path(path).exists()):
                        return path
    raise cv.Invalid(f"Not found: {file}")


MDI_SELECTED_GLYPHS_NAMES = []
MDI_META_FILE = find_file("mdi.meta.json")

with open(MDI_META_FILE) as metaJson:
    MDI_MAP = dict((i['name'], chr(int(i['codepoint'], 16)))
                   for i in json.load(metaJson))
MDI_KNOWN_GLYPHS_NAMES = MDI_MAP.keys()


def import_icon(icon):
    if icon not in MDI_SELECTED_GLYPHS_NAMES:
        MDI_SELECTED_GLYPHS_NAMES.append(icon)


def mdiIcon(value):
    """Validate that a given config value is a valid icon."""
    # validate_icon(value)
    if (isinstance(value, list)):
        for icon in value:
            mdiIcon(icon)
        return value
    else:
        if not value:
            return value
        if value == "*":
            return value
        if value in MDI_KNOWN_GLYPHS_NAMES:
            import_icon(value)
            return value
        raise cv.Invalid(f'Name should be in: {MDI_KNOWN_GLYPHS_NAMES}')


GLYPH_MDI_SCHEMA = _Schema({
    cv.Required('id'): mdiIcon,
})

GLYPH_TYPE_SCHEMA = cv.Any(
    cv.typed_schema(
        {
            'empty': {},
            'image': cv.Schema(GLYPH_ICON_SCHEMA),
            'mdi':  cv.Schema(GLYPH_MDI_SCHEMA)
        }
    ),
)

WIDGET_SENSOR_SCHEMA = _Schema({
    cv.GenerateID(CONF_RANDOM): cv.declare_id(SensorWidget),
    cv.Required(CONF_ID): cv.use_id(sensor.Sensor)
})

WIDGET_BINARY_SENSOR_SCHEMA = _Schema({
    cv.GenerateID(CONF_RANDOM): cv.declare_id(BinarySensorWidget),
    cv.Required(CONF_ID): cv.use_id(binary_sensor.BinarySensor),
    cv.Optional(CONF_GLYPH_ON): GLYPH_TYPE_SCHEMA,
    cv.Optional(CONF_GLYPH_OFF): GLYPH_TYPE_SCHEMA,
    cv.Optional(CONF_STICKY): cv.boolean,
})

WIDGET_DIGITAL_TIME_SCHEMA = _Schema({
    cv.GenerateID(CONF_RANDOM): cv.declare_id(DigitalTimeWidget),
    cv.Required(CONF_ID): cv.use_id(time.RealTimeClock),
    cv.Optional(CONF_FORMAT, default="%H:%M"): cv.string
})

WIDGET_ANALOG_TIME_SCHEMA = _Schema({
    cv.GenerateID(CONF_RANDOM): cv.declare_id(AnalogTimeWidget),
    cv.Required(CONF_ID): cv.use_id(time.RealTimeClock)
})

WIDGET_TYPE_SCHEMA = cv.Any(
    cv.typed_schema(
        {
            CONF_BINARY_SENSOR: cv.Schema(WIDGET_BINARY_SENSOR_SCHEMA),
            CONF_SENSOR: cv.Schema(WIDGET_SENSOR_SCHEMA),
            CONF_ANALOG_TIME: cv.Schema(WIDGET_ANALOG_TIME_SCHEMA),
            CONF_DIGITAL_TIME: cv.Schema(WIDGET_DIGITAL_TIME_SCHEMA),
        }
    ),
)

WIDGET_SCHEMA = _Schema({
    cv.Required(CONF_SOURCE): WIDGET_TYPE_SCHEMA,
})

GROUP_SCHEMA = _Schema({
    cv.Required(CONF_ID): cv.declare_id(Group),
    cv.Optional(CONF_VISIBLE, default=True): cv.boolean,
    cv.Required(CONF_GLYPH): GLYPH_TYPE_SCHEMA,
    cv.Optional(CONF_WIDGETS, default=[]): cv.ensure_list(WIDGET_SCHEMA),
})


CONFIG_SCHEMA = _Schema(
    {
        cv.Required(CONF_GROUPS): cv.ensure_list(GROUP_SCHEMA),
    }
)


def _validate_sensor(type, id):
    _LOGGER.debug(f"Should _validate_sensor: {id}({type})")


async def _create_glyph_image(config: dict):
    return await cg.get_variable(config[CONF_ID])


async def _create_glyph_mdi(config: dict):
    rawIcon = config[CONF_ID]
    if (rawIcon == "*"):
        rawIcon = MDI_SELECTED_GLYPHS_NAMES
    if (isinstance(rawIcon, list)):
        for icon in rawIcon:
            import_icon(icon)

        icons = ", ".join(f'"{i}"' for i in rawIcon)
        return MockObj(
            "std::make_shared<esphome::matrix_glyphs::AnimationGlyph>(esphome::matrix_glyphs::AnimationGlyph({"+icons+"}))")
    else:
        import_icon(rawIcon)
        return MockObj(
            f"std::make_shared<esphome::matrix_glyphs::MdiGlyph>(\"{rawIcon}\")")


async def _create_glyph(config: dict):
    type = config[CONF_TYPE]

    if (type == 'image'):
        return await _create_glyph_image(config)
    elif (type == 'mdi'):
        return await _create_glyph_mdi(config)
    else:
        raise cv.Invalid(f"Not yet supported: {type}")


async def _set_glyph(var: Pvariable, config: dict):
    cg.add(var.set_image(await _create_glyph(config)))


async def _process_digital_time(idx: id, groupVar: Pvariable, config: dict):
    widgetVar = cg.new_Pvariable(config[CONF_RANDOM])
    cg.add(widgetVar.set_source(await cg.get_variable(config[CONF_ID])))
    cg.add(widgetVar.set_format(config[CONF_FORMAT]))
    cg.add(groupVar.add(widgetVar))

async def _process_analog_time(idx: id, groupVar: Pvariable, config: dict):
    widgetVar = cg.new_Pvariable(config[CONF_RANDOM])
    cg.add(widgetVar.set_source(await cg.get_variable(config[CONF_ID])))
    cg.add(groupVar.add(widgetVar))


async def _process_binary_sensor(idx: int, groupVar: Pvariable, config: dict):
    widgetVar = cg.new_Pvariable(config[CONF_RANDOM])

    cg.add(widgetVar.set_sensor(await cg.get_variable(config[CONF_ID])))
    cg.add(groupVar.add(widgetVar))

    if (CONF_GLYPH_OFF in config):
        cg.add(widgetVar.set_off_glyph(await _create_glyph(config[CONF_GLYPH_OFF])))
    if (CONF_GLYPH_ON in config):
        cg.add(widgetVar.set_on_glyph(await _create_glyph(config[CONF_GLYPH_ON])))

    binarySwitchIdName = str(config[CONF_ID]) + "_sensor"
    binarySwitchId = core.ID(
        binarySwitchIdName, is_declaration=False, type=binary_sensor.BinarySensor)

    switchIdName = str(config[CONF_ID]) + "_sticky_switch"
    if (CORE.has_id(binarySwitchId)):
        _LOGGER.warn(
            "Id not unique for %s will use index %s to make unique", binarySwitchIdName, idx)
        binarySwitchIdName = f"{str(config[CONF_ID])}_sensor{idx}"
        switchIdName = f"{str(config[CONF_ID])}_sticky_switch{idx}"
        binarySwitchId = core.ID(
            binarySwitchIdName, is_declaration=False, type=binary_sensor.BinarySensor)

    # copy device class if specified
    device_class = ""
    for sensor in CORE.config[CONF_BINARY_SENSOR]:
        if (str(sensor[CONF_ID]) == str(config[CONF_ID])):
            device_class = sensor[CONF_DEVICE_CLASS]

    binarySensorConf = {
        CONF_ID: binarySwitchId,
        CONF_NAME: str(binarySwitchId),
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_DEVICE_CLASS: device_class,
    }
    binarySensorVar: Pvariable = cg.Pvariable(
        binarySensorConf[CONF_ID], widgetVar.get_alert_sensor())
    await binary_sensor.register_binary_sensor(binarySensorVar, binarySensorConf)

    # create switch
    switchConf = {
        CONF_ID: core.ID(switchIdName, is_declaration=False, type=switch.Switch),
        CONF_NAME: switchIdName,
        CONF_DISABLED_BY_DEFAULT: False,
    }
    stickySwitchVar: Pvariable = cg.Pvariable(
        switchConf[CONF_ID], widgetVar.get_sticky_switch())
    if (CONF_STICKY in config):
        cg.add(stickySwitchVar.publish_state(config[CONF_STICKY]))
    await switch.register_switch(stickySwitchVar, switchConf)


async def _process_sensor(groupVar: Pvariable, config: dict):
    widgetVar = cg.new_Pvariable(config[CONF_RANDOM])

    cg.add(widgetVar.set_sensor(await cg.get_variable(config[CONF_ID])))
    cg.add(groupVar.add(widgetVar))


async def _process_source(idx: id, groupVar: Pvariable, config: dict):
    type = config[CONF_TYPE]
    if (type == CONF_DIGITAL_TIME):
        await _process_digital_time(idx, groupVar, config)
    elif (type == CONF_ANALOG_TIME):
        await _process_analog_time(idx, groupVar, config)
    elif (type == CONF_BINARY_SENSOR):
        _validate_sensor(type, config[CONF_ID])
        await _process_binary_sensor(idx, groupVar, config)
    elif (type == CONF_SENSOR):
        _validate_sensor(type, config[CONF_ID])
        await _process_sensor(groupVar, config)
    else:
        raise


async def _process_widget(idx: int, groupVar: Pvariable, config: dict):
    await _process_source(idx, groupVar, config[CONF_SOURCE])


async def _process_group(controllerVar: Pvariable, config: dict):
    if (config[CONF_VISIBLE] == False):
        return

    id = config[CONF_ID]
    var = cg.new_Pvariable(id)

    cg.add(controllerVar.add(var))

    await _set_glyph(var, config[CONF_GLYPH])

    widgets: dict = config[CONF_WIDGETS]
    for idx, c in enumerate(widgets):
        await _process_widget(idx, var, c)


def extract_icons(conf):
    if (isinstance(conf, list)):
        for item in conf:
            extract_icons(item)
    else:
        if (CONF_DEVICE_CLASS in conf):
            device_class = conf[CONF_DEVICE_CLASS]
            if (device_class == DEVICE_CLASS_TEMPERATURE):
                import_icon(MDI_DEVICE_CLASS_TEMPERATURE)
            if (device_class == DEVICE_CLASS_MOTION):
                import_icon(MDI_DEVICE_CLASS_MOTION_WALK)
                import_icon(MDI_DEVICE_CLASS_MOTION_RUN)


async def to_code(config):
    # lets gather all icons
    if CONF_BINARY_SENSOR in CORE.config:
        extract_icons(CORE.config[CONF_BINARY_SENSOR])
    if CONF_SENSOR in CORE.config:
        extract_icons(CORE.config[CONF_SENSOR])
        
    # check if we are complete
    for icon in MDI_SELECTED_GLYPHS_NAMES:
        if icon not in MDI_MAP:
            raise cv.Invalid(f"Icon not mapped: {icon}")

    print(f"MDI_SELECTED_GLYPHS_NAMES={MDI_SELECTED_GLYPHS_NAMES}")
    MDI_SELECTED_GLYPHS = set(MDI_MAP[name]
                              for name in MDI_SELECTED_GLYPHS_NAMES)
    print(f"MDI_SELECTED_GLYPHS={MDI_SELECTED_GLYPHS}")

    # lets introduce the font
    mdi_ttf = find_file("mdi.ttf")
    print(f"ttf: {mdi_ttf}")
    base_name = "matrix_glyphs"
    fontId = base_name+"_font"
    fontConfig = {
        CONF_ID: fontId,
        CONF_FILE: str(mdi_ttf),
        CONF_SIZE: 8,
        CONF_GLYPHS: MDI_SELECTED_GLYPHS,
        CONF_RAW_DATA_ID: core.ID(base_name+"_prog_arr", is_declaration=True, type=cg.uint8),
        CONF_RAW_GLYPH_ID: core.ID(
            base_name+"_data", is_declaration=True, type=font.GlyphData)
    }
    # delegete the work
    validated = font.CONFIG_SCHEMA(fontConfig)
    await font.to_code(validated)

    var = MockObj('esphome::matrix_glyphs::controller')

    cg.add(var.set_mdi_font(MockObj(fontId)))
    for glyph in sorted(MDI_SELECTED_GLYPHS_NAMES):
        _LOGGER.info("Adding glyph with name '%s'", glyph)
        cg.add(var.add_mdi_code(glyph, MDI_MAP[glyph]))

    groups: dict = config[CONF_GROUPS]
    for i, c in enumerate(groups):
        await _process_group(var, c)


    
