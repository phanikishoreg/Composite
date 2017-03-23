#!/bin/sh

cp scheddev.$1.o llboot.o
./cos_linker "llboot.o, ;llpong.o, :" ./gen_client_stub
