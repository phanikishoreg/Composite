C_OBJS=omp_fft.o
ASM_OBJS=
COMPONENT=fft.o
INTERFACES=
DEPENDENCIES=par_mgr mem_mgr_large sched valloc printc timed_blk lock
IF_LIB=/usr/lib/libm.a

include ../../Makefile.subsubdir

CFLAGS += -fopenmp
CFLAGS += -Wl,-Bstatic -lm -Wl,-Bdynamic #FIXME: this doesn't work. use the IF_LIB for now. 

