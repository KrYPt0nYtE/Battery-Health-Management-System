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
