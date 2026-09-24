# ESP32-Based Battery Management System with Safety Monitoring and Blynk IoT

## Overview

This project implements an ESP32-based Battery Management System (BMS) safety and monitoring platform for a 4-cell battery pack.

The system continuously monitors individual cell voltages, evaluates cell-to-cell voltage imbalance, estimates State of Charge (SoC), detects abnormal sensor and battery conditions, controls a safety relay, and provides real-time telemetry through the Blynk IoT platform.

The system also implements an offline telemetry queue so that important telemetry events generated during communication loss can be replayed when the Blynk connection is restored.

An analytics layer provides a composite battery risk score, battery health classification, and maintenance recommendations.

---

## Key Features

- 4-cell battery voltage monitoring
- Individual cell SoC estimation
- Cell voltage imbalance detection
- Adaptive imbalance threshold based on average SoC
- Voltage imbalance trend detection
- Over-voltage protection
- Under-voltage protection
- Voltage jump detection
- Frozen ADC/sensor detection
- Out-of-range voltage detection
- Relay feedback monitoring
- Relay mismatch detection
- Communication fault detection
- ADC failure simulation
- Communication failure simulation
- Multi-level system safety state machine
- Automatic failsafe relay control
- Controlled shutdown mechanism
- Hysteresis-based fault recovery
- Offline telemetry event queue
- Automatic telemetry replay after reconnection
- Wi-Fi and Blynk connection monitoring
- Blynk IoT dashboard
- Composite battery risk score
- Battery health classification
- Maintenance recommendation system
- Operator recommendation system
- LCD-based local monitoring
- Blynk graph-based telemetry visualization

---

# System Architecture

```text
                    +----------------------+
                    |      4 Cell Pack     |
                    |  Cell 1 - Cell 4    |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    |      ESP32 ADC       |
                    |  Voltage Acquisition |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    | Battery Analysis     |
                    |----------------------|
                    | Cell Voltage         |
                    | SoC Estimation       |
                    | Imbalance            |
                    | Imbalance Trend      |
                    | Adaptive Threshold   |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    |    Fault Detection   |
                    |----------------------|
                    | Over Voltage         |
                    | Under Voltage        |
                    | Voltage Jump         |
                    | Frozen Sensor        |
                    | Out of Range         |
                    | Relay Mismatch       |
                    | Communication Fault  |
                    | ADC Failure          |
                    +----------+-----------+
                               |
                               v
                    +----------------------+
                    |   Safety State FSM   |
                    |----------------------|
                    | NORMAL               |
                    | DEGRADED             |
                    | FAILSAFE             |
                    | SHUTDOWN             |
                    +----------+-----------+
                               |
                 +-------------+-------------+
                 |                           |
                 v                           v
        +------------------+        +------------------+
        | Safety Relay     |        | Analytics Engine |
        | + Feedback       |        | Risk / Health    |
        +------------------+        +--------+---------+
                                             |
                                             v
                                   +-------------------+
                                   | Blynk IoT Cloud   |
                                   | Dashboard         |
                                   +-------------------+
```

---

# Hardware

The project is designed around an ESP32 development board.

### Main Components

| Component | Purpose |
|---|---|
| ESP32 | Main controller |
| 4-cell battery model | Battery monitoring |
| Relay | Safety disconnect/control |
| Relay feedback input | Verifies actual relay state |
| I2C LCD | Local system display |
| Wi-Fi | Cloud connectivity |
| Blynk IoT | Remote monitoring and analytics |

---

# GPIO Configuration

| Function | GPIO |
|---|---:|
| Cell 1 ADC | GPIO 34 |
| Cell 2 ADC | GPIO 35 |
| Cell 3 ADC | GPIO 32 |
| Cell 4 ADC | GPIO 33 |
| Relay | GPIO 19 |
| Relay Feedback | GPIO 18 |
| I2C SDA | GPIO 21 |
| I2C SCL | GPIO 22 |

---

# Battery Monitoring

Each cell is independently sampled through an ESP32 ADC input.

The measured ADC value is converted to voltage using:

```cpp
voltage = (rawValue / 4095.0) * 3.3;
```

The estimated State of Charge is calculated from the measured cell voltage:

```text
SoC = (voltage / 3.3) * 100
```

The value is constrained between 0% and 100%.

---

# Cell Imbalance Detection

The system identifies:

- Weakest cell
- Strongest cell
- Voltage imbalance
- Imbalance trend

The voltage imbalance is calculated as:

```text
Voltage Imbalance =
Strongest Cell Voltage - Weakest Cell Voltage
```

The system also determines whether the imbalance is:

- Stable
- Increasing
- Decreasing

---

# Adaptive Imbalance Threshold

The imbalance threshold changes according to the average battery SoC.

