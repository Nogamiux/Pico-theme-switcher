# Pico Theme Switcher

A simple theme switcher for Pico Launcher, written entirely in C (using devkitARM and libnds).

This Nintendo DS homebrew application allows you to view the themes installed in the `/_pico/themes/` folder on your SD card, letting you select one via a convenient dual-screen graphical interface. Once selected, the program will automatically update the `/_pico/settings.json` configuration file with the chosen theme, ready to be displayed upon the next PicoLauncher reboot.

---

## Build Instructions

Requires [devkitARM](https://devkitpro.org/wiki/Getting_Started) and `libnds`.

1. Place a 32x32 indexed `.bmp` image named `icon.bmp` in the project root.
2. Run `make`.

The output `pico-theme-switcher.nds` can be copied directly to your flashcart.

---

## Credits and Acknowledgements

A special thanks to the **LNH team** for the development of the **DSPico** flashcart and all its fantastic components. Thank you for your invaluable work on the hardware and software that makes projects like this possible!
