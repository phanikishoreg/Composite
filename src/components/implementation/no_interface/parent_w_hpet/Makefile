#C_OBJS=sl_aep.o sl.o sl_mod_fpds.o sl_thd_static_backend.o sl_timer_mod_periodic.o test_llscheddev.o llbooter_sched.o 
COMPONENT=test_llsched.o
INTERFACES=
DEPENDENCIES=
IF_LIB=
ADDITIONAL_LIBS=-lcobj_format -lcos_defkernel_api -lcos_kernel_api -lheap

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o

CFLAGS += -DSL_DEBUG_DEADLINES -DSL_DEBUG_DL_MISS_DIAG_USEC=1000000
#CFLAGS += -DSL_DEBUG
