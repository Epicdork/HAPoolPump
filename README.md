# Jacuzzi JVS165S / Hayward Max Flo VS ESPHome / Home Assistant Controller

Reverse-engineering and controlling a Jacuzzi JVS165S / Hayward Max Flo VS variable-speed pool pump using an ESP32-S3, RS-485, ESPHome, and Home Assistant. The Jacuzzi JVS165S is just a rebranded Hayward Max Flo pump, just in white and it was cheaper.

---

## Project Overview

This project started with a simple question:

> Can the Jacuzzi JVS165S / Hayward Max Flo VS variable-speed pump be controlled directly over its RS-485 connection?

The JVS165S provides a black four-terminal automation connector labeled:

```text
12V
A
B
COM
```
This located under the upper rear cover of the motor where mains power is connected. Removal of three three screws to access the pump control board is required. One half is the dangerous side - DO NOT TOUCH! The other side has the black connector and a green relay connector.

Unfortunately, the communication protocol does not appear to be publicly documented in enough detail to build a controller around it.

The goal became to passively capture the factory controller traffic, determine the serial settings and protocol structure, decode useful commands and responses, and eventually replace the factory controller with an ESP32-based controller.

The project reached that goal.

The current implementation can:

- Start and resume the pump
- Perform the pump's prime/startup sequence
- Stop the pump
- Select specific operating RPMs
- Read actual RPM
- Read pump power consumption
- Determine running/stopped state
- Expose controls and sensors through ESPHome
- Control the pump from Home Assistant
- Operate directly from the ESP32 without Home Assistant being required for pump communications

The primary development platform is a:

```text
Waveshare ESP32-S3-RS485-CAN-U
```

---

# Hardware

## Controller

Relevant GPIO assignments:

```text
UART TX        GPIO17
UART RX        GPIO18
RS-485 Enable  GPIO21
```

GPIO21 controls the half-duplex RS-485 transceiver:

```text
GPIO21 LOW   = Receive
GPIO21 HIGH  = Transmit
```

The ESP32 is powered independently over USB.

---

# Pump Connection

During passive sniffing, the original Jacuzzi controller remained connected and the ESP32 listened in parallel.

The working connection was:

```text
Jacuzzi JVS165S        Waveshare
--------------------------------
A                  -> RS485 A+
B                  -> RS485 B-
```

The following were not connected to the ESP32:

```text
12V
COM
```

The ESP32 was independently powered.

For active transmission testing, the factory controller's A/B connection was disconnected so that only one device was acting as the RS-485 master.

That is important because two controllers transmitting on the same half-duplex bus can corrupt traffic or cause unpredictable behavior.

---

# ESPHome Project Folder Structure

The custom JVS165S protocol logic is implemented as a local ESPHome component, so the project is split across several files.

```text
/homeassistant/esphome/
├── pool.yaml
├── secrets.yaml
└── components/
    └── jvs165s/
        ├── __init__.py
        ├── jvs165s.h
        └── jvs165s.cpp
```

- `pool.yaml` — Main ESPHome configuration for the Waveshare board, including Wi-Fi, UART, GPIO assignments, and the `jvs165s:` component setup.
- `secrets.yaml` — Stores private values such as Wi-Fi credentials, API encryption keys, and OTA passwords referenced with `!secret`.
- `components/jvs165s/__init__.py` — Defines the custom ESPHome component schema and connects YAML options to the C++ classes.
- `components/jvs165s/jvs165s.h` — Declares the JVS165S component classes, sensors, buttons, state variables, and protocol functions.
- `components/jvs165s/jvs165s.cpp` — Contains the actual pump-control logic, including CRC calculation, byte escaping, RS-485 communication, priming, RPM control, and response parsing.

Inside the ESPHome add-on/container, the same structure may appear under:

```text
/config/esphome/
```

The relative layout remains the same.

---

# Why the `components` Folder Is Required

ESPHome normally uses built-in components such as:

```yaml
sensor:
binary_sensor:
button:
uart:
wifi:
```

The JVS165S protocol is not a built-in ESPHome component, so a local external component was created.

ESPHome is told to load it with:

```yaml
external_components:
  - source:
      type: local
      path: components
```

The path is relative to the main ESPHome configuration directory.

The folder name:

```text
components/jvs165s/
```