| Average SoC | Threshold |
|---|---:|
| Above 80% | 0.10 V |
| 40–80% | 0.15 V |
| Below 40% | 0.20 V |

This allows the system to use different imbalance limits depending on the operating condition of the battery.

---

# Fault Detection

The BMS detects multiple fault conditions.

## 1. Over-Voltage Fault

Triggered when a cell reaches or exceeds:

```text
3.20 V
```

## 2. Under-Voltage Fault

Triggered when a cell reaches or falls below:

```text
0.80 V
```

## 3. Voltage Jump Fault

The system monitors cell voltage history and detects abnormal voltage jumps.

## 4. Frozen Sensor Fault

A cell sensor is considered potentially frozen when its reading remains effectively unchanged while other battery cells are changing.

## 5. Out-of-Range Fault

Cell voltages outside the valid ADC range are detected.

## 6. Relay Mismatch Fault

The commanded relay state is compared against the physical relay feedback signal.

## 7. Communication Fault

A communication failure can be detected/simulated and is incorporated into the system safety logic.

## 8. ADC Failure Fault

An ADC failure condition can be simulated for testing the fault-handling mechanism.

---

# Safety State Machine

The system uses four main operating states:

```text
NORMAL
   |
   v
DEGRADED
   |
   v
FAILSAFE
   |
   v
SHUTDOWN
```

### NORMAL

The battery is operating normally and no critical fault is detected.

### DEGRADED

The system remains operational but an abnormal condition such as cell imbalance or communication degradation has been detected.

### FAILSAFE

A critical battery or hardware fault has been detected.

The relay is commanded to the safety state.

### SHUTDOWN

A controlled shutdown has been requested.

The system remains in the shutdown state until reset/reinitialization.

---

# Fault Recovery

The system uses hysteresis and verification timing to prevent unstable state transitions.

After a failsafe condition is cleared, the system does not immediately return to normal operation.

Instead, a verification period is applied:

```text
5 seconds
```

Only after the system remains in a valid condition during this period is recovery to NORMAL permitted.

---

# Relay Safety Mechanism

The relay is controlled according to the system state.

| System State | Relay Command |
|---|---|
| NORMAL | LOW |
| DEGRADED | LOW |
| FAILSAFE | HIGH |
| SHUTDOWN | HIGH |

The relay feedback input is continuously monitored to determine whether the physical relay state matches the expected state.

---

# Offline Telemetry Queue

The system includes an offline telemetry event queue.

When Blynk connectivity is unavailable, selected telemetry events are stored locally.

The queue can store up to:

```text
20 events
```

Events include:

- Cell voltage changes
- Relay state changes
- Fault state changes
- System state changes
- Wi-Fi RSSI changes

When Blynk connectivity is restored, the queued events are replayed automatically.

```text
                 Blynk Offline
                      |
                      v
             +------------------+
             | Offline Queue    |
             | Store Events     |
             +--------+---------+
                      |
                 Connection
                   Restored
                      |
                      v
             +------------------+
             | Replay Queue     |
             +--------+---------+
                      |
                      v
                Blynk Cloud
```

---

# Blynk IoT Dashboard

The Blynk dashboard provides remote monitoring of the BMS.

### Datastream Mapping

| Virtual Pin | Parameter |
|---|---|
| V0 | Cell 1 Voltage |
| V1 | Cell 2 Voltage |
| V2 | Cell 3 Voltage |
| V3 | Cell 4 Voltage |
| V4 | Weakest Cell |
| V5 | Strongest Cell |
| V6 | Relay State |
| V7 | Fault State |
| V8 | System State |
| V9 | Wi-Fi RSSI |
| V10 | Offline Queue Depth |
| V11 | Telemetry Status |
| V12 | Fault ID |
| V13 | Voltage Imbalance |
| V14 | Composite Risk Score |
| V15 | Battery Health |
| V16 | Maintenance Recommendation |
| V17 | Uptime |
| V18 | Fault Count |
| V19 | Operator Recommendation |
| V20 | Uptime |
| V21 | State Severity |

---

# Analytics

The analytics layer combines several parameters to generate a composite risk score.

The risk calculation considers:

1. Voltage imbalance
2. Imbalance trend
3. Fault activity
4. State of charge
5. System safety state

The resulting score is constrained between:

```text
0 – 100
```

---

# Battery Health Classification

The composite risk score is used to classify battery health.

| Risk Score | Classification |
|---:|---|
| < 25 | HEALTHY |
| 25–49 | WATCH |
| 50–74 | DEGRADED |
| ≥ 75 | CRITICAL |

---

# Maintenance Recommendations

The system generates recommendations based on the detected condition.

Examples include:

