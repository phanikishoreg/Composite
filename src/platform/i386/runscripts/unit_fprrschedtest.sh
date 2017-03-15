#!/bin/sh

cp scheddev.fprr.o llboot.o
./cos_linker "llboot.o, ;llpong.o, :" ./gen_client_stub