corresponds to the YAML section:

```yaml
jvs165s:
```

The custom component contains the logic for:

```text
Jacuzzi packet formatting
CRC generation
byte stuffing
RS-485 transmit/receive direction
STOP
Resume / Prime
RPM commands
status parsing
wattage parsing
pump state management
```

---

# ESPHome File Responsibilities

## `__init__.py`

This is the ESPHome integration layer. It tells ESPHome which YAML options are valid and connects those options to the underlying C++ classes.

Typical options include:

```text
uart_id
direction_pin
running
priming
commanded_rpm
actual_rpm
power
diagnostic
stop_test
resume_test
speed_1500
speed_2700
speed_3100
```

Without this file, ESPHome would not understand the custom `jvs165s:` YAML block.

## `jvs165s.h`

This is the C++ header. It declares:

```text
component classes
button classes
sensor pointers
pump state variables
timing constants
protocol functions
```

Examples of methods used by the component include:

```text
send_stop_test()
send_resume_test()
set_speed_1500()
set_speed_2700()
set_speed_3100()
```

## `jvs165s.cpp`

This contains the actual pump-control implementation. It performs:

```text
packet construction
CRC calculation
0D byte escaping
RS-485 direction switching
command transmission
response parsing
prime timing
RPM control
wattage parsing
Home Assistant state publishing
```

---

# Using `secrets.yaml`

ESPHome supports storing private values separately in:

```text
secrets.yaml
```

For example:

```yaml
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
```

The corresponding file might contain:

```yaml
wifi_ssid: "MyWiFi"
wifi_password: "MyPassword"
```

Other common secret values may include:

```text
ESPHome API encryption key
OTA password
MQTT credentials
other authentication tokens
```

A `!secret` reference only works if that exact name exists in `secrets.yaml`.

For example, this:

```yaml
password: !secret fallback_password
```

requires this:

```yaml
fallback_password: "actual-password"
```

If the secret is missing, ESPHome will fail validation with an error such as:

```text
Secret 'fallback_password' not defined
```

A useful rule is:

```text
If pool.yaml contains:

!secret some_name

then secrets.yaml must contain:

some_name: "actual value"
```

The pump protocol settings themselves are not secrets. These remain directly in the configuration:

```text
GPIO17
GPIO18
GPIO21
1200 baud
8N1
RPM values
prime duration
polling interval
```

---

# Main `pool.yaml` Pump Configuration

The relevant UART section is:

```yaml
uart:
  id: pump_uart
  tx_pin: GPIO17
  rx_pin: GPIO18
  baud_rate: 1200
  data_bits: 8
  parity: NONE
  stop_bits: 1
```

The local pump component is then instantiated approximately as:

```yaml
jvs165s:
  id: pool_pump
  uart_id: pump_uart
  direction_pin: GPIO21

  running:
    name: "Running"

  priming:
    name: "Priming"

  commanded_rpm:
    name: "Commanded RPM"

  actual_rpm:
    name: "Actual RPM"

  power:
    name: "Power"

  diagnostic:
    name: "Diagnostic"
    entity_category: diagnostic

  stop_test:
    name: "Pool Pump STOP"

  resume_test:
    name: "Pool Pump Resume"

  speed_1500:
    name: "Pool Pump 1500 RPM"

  speed_2700:
    name: "Pool Pump 2700 RPM"

  speed_3100:
    name: "Pool Pump 3100 RPM"
```

This creates the Home Assistant entities used to operate and monitor the pump.

---

# Validating the ESPHome Configuration

Before compiling or installing firmware, validate the configuration.

A successful validation should end with:

```text
INFO Configuration is valid!
```

If ESPHome cannot find the component, common causes include:

```text
incorrect external_components path
incorrect folder name
missing __init__.py
YAML indentation errors
```

If a secret is missing, ESPHome normally reports the missing secret name directly.

---

# Discovering the Serial Settings

Several baud rates initially appeared to produce recognizable data, including:

```text
2400
4800
1200
```

The breakthrough came from measuring individual bit timing.

Repeated signal transitions measured approximately:

```text
833 µs
```

Since:

```text
1 / 1200 = 833.33 µs
```

the true baud rate was identified as:

```text
1200 baud
```

Further testing established the final UART configuration:

```text
1200 baud
8 data bits
No parity
1 stop bit
```

or simply:

```text
1200 8N1
```

---

# Packet Framing

Most packets end with:

```text
0D 0A
```

which acts as the protocol terminator.

Four major packet families were observed:

```text
01 01 ...
01 40 ...
01 07 60 ...
01 60 ...
```

Further testing revealed that the protocol follows a controller-initiated request/response model.

---

# Communication Model

With the factory controller disconnected and the ESP32 operating receive-only, the pump produced no traffic.

That strongly indicates that the pump does not normally broadcast status on its own.

Instead, the controller initiates each exchange.

The normal pattern appears to be:

```text
Controller -> 01 01 command
Pump       -> 01 40 status

Controller -> 01 07 60 extended-status request
Pump       -> 01 60 extended-status response
```

---

# RPM Command

Normal speed commands use the following logical structure:

```text
01 01 RPM_LO RPM_HI CRC 0D 0A
```

RPM is encoded as a 16-bit little-endian value.

For example:

```text
1500 decimal = 0x05DC
```

Little-endian representation:

```text
DC 05
```

Complete command:

```text
01 01 DC 05 36 0D 0A
```

Known examples:

| RPM | Command |
|---:|---|
| 1150 | `01 01 7E 04 03 0D 0A` |
| 1500 | `01 01 DC 05 36 0D 0A` |
| 1725 | `01 01 BD 06 DF 0D 0A` |
| 2000 | `01 01 D0 07 C4 0D 0A` |
| 2300 | `01 01 FC 08 BB 0D 0A` |
| 2700 | `01 01 8C 0A 17 0D 0A` |
| 2800 | `01 01 F0 0A 49 0D 0A` |
| 3200 | `01 01 80 0C F9 0D 0A` |

The CRC can be generated dynamically, so operation is not limited to these captured RPM values.

---

# STOP Command

Stopping the pump uses an RPM value of zero:

```text
01 01 00 00 6B 0D 0A
```

This command was successfully transmitted from the ESP32 and acknowledged by the pump.

It became the first safe active-transmission test because the pump was already stopped when the command was sent.

The final ESPHome component also handles an important Home Assistant state issue here. Once a STOP command is acknowledged by an `01 40` response showing:

```text
Actual RPM = 0
```

the component immediately publishes:

```text
Running = OFF
Power = 0 W
Priming = OFF
Prime Time Remaining = 0 seconds
```

This is necessary because the controller intentionally stops normal extended-status polling while the pump is stopped. Without explicitly publishing `0 W`, Home Assistant would retain the last wattage value received while the pump was running.

The pump therefore does not need to be continuously polled while stopped simply to keep the Home Assistant power value accurate.

---

# START / RESUME Command

Starting the pump requires a special command rather than simply sending a target RPM.

The factory controller sends:

```text
01 01 7A 4D AF 0D 0A
```

The data value:

```text
7A 4D
```

corresponds to:

```text
0x4D7A
```

This is not a realistic RPM value and behaves as a special Resume/Prime command.

A single transmission generally does not immediately start the pump. Repeated transmissions approximately once per second cause the pump to begin its startup sequence.

---

# Prime Sequence

When Resume is selected, the controller repeatedly sends:

```text
01 01 7A 4D AF 0D 0A
```

approximately once per second.

The pump accelerates to roughly:

```text
~3450 RPM
```

during priming.

The ESPHome implementation uses a:

```text
60-second prime cycle
```

At the same time, the component publishes a Home Assistant sensor named:

```text
Prime Time Remaining
```

The value begins at:

```text
60 seconds
```

and is updated approximately once per second:

```text
60
59
58
57
...
3
2
1
0
```

If STOP is pressed during priming, the countdown is immediately reset to:

```text
0 seconds
```

When the prime period completes normally, `Priming` is turned off, the countdown is set to zero, and the controller transitions to the selected operating RPM.

```text
Resume
   |
   v
Priming = ON
Prime Time Remaining = 60
   |
   v
Prime command every ~1 second
   |
   v
Pump reaches ~3450 RPM
   |
   v
Countdown: 59 ... 2 ... 1 ... 0
   |
   v
Priming = OFF
   |
   v
Selected speed command begins
```

Because this countdown is calculated locally from the ESP32's prime timer, it does not require any additional RS-485 traffic to the pump.

