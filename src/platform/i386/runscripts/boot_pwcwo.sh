#!/bin/sh

cp no_interface.parent_w_hpet.o llboot.o
./cos_linker "llboot.o, ;no_interface.child_wo_hpet.o, :" ./gen_client_stub
