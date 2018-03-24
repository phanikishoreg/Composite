#!/bin/sh

cp llboot_comp.o llboot.o
./cos_linker "llboot.o, ;*unitrawcapmgr.o, :" ./gen_client_stub
