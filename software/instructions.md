# 615 Labs Erlenmeyer SAO — Build and Flash Guide

This guide documents the known-working Windows workflow for compiling and flashing the 615 Labs Erlenmeyer SAO.

## Hardware

- **MCU:** WCH `CH32V003F4U6`
- **Package:** QFN-20
- **Programmer:** WCH-Link / WCH-LinkE
- **Programming interface:** 3.3 V, GND, and SWDIO/SWIO

### Programmer wiring

| WCH-Link | SAO |
|---|---|
| 3.3 V | 3.3 V |
| GND | GND |
| SWDIO / SWIO | SWDIO / SWIO |

Do not connect 5 V to the SAO programming interface.

---

## 1. Install Arduino IDE

Download and install Arduino IDE 2:

<https://www.arduino.cc/en/software>

Open Arduino IDE after installation.

---

## 2. Install the OpenWCH board package

In Arduino IDE:

1. Open **File → Preferences**.
2. Add this URL under **Additional Boards Manager URLs**:

```text
https://github.com/openwch/board_manager_files/raw/main/package_ch32v_index.json
```

3. Open **Tools → Board → Boards Manager**.
4. Search for:

```text
wch
```

5. Install **CH32 MCU EVT Boards**.
6. This project was proven with **version 1.0.4**.

### Board selection

Select the OpenWCH CH32V003 target:

```text
CH32V003F4P
```

The physical SAO uses the `CH32V003F4U6` QFN-20 package. The firmware directly addresses the confirmed GPIO ports and pins.

---

## 3. Install WCH-LinkUtility

Download the official Windows utility:

<https://www.wch.cn/downloads/WCH-LinkUtility_ZIP.html>

1. Extract the ZIP.
2. Run the included WCH-Link driver installer as Administrator if Windows does not recognize the programmer:

```text
WCHLinkDrv_WHQL_S.exe
```

3. Open `WCH-LinkUtility`.
4. Connect the WCH-Link programmer.
5. Confirm the programmer is in **RISC-V / LinkRV** mode.

---

## 4. Confirm the MCU connection

Connect the SAO to the programmer using:

```text
3.3 V
GND
SWDIO
```

In WCH-LinkUtility:

1. Use automatic chip detection.
2. Confirm that the device is detected as the **CH32V003 family**.
3. Do not continue if the utility cannot identify the MCU.

Known-good readout:

```text
MCU family: CH32V003
Flash: 16 KB
```

---

## 5. Required firmware initialization

LED3 uses `PA1`. On this MCU, PA1 may remain assigned to the oscillator function unless the remap is explicitly disabled.

The firmware must:

1. Enable the AFIO and GPIO clocks.
2. Disable the PA1/PA2 oscillator remap.
3. Configure all LED pins as push-pull outputs.

Known-working remap function:

```cpp
void disablePA1PA2OscillatorRemap()
{
#if defined(GPIO_Remap_PA1_2)

  GPIO_PinRemapConfig(GPIO_Remap_PA1_2, DISABLE);

#elif defined(GPIO_Remap_PA1PA2)

  GPIO_PinRemapConfig(GPIO_Remap_PA1PA2, DISABLE);

#else

  AFIO->PCFR1 &= ~((uint32_t)0x00008000);

#endif
}
```

The setup must enable AFIO before calling that function:

```cpp
RCC_APB2PeriphClockCmd(
  RCC_APB2Periph_AFIO |
  RCC_APB2Periph_GPIOA |
  RCC_APB2Periph_GPIOC |
  RCC_APB2Periph_GPIOD,
  ENABLE
);

disablePA1PA2OscillatorRemap();
```

Without this step, LED3 may remain dark even when the PCB trace, resistor, LED, and solder joints are good.

---

## 6. Confirmed LED mapping

| LED | QFN pad | GPIO |
|---|---:|---|
| LED1 | 1 | `PD7` |
| LED2 | 18 | `PD4` |
| LED3 | 2 | `PA1` |
| LED4 | 17 | `PD3` |
| LED5 | 11 | `PC4` |
| LED6 | 16 | `PD2` |
| LED7 | 10 | `PC3` |
| LED8 | 7 | `PC0` |

### Animation groups

```text
Bubbles: LED4 → LED3 → LED2 → LED1
Liquid:  LED5, LED6, LED7, LED8
```

---

## 7. Compile the firmware

Open the `.ino` firmware in Arduino IDE.

Click:

```text
Sketch → Verify/Compile
```

A successful build should report usage below:

```text
Maximum program storage: 16384 bytes
Maximum dynamic memory: 2048 bytes
```

The Arduino console may still fail during its own upload stage. The known-working process is:

```text
Arduino IDE = compile/export
WCH-LinkUtility = flash
```

---

## 8. Export the compiled binary

In Arduino IDE, click:

```text
Sketch → Export Compiled Binary
```

Arduino places the compiled binary in the sketch folder.

Use:

```text
<sketch-name>.ino.bin
```

Do not select a bootloader image if more than one file is generated.

---

## 9. Flash with WCH-LinkUtility

1. Open WCH-LinkUtility.
2. Connect the WCH-Link to the computer.
3. Connect the SAO using 3.3 V, GND, and SWDIO.
4. Confirm the utility detects the CH32V003.
5. Select the exported:

```text
<sketch-name>.ino.bin
```

6. Run the program/download operation.
7. Confirm successful:
   - Erase
   - Program
   - Verify
   - Reset
8. Disconnect the programmer.
9. Power-cycle the SAO from a normal SAO badge connection.

The new animation should begin after reset.

---

## Troubleshooting

### Arduino compiles but upload fails

Example:

```text
WCH-Link failed to connect with riscvchip
Failed uploading
```

Use Arduino only to export the `.ino.bin`, then flash it through WCH-LinkUtility.

### WCH-LinkUtility cannot detect the programmer

- Install `WCHLinkDrv_WHQL_S.exe`.
- Reconnect the WCH-Link.
- Confirm **RISC-V / LinkRV** mode.
- Check Device Manager for the WCH-Link device.

### WCH-LinkUtility detects the programmer but not the MCU

Check:

```text
3.3 V → 3.3 V
GND   → GND
SWDIO → SWDIO
```

Also inspect the adapter orientation and continuity.

### LED3 does not work

Confirm the firmware includes:

```cpp
disablePA1PA2OscillatorRemap();
```

and that the AFIO clock is enabled before the call.

### Flash succeeds but the old animation still runs

- Confirm the newly exported `.ino.bin` is selected.
- Check the file timestamp.
- Program and verify again.
- Disconnect the programmer and fully power-cycle the SAO.

---

## Known-working workflow summary

```text
Install Arduino IDE
        ↓
Install OpenWCH board package
        ↓
Install WCH-LinkUtility and driver
        ↓
Compile firmware in Arduino
        ↓
Export <sketch>.ino.bin
        ↓
Connect 3.3 V, GND, and SWDIO
        ↓
Detect CH32V003 in WCH-LinkUtility
        ↓
Program + verify + reset
        ↓
Disconnect programmer and power-cycle SAO
```
