#ifndef SYSCALL_ARGS_H
#define SYSCALL_ARGS_H

struct sysargs {
	unsigned long r1, r2, r3, r4, r5, r6;
};

#define SETGET_R(n) \
static inline unsigned long \
sysargs_get_r##n(struct sysargs *args) \
{ return args->r##n; } \
static inline void \
sysargs_set_r##n(struct sysargs *args, unsigned long val) \
{ args->r##n = val; }

SETGET_R(1);
SETGET_R(2);
SETGET_R(3);
SETGET_R(4);
SETGET_R(5);
SETGET_R(6);

#endif /* SYSCALL_ARGS_H */
