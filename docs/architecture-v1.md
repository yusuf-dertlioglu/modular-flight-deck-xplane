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
