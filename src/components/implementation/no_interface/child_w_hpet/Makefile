#C_OBJS=unit_schedlib.o
COMPONENT=test_hlsched.o
INTERFACES=
DEPENDENCIES=
IF_LIB=
ADDITIONAL_LIBS=-lcos_defkernel_api -lcos_kernel_api -lheap

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o

CFLAGS += -DSL_DEBUG_DEADLINES -DSL_DEBUG_DL_MISS_DIAG_USEC=1000000
#CFLAGS += -DSL_DEBUG
