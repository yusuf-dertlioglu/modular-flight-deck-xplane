# System Architecture v1.0

This document describes the hardware topology, power distribution, and dual-core RP2040 firmware architecture of the **v1.0** Modular Flight Deck.

---

## Hardware Architecture & Signal Routing

The system is built around a central controller PCB hosting a **Raspberry Pi Pico (RP2040)**, connected to three modular satellite PCBs via wired harnesses.

### Mermaid System Overview

```mermaid
flowchart TB
    subgraph Central["Central Board (RP2040)"]
        PICO["RP2040 (Pico)"]
        LDO_DISP["3.3V LDO (Display)"]
        LDO_SYS["3.3V LDO (Satellites & Peripherals)"]
        DISP["TFT Display (ILI9488, SPI0)"]
        TOUCH["Touch Controller (HR2046, SPI0)"]
        ENC["Rotary Encoder + Pushbutton"]
    end

    subgraph CenterSat["Center Satellite (7-Pin Cable)"]
        MUX["CD74HC4067 (16-Ch Analog/Digital Mux)"]
        C_IN["10 Buttons/Switches + 2 Analog Axes (Camera)"]
    end

    subgraph LeftSat["Left Satellite (6-Pin Cable)"]
        ADC_L["MCP3208 12-Bit ADC (SPI1)"]
        L_IN["Throttle, Mixture, Rudder + Brake Button"]
    end

    subgraph RightSat["Right Satellite (6-Pin Cable)"]
        ADC_R["MCP3208 12-Bit ADC (SPI1)"]
        R_IN["2-Axis Industrial Joystick (Pitch / Roll)"]
    end

    USB["USB Host (PC / X-Plane 11 via Python UDP Bridge)"]

    PICO <-->|"Native USB (HID + CDC)"| USB
    LDO_DISP -->|"3.3V"| DISP
    LDO_SYS -->|"3.3V, GND"| CenterSat
    LDO_SYS -->|"3.3V, GND"| LeftSat
    LDO_SYS -->|"3.3V, GND"| RightSat

    PICO <-->|"SPI0"| DISP
    PICO <-->|"SPI0"| TOUCH
    PICO <-->|"GPIO + Pushbutton"| ENC

    C_IN --> MUX
    C_IN --> MUX
    MUX -->|"4x Channel Select (GPIO)"| PICO
    MUX -->|"Analog Out (ADC Pin)"| PICO

    L_IN --> ADC_L
    R_IN --> ADC_R
    PICO <-->|"SPI1 + CS"| ADC_L
    PICO <-->|"SPI1 + CS"| ADC_R
end


## Central Board Components

- MCU: Raspberry Pi Pico (RP2040) running C firmware built on the Pico SDK.
- Display & Touch: Resistive TFT display driven via an ILI9488 controller over SPI0, paired with an HR2046 touch controller over SPI0.
- Rotary Encoder: Hardware rotary encoder with integrated pushbutton directly on the central board, used for adjusting autopilot target values and UI interaction.
- Power Supply (Dual 3.3V LDOs): Two separate 3.3V LDO regulators on the central board—one dedicated to powering the display and a second dedicated to supplying the three satellite boards (and remaining peripherals).

## Satellite Boards & Cabling

- Center Satellite Board (7-pin cable): Houses 10 buttons/switches (including camera zoom, assignable buttons, landing gear, parking brake, and (MOM)-OFF-(MOM) flaps toggle) and 2 analog signals (mini-joystick). Connected via a CD74HC4067 16-channel multiplexer using 4 channel-select lines, 3.3V (from the shared LDO), GND, and 1 signal line routed to an RP2040 ADC input.
- Left Satellite Board (6-pin cable): Houses dual sliders (Throttle, Mixture), horizontal slider (Rudder), and the brake pushbutton. Uses an MCP3208 ADC communicating over SPI1 (plus 3.3V, GND, and chip-select/SPI1 lines).
- Right Satellite Board (6-pin cable): Houses the 2-axis industrial joystick (Pitch/Roll) with an MCP3208 ADC communicating over SPI1 (plus 3.3V, GND, and chip-select/SPI1 lines).

## Dual-Core Firmware Architecture (RP2040)

The RP2040 runs both cores concurrently to separate time-critical I/O and USB communication from display rendering and UI handling:

### Core 0 — Input Acquisition, State Sync & USB Bridge

- Polls/reads inputs from buttons, switches, and analog channels across the CD74HC4067 multiplexer and both MCP3208 ADCs (SPI1).
- Synchronizes shared state/values with Core 1 using a `multicore_fifo` sync handshake (`FIFO_CMD_SYNC` / `FIFO_ACK_SYNC`) together with dedicated mutexes.
- Prepares and transmits native USB HID reports for flight controls.
- Exchanges bidirectional telemetry and autopilot state/target data over USB CDC with the external Python UDP bridge for X-Plane 11.

### Core 1 — Display Rendering, Touch & Encoder

- Drives the ILI9488 display and runs LVGL UI updates.
- Processes HR2046 touch input (SPI0) and rotary encoder / pushbutton events.
- Synchronizes UI/target state with Core 0 via `sync_with_core0()` and mutexes.

## Synchronization, Mutexes & Bus Notes

- Telemetry Mutex: Protects incoming telemetry data received by Core 0 over USB CDC and read by Core 1 for display updates.
- Autopilot Mutex: Protects autopilot target values/states modified by Core 1 (via touch and rotary encoder) and sent by Core 0 over USB to the host/X-Plane 11.
- SPI0 Mutex: Serializes access to the shared SPI0 bus between the ILI9488 display and the HR2046 touch controller so display transfers and touch reads do not collide.
- SPI1 & MCP3208 Polling: Both MCP3208 ADCs are polled exclusively by Core 0 over SPI1 and do not require an SPI1 bus mutex while Core 1 does not access SPI1.
- Multicore FIFO Sync: Core 0 pushes `FIFO_CMD_SYNC` via `multicore_fifo_push_blocking()` and waits for `FIFO_ACK_SYNC` (`multicore_fifo_pop_blocking()`) while Core 1 waits for `FIFO_CMD_SYNC` in `sync_with_core0()` and acknowledges it—keeping both cores aligned on shared data updates.
