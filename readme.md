## persistence of firmware

Code + schematics + layout + documentation for my persistence of vision PCB business card. First revision of the board was sent to OSHPark on July 15th and is expected back on the 28th. Parts were ordered from Digikey on the 15th as well.

```
/design -- eagle board and schematic files (.brd, .sch)
    /cam -- cam files (usually not up to date with .brd file)
/documents
    /datasheets -- datasheet documents for every major component (.pdf)
    /misc -- current bom and parts order lists from Digikey
    -- various text files containing unorganized thoughts
/firmware -- MPLAB X Project file and firmware source
```

### rev0
Board uses a PIC18F2XK20, and includes a 3-axis accelerometer (LIS3DSH) and ambient light sensor (APDS-9008). The accelerometer will be used to detect the edges of the shaking motion to time out each LED column, and the light sensor will be used to program new messages into the card using a webapp that flashes the screen. The board has a on-off switch, and a switch to put it into programming mode. Further modes may be selectable from the webapp. The board is powered from two CR2032s (rev0) in parallel at 3.0V each. A boost converter (MCP1640) steps the voltage up to 3.3V.

### form factor goals
Most of the PCB business cards I've seen have been clunky, and not very likely to be put inside a wallet, so I wanted something that would be completely flush (no compoents or batteries protruding from the top or the bottom of the board).

The final card will be a stackup of two PCBs. The bottom PCB will be 0.6mm thick, and will have all of the components mounted on it (except for the light sensor and it's supporting components). The top PCB will be 1.0mm thick, and will have cutouts in the same locations as the components on the bottom board. This way the two boards will lay on top of one another and no components will protrude from the surface of the card. All components were chosen to be less than 1.0mm thick.

CR2016 batteries are exactly 1.6mm thick. Holes will be cut through both boards in the same location, and two CR2016s will be placed inside. This way, the batteries themselves will be flush with the board. Power connections will be made between the top and bottom PCB, since one side of each battery will be connected to the bottom board.

The light sensor will be placed on the bottom of the 1.0mm board, and have a corresponding hole in the 0.6mm board. This way the LEDs can be seen as progress indicators when the board is placed flat, bottom down on a display.

The first revision has none of these features. It was built as a quick proof of concept so I can work on the firmware while I'm working on the real version.
