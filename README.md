# 615-labs-SAO
Science of debatable sanity

- 35mm x 50mm
- 4x green back-emitting SMD LEDs, rear mounted, glow through bare PCB spot
- microcontroller inc programming pads
- dropper resistors
- SAO connector, rear mounted
- dithered green soldermask, with white silkscreen?

## process
- KiCad
  - create project
  - create schematic
    - place simple devices: (1x) LED, R, C
      rough calculation: *if* LED forward voltage drop is (typical) 1.6V, resistor needs to drop 3.3V-1.6V = 1.7V
      *if* LED forward current is (typical) 20mA, V=IR ... 1.7 = 20e-3 x R ... R=85
      but LEDs produce more light per watt at higher currents, and we can PWM it down arbitrarily, so drop Rto about 20
      E12 series: 22

- place SAO connector and CPU on schematic, wire together

- browse [JLCPCB's assembly parts library](https://jlcpcb.com/parts)
  - Optoelectronic / LED Discrete
    - checkboxes: In-stock, Economic PCBA, Datasheet available
    - filters: color (control-click each variant containing "Green" and no comma), mounting Reverse
    - checkbox Parts Type Basic not even selectable (because reverse-emitting)
    - Apply
    - sort by price ascending
      - [cheapest](https://jlcpcb.com/partdetail/C7496852) at 1.14 cents in qty 1, and they have 3k in stock
      - [second cheapest](https://jlcpcb.com/partdetail/XINGLIGHT-XL_3216UGCFB/C3646937) at 1.42 cents in qty 1, and they have over 700k in stock
        but its forward voltage Vf might possibly sneak over 3.3V so it's not a viable option; back to the cheaper one
    - Add to My Part Lib for Assembly
    - grab datasheet
    - forward voltage drop Vf is 2.4 .. 2.7 - current thus 0.6/22 .. 0.9/22 = 27 .. 41 mA - almost inside absolute max pulse current - fuck it we ride
    - make new schematic symbol and PCB footprint based off existing LED and 1206

  - Resistors / Chip surface mount
    - checkboxes: In-stock, Basic, Economic PCBA, (don't care about Datasheet here)
    - filters: resistance: 21 to 23 ohm - select 22
    - Apply
    - sort by price ascending
      - [cheapest](https://jlcpcb.com/partdetail/25835-0402WGF220JTCE/C25092) at 0.07 cents in qty 1
    - Add to My Part Lib for Assembly
    - package is 0402 - update schematic and thus PCB

  - Connectors / Pin headers
    - use same as already selected for doom-floppy-SAO

  - Capacitors / Multi-Layer Ceramic SMD
    - checkboxes: In-stock, Basic, Economic PCBA, (don't care about Datasheet here)
    - filters: capacitance: 1 to 10 uF - browse through
    - Apply
    - sort by price ascending
      - [choice - highest value under 1c each](https://jlcpcb.com/partdetail/24366-CL10A225KO8NNNC/C23630) at 0.83 cents in qty 1
    - Add to My Part Lib for Assembly
    - package is 0603 - update schematic and thus PCB

- on layer Edge Cuts, place rectangle; edit, start point 50,50; size 35x50

- in schematic
  - make 7 copies of R and LED
  - for each GPIO pin, right-click and select function - if there's a TIMn_CHm, select it (that's PWM outputs, primary pin assignment)
  - one pin has function SWIO - select it and wire to SAO GPIO1 pin - program by connecting WCH-LinkE to SAO connector
  - wire from each TIMnCHm to one R and thus through LED to GND (doesn't matter which - will reallocate based on board routing)

- from schematic, Tools / Update PCB
