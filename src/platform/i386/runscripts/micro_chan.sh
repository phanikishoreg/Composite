#!/bin/sh

cp micro_chan.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub
