# depending on https://materialdesignicons.com/
from logging import Logger
import logging
from os import defpath
from typing import Optional
from esphome.components.font import (
    CONF_RAW_DATA_ID,
    CONF_RAW_GLYPH_ID
)
from esphome.const import CONF_BAUD_RATE, CONF_ID, CONF_LOGGER
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

oclock_ns = cg.esphome_ns.namespace("oclock_stub")

OClockStubController = oclock_ns.class_("OClockStubController", cg.Component)


_LOGGER = logging.getLogger(__name__)


CONFIG_SCHEMA = _Schema(
    {
        cv.GenerateID(): cv.declare_id(OClockStubController),
    }
)



DEPENDENCIES = []
CODEOWNERS = ["@hvandenesker"]


async def to_code(config):
    if CONF_LOGGER in CORE.config:
        baudrate=CORE.config[CONF_LOGGER][CONF_BAUD_RATE]
        if (baudrate != 0):
            raise cv.Invalid(f"Make sure logger.baudrate = 0 !")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
