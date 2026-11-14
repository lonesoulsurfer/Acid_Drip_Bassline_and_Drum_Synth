# Licensing

Acid Drip has two licences: one for the firmware and one for the hardware.

## Firmware: GPL-3.0

The firmware is the code in the `src/` folder.

- Licence: GNU General Public License v3.0. See [LICENSE](LICENSE).
- Copyright (c) 2026 Marcus (lonesoulsurfer).
- The acid synth engine is adapted from [Badass Bass](https://github.com/Tschibo00/BadAssbasSSynth) by Tschibo00. Badass Bass uses GPL-3.0. Thus the Acid Drip firmware must also use GPL-3.0.

You can use, change and share the firmware. If you share a changed version, you must also share its source code under GPL-3.0.

## Hardware: CERN-OHL-S-2.0

The hardware is the PCB files, the front panel files, the Gerber files and the schematic.

- Licence: CERN Open Hardware Licence Version 2, Strongly Reciprocal. See [LICENSE-HARDWARE](LICENSE-HARDWARE).
- Copyright (c) 2026 Marcus (lonesoulsurfer).

You can make, change and sell boards from these files. If you share or sell a changed design, you must also share the changed design files under the same licence.

## Code from other projects

### DrumKid

The drum engine is adapted from [DrumKid](https://github.com/mattybrad/drumkid). DrumKid uses the MIT License. The MIT License lets other projects use DrumKid code, and it is compatible with GPL-3.0. The MIT License requires this notice:

```
MIT License

Copyright (c) 2019 Matt Bradshaw

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### Libraries

This repository does not include a copy of these libraries. The Arduino Library Manager installs them.

| Library | Licence |
|---------|---------|
| [Mozzi](https://github.com/sensorium/Mozzi) (version 2) | LGPL-2.1 |
| [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library) | BSD |
| [Adafruit ILI9341](https://github.com/adafruit/Adafruit_ILI9341) | MIT |
| [Arduino-Pico core](https://github.com/earlephilhower/arduino-pico) | LGPL-2.1 |

The `.uf2` files in the releases include compiled code from these libraries.
