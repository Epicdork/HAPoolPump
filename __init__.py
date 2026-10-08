import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import binary_sensor, button, sensor, uart
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_POWER,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_REVOLUTIONS_PER_MINUTE,
    UNIT_SECOND,
    UNIT_WATT,
)

DEPENDENCIES = ["uart"]

jvs165s_ns = cg.esphome_ns.namespace("jvs165s")

JVS165SComponent = jvs165s_ns.class_(
    "JVS165SComponent", cg.Component, uart.UARTDevice
)

JVS165SStopTestButton = jvs165s_ns.class_(
    "JVS165SStopTestButton", button.Button
)
JVS165SResumeTestButton = jvs165s_ns.class_(
    "JVS165SResumeTestButton", button.Button
)
JVS165SSpeed1500Button = jvs165s_ns.class_(
    "JVS165SSpeed1500Button", button.Button
)
JVS165SSpeed2700Button = jvs165s_ns.class_(
    "JVS165SSpeed2700Button", button.Button
)
JVS165SSpeed3100Button = jvs165s_ns.class_(
    "JVS165SSpeed3100Button", button.Button
)

CONF_DIRECTION_PIN = "direction_pin"
CONF_RUNNING = "running"
CONF_PRIMING = "priming"
CONF_PRIME_TIME_REMAINING = "prime_time_remaining"
CONF_COMMANDED_RPM = "commanded_rpm"
CONF_ACTUAL_RPM = "actual_rpm"
CONF_POWER = "power"
CONF_DIAGNOSTIC = "diagnostic"
CONF_STOP_TEST = "stop_test"
CONF_RESUME_TEST = "resume_test"
CONF_SPEED_1500 = "speed_1500"
CONF_SPEED_2700 = "speed_2700"
CONF_SPEED_3100 = "speed_3100"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(JVS165SComponent),
            cv.Required(CONF_DIRECTION_PIN): pins.gpio_output_pin_schema,

            cv.Optional(CONF_RUNNING): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_PRIMING): binary_sensor.binary_sensor_schema(),

            cv.Optional(CONF_PRIME_TIME_REMAINING): sensor.sensor_schema(
                unit_of_measurement=UNIT_SECOND,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),

            cv.Optional(CONF_COMMANDED_RPM): sensor.sensor_schema(
                unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_ACTUAL_RPM): sensor.sensor_schema(
                unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_DIAGNOSTIC): sensor.sensor_schema(
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),

            cv.Optional(CONF_STOP_TEST): button.button_schema(
                JVS165SStopTestButton
            ),
            cv.Optional(CONF_RESUME_TEST): button.button_schema(
                JVS165SResumeTestButton
            ),
            cv.Optional(CONF_SPEED_1500): button.button_schema(
                JVS165SSpeed1500Button
            ),
            cv.Optional(CONF_SPEED_2700): button.button_schema(
                JVS165SSpeed2700Button
            ),
            cv.Optional(CONF_SPEED_3100): button.button_schema(
                JVS165SSpeed3100Button
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    direction_pin = await cg.gpio_pin_expression(config[CONF_DIRECTION_PIN])
    cg.add(var.set_direction_pin(direction_pin))

    if CONF_RUNNING in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_RUNNING])
        cg.add(var.set_running_sensor(sens))

    if CONF_PRIMING in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_PRIMING])
        cg.add(var.set_priming_sensor(sens))

    if CONF_PRIME_TIME_REMAINING in config:
        sens = await sensor.new_sensor(config[CONF_PRIME_TIME_REMAINING])
        cg.add(var.set_prime_time_remaining_sensor(sens))

    if CONF_COMMANDED_RPM in config:
        sens = await sensor.new_sensor(config[CONF_COMMANDED_RPM])
        cg.add(var.set_commanded_rpm_sensor(sens))

    if CONF_ACTUAL_RPM in config:
        sens = await sensor.new_sensor(config[CONF_ACTUAL_RPM])
        cg.add(var.set_actual_rpm_sensor(sens))

    if CONF_POWER in config:
        sens = await sensor.new_sensor(config[CONF_POWER])
        cg.add(var.set_power_sensor(sens))

    if CONF_DIAGNOSTIC in config:
        sens = await sensor.new_sensor(config[CONF_DIAGNOSTIC])
        cg.add(var.set_diagnostic_sensor(sens))

    if CONF_STOP_TEST in config:
        b = await button.new_button(config[CONF_STOP_TEST], var)
        cg.add(var.set_stop_button(b))

    if CONF_RESUME_TEST in config:
        b = await button.new_button(config[CONF_RESUME_TEST], var)
        cg.add(var.set_resume_button(b))

    if CONF_SPEED_1500 in config:
        b = await button.new_button(config[CONF_SPEED_1500], var)
        cg.add(var.set_speed_1500_button(b))

    if CONF_SPEED_2700 in config:
        b = await button.new_button(config[CONF_SPEED_2700], var)
        cg.add(var.set_speed_2700_button(b))

    if CONF_SPEED_3100 in config:
        b = await button.new_button(config[CONF_SPEED_3100], var)
        cg.add(var.set_speed_3100_button(b))
