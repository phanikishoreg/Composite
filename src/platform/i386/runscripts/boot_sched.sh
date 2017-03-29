#!/bin/sh

cp no_interface.llscheddev.o llboot.o
./cos_linker "llboot.o, ;no_interface.hlscheddev.o, :" ./gen_client_stub