```text
System operating normally
Monitor cell voltage imbalance
Monitor increasing imbalance
Inspect affected battery cell
Inspect relay and feedback
Inspect ADC and voltage sensors
Check communication link
System shutdown - inspect immediately
```

---

# Operator Recommendations

The system also generates an operator-facing recommendation based on the current battery and safety state.

Examples:

```text
SYSTEM OPERATING NORMALLY
MONITOR CELL IMBALANCE
RECHARGE BATTERY
CHECK CELL OVER-VOLTAGE
CHECK CELL UNDER-VOLTAGE
INSPECT CELL SENSOR
CHECK ADC SENSOR
INSPECT RELAY FEEDBACK
CHECK COMMUNICATION LINK
CRITICAL FAULT - CHECK BATTERY
SYSTEM SHUTDOWN - INSPECT BATTERY
```

---

# LCD Interface

A 16×2 I2C LCD provides local system information.

The display cycles through:

### Battery Status

```text
BATTERY STATUS
SOC: XX% BAL
```

or

```text
BATTERY STATUS
SOC: XX% IMBAL
```

### System Status

```text
SYSTEM STATUS
NORMAL RELAY:OFF
```

### Telemetry

```text
TELEMETRY
W:X.XX S:X.XX
```

When a fault is detected, the LCD prioritizes the fault screen.

---

# Blynk Graphs

The dashboard includes graph-based monitoring of battery telemetry.

The system periodically publishes cell voltage values to the Blynk datastreams for visualization.

The graph update interval is:

```text
5 seconds
```

This allows changes in individual cell voltage and battery behavior to be observed over time.

---

# Testing

The system was tested under multiple operating conditions.

### Normal Operation

Verified:

- Cell voltage monitoring
- SoC calculation
- Balanced battery classification
- Relay state
- Blynk telemetry
- LCD operation

### Imbalance Condition

Verified:

- Cell imbalance detection
- DEGRADED state transition
- Recovery to NORMAL after the condition cleared

### Fault Conditions

The fault handling framework supports testing of:

- Over-voltage
- Under-voltage
- Voltage jump
- Frozen sensor
- Relay mismatch
- ADC failure
- Communication failure

### Communication Recovery

The offline telemetry queue was tested by:

1. Starting the system without an active Blynk connection.
2. Generating telemetry events.
3. Allowing the events to enter the offline queue.
4. Restoring connectivity.
5. Verifying automatic queue replay.

---

# Project Structure

```text
BMS-ESP32/
│
├── README.md
│
├── src/
│   └── BMS_PROJECT.ino
│
├── docs/
│   └── BMS_Safety_System_Report.pdf
│
├── screenshots/
│   ├── dashboard.png
│   ├── graphs.png
│   ├── normal_state.png
│   └── fault_state.png
│
└── simulation/
    └── Wokwi project files / link
```

---

# Technologies Used

- ESP32
- Arduino Framework
- C/C++
- Blynk IoT
- Wi-Fi
- I2C
- LCD
- ADC-based voltage sensing
- Finite State Machine
- Embedded fault detection
- Telemetry buffering
- Battery analytics

---

# Simulation

The project can be tested using an ESP32 simulation environment such as Wokwi.

The simulation allows the battery monitoring, relay logic, fault detection, LCD interface, Wi-Fi connection, Blynk telemetry and analytics functionality to be evaluated without requiring a physical battery pack.

---

# Results

The completed system demonstrates:

- Continuous multi-cell voltage monitoring
- Battery imbalance detection
- Automated fault identification
- Safety-state transitions
- Relay-based protection
- Relay feedback verification
- Offline telemetry buffering
- Cloud telemetry through Blynk
- Battery risk assessment
- Battery health classification
- Maintenance recommendations
- Operator recommendations
- Historical graph visualization

The system was tested for state transitions and Blynk telemetry, with transitions being reflected on the dashboard within seconds.

---

# Future Scope

Possible improvements include:

- Accurate battery SoC estimation using coulomb counting
- State of Health (SoH) estimation
- Temperature sensing
- Current sensing
- Active/passive cell balancing hardware
- EEPROM/Flash-based persistent fault logging
- Secure credential management
- Mobile notifications for critical faults
- CAN communication
- Hardware battery protection circuitry
- Integration with a real multi-cell battery pack
- More advanced battery degradation prediction

---

# Important Note

This project is an embedded monitoring and safety-system prototype.

The voltage model and SoC calculation are simplified for the prototype implementation and should not be considered a production-grade battery protection algorithm without appropriate electrical characterization, sensing circuitry, calibration, isolation, current/temperature monitoring, and hardware protection mechanisms.

---

## Author

**Kaushal Naik**

Electronics – VLSI Design and Technology  
Goa College of Engineering
