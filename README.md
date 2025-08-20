# TrySpace Component Radionstration
This repository radionstrates a component in the TrySpace environment.

## Overview
Command line interface (CLI), flight software (FSW), ground software (GSW), and simulation (SIM) directories are included in this repository.

The radio component is a UART device that is speak when spoken to.
Each command that is successfully interpreted is echoed back.
If additional telemetry is to be generated, it will follow.
The specific command format is as follows:
* uint16, header, 0xC0FF
* uint16, command
  * (0) No operation
  * (1) Get housekeeping
  * (2) Get data
  * (3) Set configuration
* uint16, payload
  * Unused except for set configuration command
* uint16, trailer, 0xFEFE

Response formats:
* Housekeeping
  * uint16, header, 0xC0FF 
  * uint16, command counter
  * uint16, configuration
  * uint16, trailer, 0xFEFE
* Data
  * uint16, header, 0xC0FF
  * uint16, data channel 1
  * uint16, data channel 2
  * uint16, data channel 3
  * uint16, trailer, 0xFEFE

### Command Line Interface
The CLI can be configured to connect to either the hardware or simulation.
This enables direct checkouts these without interference.

### Flight Software
The core Flight System (cFS) flight software application receives commands from the software bus.
Two message IDs exist for commands:
* 0x18FC - Commands
  * (0) No operation
  * (1) Reset counters
  * (2) Enable
  * (3) Disable
* 0x18FD - Requests
  * (0) Request housekeeping
  * (1) Request data point

Two message IDs exist for telemetry:
* 0x08FC - Application Housekeeping
* 0x08FD - Component Telemetry

### Ground Software
The XTCE file provided details the CCSDS Space Packet Protocol format used for commanding and telemetry.

### Simulation
The simulation builds as both a standalone executable that would connect to simulith as the external time driver and as a library for the tryspace-director to load.
