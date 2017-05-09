all: clean lib client

client: spin.h udp_sender1w.c
	gcc udp_sender1w.c -o udp_sender1w -L. -lspin -g -O3

lib: spin.h spin.c
	gcc spin.c -O3 --shared -o libspin.so.0
	ln -s libspin.so.0 libspin.so

clean: cleanlib
	rm -f udp_sender1w

cleanlib:
	rm -f libspin.so*
