# Table of Contents
- [Description](#description)
- [Requirements](#requirements)
- [How to Run](#how-to-run)

# Description
In this branch it has been developed the the comparision between the Earliest Deadline First (EDF) scheduler with the default scheduling of the FreeRTOS (Fixed Priority Pre-emptive Scheduling with Time Slicing).

In particulary it has been shown that the EDF is an optimal algorithm so it does not cross the deadlines whereas the default scheduling, in some cases, could cross the deadlines.

# Requirements
To properly execute this project, be sure to meet the following requirements:
- **OS**:
    - Windows (32-bit or 64 bit)
    - macOS
    - Linux (Arch, Debian/Ubuntu, Fedora, Gentoo, RHEL/CentOS, SUSE)
- **QEMU**: You must have QEMU installed on your system. You can download the latest version from [official QEMU website](https://www.qemu.org/download/).
- [**arm-none-eabi-gcc compiler**](https://developer.arm.com/downloads/-/gnu-rm) and **GNU make utility** must be installed on your system

# How to Run
## for Linux enviroment
Exclusively the first time, in order to install QEMU and all of its dependecies, run the following command (from the project directory):
```shell
./install.sh
```

Then, whenever you want to compile and start the QEMU emulator to run the code, simply open a terminal, navigate to the project directory, and run the following command:
```shell
./compileProject.sh
```

## for Windows/MacOS
Start QEMU with the following command line, replacing **[path-to]** with the correct path to the project.out file generated by the GCC build.
```shell
qemu-system-arm -machine mps2-an385 -cpu cortex-m3 -kernel [path-to]/project.out -monitor none -nographic -serial stdio
```

For more info, please visit the [**official page of FreeRTOS on ARM Cortex-M3 with QEMU**](https://www.freertos.org/freertos-on-qemu-mps2-an385-model.html)
