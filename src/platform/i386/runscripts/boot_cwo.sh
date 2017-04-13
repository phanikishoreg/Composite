#!/bin/sh

cp no_interface.child_wo_hpet.o llboot.o
./cos_linker "llboot.o, ;llpong.o, :" ./gen_client_stub
