# Software Build Guide

You want to tweak the software a lil bit on your own? Here's the guide.

Project is build with PlatformIO framework.

- Install PlatformIO extension in VSCode
- PIO Home > Platforms > Download and install "Espressif 32" **only 4.3.0, it's not default version!**
- Download submodules `git submodule update --init` </br> NimBLE repo in {project}/app/ble/esp32-nimble-cpp) should be downloaded. If not use `git submodule add https://github.com/h2zero/esp-nimble-cpp`

Off you go! In case of errors - clean and build.