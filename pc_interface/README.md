# LabVIEW PC Interface

This directory contains the LabVIEW PC interface developed for the Battery Management System.

## COM to Graph.vi

`COM to Graph.vi` receives serial telemetry transmitted by the STM32 over UART and provides real-time visualization of battery measurements and system status.

The interface was used during system integration, testing, and the final project demonstration to monitor data from the completed BMS, including:

- Pack voltage
- Individual cell-group voltages
- Battery temperatures
- Pack current
- Fault and system-status information

The STM32 telemetry interface operates over USART2 at 115200 baud.