---

# Normal Status Response

The pump responds to a normal `01 01` command with an `01 40` packet.

A typical logical packet looks approximately like:

```text
01 40 SS 00 00 00 00 00 00 DD 00 00 00 RPM_LO RPM_HI CRC 0D 0A
```

The important confirmed field is:

```text
bytes 13-14
```

which contain actual pump RPM in little-endian format.

Example:

```text
79 0D
```

means:

```text
0x0D79 = 3449 RPM
```

This lets Home Assistant display measured pump RPM rather than merely the requested RPM.

---

# Running State

One of the status bytes initially appeared as though it might represent running state, but it was inconsistent.

The more reliable method is:

```text
Actual RPM > 0  -> Running
Actual RPM = 0  -> Stopped
```

This became the basis of the ESPHome `Running` binary sensor.

---

# Extended Status Request

The factory controller periodically requests additional information with:

```text
01 07 60 4C 0D 0A
```

The pump answers with an:

```text
01 60 ...
```

response.

---

# Power Consumption

A typical extended-status response resembles:

```text
01 60 XX 01 LL 00 WW_LO WW_HI DD 00 DD 00 YY 68 00 CRC 0D 0A
```

Bytes 6 and 7 are confirmed to contain pump power in watts.

The value is little-endian.

Observed examples include:

| Bytes | Power |
|---|---:|
| `5C 00` | 92 W |
| `DF 01` | 479 W |
| `EB 01` | 491 W |
| `C4 02` | 708 W |
| `7F 03` | 895 W |
| `B7 01` | 439 W |

These values followed pump speed and load closely enough to confirm the field.

---

# CRC Algorithm

Each command contains a one-byte CRC immediately before the `0D 0A` packet terminator.

The algorithm was identified as:

```text
CRC-8
Polynomial: 0x07
Initial value: 0x01
XOR out: 0x00
MSB-first
Non-reflected
```

The CRC is calculated over all logical bytes before the CRC byte. The final `0D 0A` terminator is not included.

Working C++ implementation:

```cpp
uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0x01;

  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];

    for (int bit = 0; bit < 8; bit++) {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x07;
      else
        crc <<= 1;
    }
  }

  return crc;
}
```

Validated examples:

```text
01 01 7E 04 -> 03
01 01 BD 06 -> DF
01 01 FC 08 -> BB
01 01 80 0C -> F9
01 01 00 00 -> 6B
01 01 DC 05 -> 36
01 01 7A 4D -> AF
01 07 60    -> 4C
```

---

# Byte Stuffing / Escaping

One of the most important discoveries was that literal `0D` bytes inside packet data are escaped.

A data byte:

```text
0D
```

is transmitted on the wire as:

```text
0D 0D
```

The final terminator remains:

```text
0D 0A
```

For example, this raw wire packet:

```text
01 40 01 00 00 00 00 00 00 25 00 00 00 79 0D 0D F1 0D 0A
```

must first be unescaped to:

```text
01 40 01 00 00 00 00 00 00 25 00 00 00 79 0D F1 0D 0A
```

The RPM field is then:

```text
79 0D
```

which equals:

```text
3449 RPM
```

and the CRC validates correctly.

The correct processing order is therefore:

```text
Build logical packet
       |
       v
Calculate CRC
       |
       v
Append CRC
       |
       v
Escape literal 0D data bytes as 0D 0D
       |
       v
Append final 0D 0A terminator
```

---

# Active Transmission Validation

The first active test sent a STOP command while the pump was already stopped:

```text
01 01 00 00 6B 0D 0A
```

The pump responded with:

```text
01 40 00 00 00 00 00 00 00 25 00 00 00 00 00 59 0D 0A
```

Actual RPM decoded as:

```text
0 RPM
```

This verified:

```text
UART configuration
RS-485 polarity
TX wiring
RX wiring
direction control
CRC algorithm
packet framing
pump command acceptance
pump response decoding
```

That was the point where the project moved from passive reverse engineering into confirmed active control.

---

# RS-485 Direction Timing

The Waveshare uses half-duplex RS-485.

The transmit sequence is conceptually:

```cpp
direction_pin_->digital_write(true);

delayMicroseconds(200);

uart_->write_array(data, length);

uart_->flush();

direction_pin_->digital_write(false);
```

