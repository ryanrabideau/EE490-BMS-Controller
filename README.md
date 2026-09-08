# EE 490 - 24V Li-ion Battery Management System

This repository contains the hardware and firmware development for our EE 490 senior design project: a Battery Management System (BMS) for a custom **2P7S lithium-ion battery pack**.

The system uses an **STM32F411RE** microcontroller with an **ST L9963E** battery monitoring AFE and L9963T interface. The goal is to monitor individual cell voltages, pack current, temperature, and state of charge while providing fault detection, UART telemetry, and active cell balancing.

## System Overview

- **Battery configuration:** 2P7S
- **Cells:** Samsung INR18650-25R, 2.5 Ah per cell
- **Pack capacity:** 5.0 Ah
- **Nominal pack voltage:** 25.2 V
- **MCU:** STM32F411RE
- **Battery monitoring AFE:** ST L9963E
- **AFE interface:** ST L9963T
- **MCU/AFE communication:** SPI2
- **Telemetry:** USART2 UART
- **Firmware:** C using STM32 HAL
- **Balancing:** Chained switched-capacitor active balancing

## Firmware

The STM32 firmware is located in [`firmware/EE490BMS/`](firmware/EE490BMS/).

Communication with the L9963E is handled through the L9963T interface. The low-level driver handles register communication with the AFE, while the application layer processes the measurements and keeps track of the overall BMS state.

The firmware currently supports:

- L9963E initialization and addressing
- Register reads and writes
- Cell-voltage acquisition
- Pack-voltage calculation
- Minimum and maximum cell-voltage tracking
- Cell-to-cell voltage delta calculation
- Cell undervoltage and overvoltage detection
- Pack-current measurement using the L9963E current ADC
- Coulomb counting
- State-of-charge estimation
- Raw GPIO acquisition for the thermistors
- UART telemetry
- Communication and measurement timeouts
- Validity tracking for voltage, current, Coulomb-counting, and SoC data

The main application currently runs as a simple superloop with approximately one update per second. FreeRTOS files are still present in the generated STM32 project, but the BMS application itself does not currently use the RTOS scheduler.

## Current Progress

Communication between the STM32F411RE and L9963E is working, including device addressing and register reads/writes. Cell-voltage measurements have also been read successfully from the current four-cell bench setup.

The higher-level firmware for the final 7S system is now in place. This includes voltage processing, voltage fault detection, current sensing, Coulomb counting, SoC estimation, and UART telemetry. The current and Coulomb-counting code still needs to be validated on the complete hardware setup.

The final pack will use seven series cell groups, but the current bench configuration only enables **CELL1, CELL2, CELL13, and CELL14** on the L9963E. The final seven-cell channel mapping will be confirmed when the complete pack is connected.

Current sensing is implemented using the L9963E's 18-bit current ADC. The firmware currently contains a temporary **0.1 mΩ shunt value**, which will be replaced once the final shunt resistance is confirmed. Current polarity also needs to be verified on the physical hardware before the SoC calculation is considered final.

Temperature acquisition is partially implemented. The firmware can read the seven GPIO channels intended for the thermistors and convert the raw ADC values to voltages, but the final voltage-to-temperature conversion has not been added yet.

Active balancing is also still under development. The planned system uses a chained switched-capacitor topology, with the balancing hardware controlled by the STM32.

## UART Telemetry

The latest BMS measurements can be formatted and transmitted over USART2 for debugging and monitoring from a PC.

Example output:

```text
PACK: 25.184 V | CELLS: 3.598 3.602 3.596 3.600 3.595 3.601 3.592 V | CURRENT: -1.243 A | SOC: 81.7 % | VALID[V:1 I:1 SOC:1]
```

The validity flags make it possible to distinguish valid measurements from failed or incomplete AFE communication.

## Repository Structure

- [`firmware/`](firmware/) - STM32 firmware
  - [`EE490BMS/`](firmware/EE490BMS/) - Main STM32CubeIDE project
  - [`Core/Inc/`](firmware/EE490BMS/Core/Inc/) - Application and driver headers
  - [`Core/Src/`](firmware/EE490BMS/Core/Src/) - Application, driver, and STM32 source files
  - [`EE490BMS.ioc`](firmware/EE490BMS/EE490BMS.ioc) - STM32CubeMX configuration
- [`hardware/`](hardware/) - Battery pack, enclosure, and mechanical CAD
  - [`STL Exports/`](hardware/STL%20Exports/) - Exported 3D-printable parts
  - [`Vendor/`](hardware/Vendor/) - Vendor component models
- [`docs/`](docs/) - Project documentation
- [`pc_interface/`](pc_interface/) - PC-side interface development

## Main Firmware Files

[`bms_app.c`](firmware/EE490BMS/Core/Src/bms_app.c) and [`bms_app.h`](firmware/EE490BMS/Core/Inc/bms_app.h) contain the higher-level BMS logic, including voltage processing, current measurement, Coulomb counting, SoC estimation, fault detection, and telemetry.

[`L9963E_utils.c`](firmware/EE490BMS/Core/Src/L9963E_utils.c) and [`L9963E_utils.h`](firmware/EE490BMS/Core/Inc/L9963E_utils.h) provide the interface between the application layer and the L9963E driver.

[`L9963E.c`](firmware/EE490BMS/Core/Src/L9963E.c) and [`L9963E_drv.c`](firmware/EE490BMS/Core/Src/L9963E_drv.c) contain the lower-level L9963E functionality and communication.

[`stm32_if.c`](firmware/EE490BMS/Core/Src/stm32_if.c) provides the STM32 hardware interface used by the L9963E driver.

[`main.c`](firmware/EE490BMS/Core/Src/main.c) contains the STM32 initialization and main BMS superloop.

## Building the Firmware

The firmware is developed using **STM32CubeIDE** and **STM32CubeMX**.

The CubeIDE project is located in:

[`firmware/EE490BMS/`](firmware/EE490BMS/)

Import the project into STM32CubeIDE and build the Debug configuration. The current shared firmware builds with **0 errors and 0 warnings**.

## Remaining Work

The main remaining work is hardware integration and validation. This includes confirming the final seven-cell L9963E mapping, verifying the current shunt value and polarity, testing Coulomb-counting accuracy, validating SoC behavior during charge and discharge, completing the thermistor temperature conversion, and implementing the active-balancing control.

Mechanical work on the battery pack and control enclosure is also still in progress. Current SolidWorks assemblies and exported STL files can be found in the [`hardware/`](hardware/) directory.
