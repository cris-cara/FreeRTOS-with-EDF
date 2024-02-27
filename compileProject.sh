#!/bin/bash

cd  CORTEX_M3/build/gcc
make clean
make -j4
qemu-system-arm -machine mps2-an385 -cpu cortex-m3 -smp cores=1 -kernel output/project.out -monitor none -nographic -serial stdio