Timing instrumentation showed:

```text
7-byte packet at 1200 8N1
Expected duration:             ~58.33 ms

Measured TX -> flush return:   58.448 ms
flush -> RX mode:               0.017 ms
RX mode -> first RX byte:       9.415 ms
```

This established that `flush()` waits for the final UART byte to physically finish transmitting.

No additional post-flush delay is needed before returning the RS-485 transceiver to receive mode.

---

# Normal Operating Polling

After priming, the controller alternates normal speed/status and extended-status transactions.

The current implementation follows approximately:

```text
Send selected RPM
      |
      v
Receive 01 40 status
      |
      v
Wait ~2.135 seconds
      |
      v
Send 01 07 60
      |
      v
Receive 01 60 extended status
      |
      v
Wait ~2.135 seconds
      |
      +---- repeat
```

Each transaction type therefore repeats roughly every:

```text
4.27 seconds
```

which closely matches the observed factory controller cadence.

---

# Current ESPHome State Machine

The custom component currently operates using three main states.

## Stopped

```text
Actual RPM = 0
Running = OFF
Priming = OFF
Prime Time Remaining = 0
Power = 0 W after STOP is confirmed
Normal polling stopped
```

The power value is explicitly set to zero after the pump acknowledges STOP with a zero-RPM status response.

## Priming

```text
Priming = ON
Prime Time Remaining = 60 -> 0
Send Resume/Prime command
approximately once per second
for 60 seconds
```

The countdown is maintained locally by the ESP32 and published once per second.

## Running

```text
Priming = OFF
Prime Time Remaining = 0

RPM command
01 40 response

Extended-status request
01 60 response
```

---

# Home Assistant Controls

The current Home Assistant implementation provides controls for:

```text
1500 RPM
2700 RPM
3100 RPM
Start / Resume
Stop
```

and exposes:

```text
Actual RPM
Commanded RPM
Power
Running
Priming
Prime Time Remaining
Diagnostic
```

The speed buttons behave differently depending on pump state.

If stopped:

```text
Selecting a speed changes the target RPM
but does not start the pump.
```

If priming:

```text
Selecting a speed changes the RPM
that will be used when prime finishes.
```

If running:

```text
Selecting a speed changes RPM immediately.
```

---

# Current Dashboard

The Home Assistant card is currently arranged approximately like:

```text
┌───────────────────────────────────────┐
│            🏊 Pool Control            │
├────────────┬────────────┬─────────────┤
│ 🌊 Low     │ 🌊 Medium  │ 🌊 High     │
├───────────────────┬───────────────────┤
│ ▶ Start / Resume  │      ⏹ STOP       │
├───────────────────┼───────────────────┤
│ Pump Speed        │ Pump Power        │
│ 2700 RPM          │ 439 W             │
└───────────────────┴───────────────────┘
```

Current presets:

```text
Low     1500 RPM
Medium  2700 RPM
High    3100 RPM
```

---


# Current Quality-of-Life Improvements

Two issues identified during normal use have now been incorporated directly into the final ESPHome component.

## Accurate 0 W state after STOP

The controller no longer leaves Home Assistant showing the last running wattage after the pump stops.

Once a STOP response confirms:

```text
Actual RPM = 0
```

the component publishes:

```text
Power = 0 W
```

and remains stopped without continuing unnecessary extended-status polling.

## Prime countdown sensor

The component now exposes:

```text
Prime Time Remaining
```

which counts from 60 seconds down to zero during the prime cycle.

This gives Home Assistant a clean source for displaying:

```text
Priming
43 seconds remaining
```

or for adding the remaining time directly to a dashboard button or status card.

The sensor is already implemented in the ESPHome component. Styling the Home Assistant Resume button to display that countdown is a dashboard/UI enhancement rather than a pump-protocol change.


# Future Local Touchscreen - On order

Replacing the original controller also removes the original physical user interface and means you need access to the Home Assistant dashboard to control. The plan is to mount a RS485 capable touch screen display inside a waterproof enclosure on my patio. 

A future version may use a 4-inch touchscreen to restore completely local pump control.

The purchased hardware is:

```text
Waveshare ESP32-S3-Touch-LCD-4
```
This specific device was chosen as researching bigger displays with higher resolution have indicated a lack of bandwidth trying to preform operations while connected to WIFI.

A possible interface:

```text
┌────────────────────────────────┐
│          POOL PUMP             │
│                                │
│          2700 RPM              │
│            439 W               │
│                                │
│  ┌──────┐ ┌──────┐ ┌──────┐   │
│  │ 1500 │ │ 2700 │ │ 3100 │   │
│  │ LOW  │ │ MED  │ │ HIGH │   │
│  └──────┘ └──────┘ └──────┘   │
│                                │
│        ▶ START / RESUME        │
│                                │
│           ■ STOP               │
└────────────────────────────────┘
```

During priming:

```text
┌────────────────────────────────┐
│            PRIMING             │
│                                │
│               43               │
│       seconds remaining        │
│                                │
│       ███████████░░░░░         │
│                                │
│           ■ STOP               │
└────────────────────────────────┘
```

The intention is for touchscreen actions to call the custom pump component directly.

That means local operation could continue even if:

```text
Home Assistant is offline
Wi-Fi is unavailable
The router is restarting
```

---

# Wiring Reference

```text
                       Jacuzzi JVS165S
                       Automation Port

                       ┌──────────────┐
                       │ 12V          │
                       │ A            │────────────┐
                       │ B            │──────────┐ │
                       │ COM          │          │ │
                       └──────────────┘          │ │
                                                 │ │
                                ┌────────────────┘ │
                                │                  │
                                ▼                  ▼
                        ┌──────────────────────────────┐
                        │ Waveshare ESP32-S3-RS485-CAN │
                        │                              │
                        │ A+  <---------------- Pump A │
                        │ B-  <---------------- Pump B │
                        │                              │
                        │ ESP32 TX  GPIO17             │
                        │ ESP32 RX  GPIO18             │
                        │ RS485 EN   GPIO21             │
                        │                              │
                        │ USB power                    │
                        └──────────────────────────────┘
```

Current working setup:

```text
Pump A   -> RS485 A+
Pump B   -> RS485 B-

Pump 12V -> not connected
Pump COM -> not connected
```

---

# Protocol Quick Reference

## Serial configuration

```text
Baud rate:  1200
Data bits:  8
Parity:     None
Stop bits:  1
Mode:       Half-duplex RS-485
```

## Packet terminator

```text
0D 0A
```

## CRC

```text
CRC-8
Polynomial: 0x07
Init:       0x01
XOR out:    0x00
Reflected:  No
```

## Byte escaping

```text
Logical data byte:
0D

Wire representation:
0D 0D

Packet terminator:
0D 0A
```

## Normal RPM command

```text
01 01 RPM_LO RPM_HI CRC 0D 0A
```

Example:

```text
2700 RPM
=
01 01 8C 0A 17 0D 0A
```

## STOP

```text
01 01 00 00 6B 0D 0A
```

## Resume / Prime

```text
01 01 7A 4D AF 0D 0A
```

Repeat approximately once per second during the prime cycle.

## Request extended status

```text
01 07 60 4C 0D 0A
```

## Standard pump status

```text
01 40 ...
```

Actual RPM:

```text
bytes 13-14
little-endian
```

## Extended pump status

```text
01 60 ...
```

Power:

```text
bytes 6-7
little-endian watts
```

---

# Known Packet Flow

Normal running operation:

```text
Controller
    |
    |  01 01 RPM_LO RPM_HI CRC 0D 0A
    v
Pump
    |
    |  01 40 ... actual RPM ... CRC 0D 0A
    v

~2.135 seconds

Controller
    |
    |  01 07 60 4C 0D 0A
    v
Pump
    |
    |  01 60 ... watts ... CRC 0D 0A
    v

~2.135 seconds

repeat
```

---

# Known and Unknown Fields

## Confirmed

```text
Requested RPM
Actual RPM
Power in watts
STOP
Resume / Prime
Extended-status poll
CRC
Packet escaping
Packet termination
Controller/pump transaction direction
```

## Not Yet Confirmed

Several fields remain deliberately undocumented rather than guessed.

These include:

- Some `01 40` status bytes
- Some `01 60` status fields
- A value normally around 35–41
- A field that increases with RPM/load
- Exact meaning of:

```text
01 01 00 20 8B 0D 0A
```

The 35–41 field might represent temperature, and the load-correlated field might represent current or torque, but there is not currently enough evidence to label either one confidently.

---

# Reverse-Engineering Process

The project followed a fairly traditional protocol-reversing workflow.

```text
1. Connect to RS-485 in receive-only mode

2. Capture factory controller traffic

3. Experiment with UART settings

4. Measure physical bit timing

5. Identify 1200 8N1

6. Identify 0D 0A framing

7. Compare captures at different RPMs

8. Discover little-endian RPM encoding

9. Identify CRC parameters

10. Discover 0D byte stuffing

11. Disconnect factory controller

12. Confirm pump does not transmit by itself

13. Send STOP while pump is already stopped

14. Verify valid pump response

15. Test Resume command

16. Reproduce factory prime behavior

17. Decode actual RPM

18. Decode power

19. Match factory polling cadence

20. Build ESPHome/Home Assistant controller
```

That progression minimized the amount of unknown behavior introduced at any one step.

---

# Safety and Disclaimer

This is an unofficial reverse-engineered implementation YMMV. Proceed at your own risk! 

Anyone reproducing this project should understand that a pool pump is high-power rotating equipment and this protocol is undocumented.

Recommended precautions:

- Start with passive monitoring.
- Verify RS-485 polarity before transmitting.
- Disconnect the factory controller before active transmission.
- Do not allow multiple RS-485 masters to transmit simultaneously.
- Begin active testing with STOP while the pump is already stopped.
- Ensure the hydraulic system is ready before starting the pump.
- Verify CRC and packet contents before transmission.
- Avoid sending random values to unknown commands or fields.
- Maintain a reliable STOP mechanism.
- Do not assume unknown fields have a particular meaning without confirming them.

The behavior described here was observed on the system used for this project.

Different pump firmware or models could behave differently.

---

# Project Status

```text
[✓] Passive RS-485 capture
[✓] UART settings identified
[✓] Packet framing identified
[✓] CRC solved
[✓] Byte escaping solved
[✓] RPM command decoded
[✓] STOP decoded
[✓] Resume/Prime decoded
[✓] Actual RPM decoded
[✓] Wattage decoded
[✓] Controller/pump direction identified
[✓] Active transmission verified
[✓] ESPHome custom component working
[✓] Home Assistant integration working
[✓] 1500 / 2700 / 3100 RPM controls
[✓] 60-second prime sequence
[✓] Prime Time Remaining countdown sensor
[✓] Automatic 0 W publication after confirmed STOP
[ ] Resume-button countdown presentation in Home Assistant
[ ] Local touchscreen interface
[ ] Additional protocol fields
```

---

# Final Architecture

```text
                    Wi-Fi
                      │
                      ▼
              ┌────────────────┐
              │ Home Assistant │
              └───────┬────────┘
                      │ ESPHome API
                      ▼
          ┌──────────────────────────────┐
          │ Waveshare ESP32-S3-RS485-CAN│
          │                              │
          │ Custom JVS165S Component     │
          │                              │
          │ • CRC generation             │
          │ • byte stuffing              │
          │ • packet parsing             │
          │ • prime state machine        │
          │ • speed control              │
          │ • RPM monitoring             │
          │ • wattage monitoring         │
          └──────────────┬───────────────┘
                         │
                   RS-485 A / B
                         │
                         ▼
               ┌──────────────────┐
               │ Jacuzzi JVS165S  │
               │ Variable-Speed   │
               │ Pool Pump        │
               └──────────────────┘
```

---

# Closing Notes

The most satisfying part of this project was that no single discovery solved everything.

The final result came from a chain of smaller breakthroughs:

```text
bit timing
   ↓
correct baud rate
   ↓
packet framing
   ↓
RPM encoding
   ↓
CRC
   ↓
byte escaping
   ↓
transaction direction
   ↓
safe active transmission
   ↓
startup / prime behavior
   ↓
telemetry decoding
```

Each discovery made the next one possible.

What started as an undocumented pair of RS-485 wires is now a usable control interface that can start, stop, monitor, and control the JVS165S from ESPHome and Home Assistant.

There is still room to take the project further, especially with:

```text
Home Assistant Resume-button countdown presentation
Local 4-inch touchscreen control
Additional protocol decoding
```
