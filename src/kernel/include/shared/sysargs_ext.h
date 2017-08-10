#ifndef SYSARGS_EXT_H
#define SYSARGS_EXT_H

struct sysargs_ext {
	unsigned long r1, r2, r3, r4;
};

#define SETGET_EXT_R(n) \
static inline unsigned long \
sysargs_ext_get_r##n(struct sysargs_ext *args) \
{ return args->r##n; } \
static inline void \
sysargs_ext_set_r##n(struct sysargs_ext *args, unsigned long val) \
{ args->r##n = val; }

SETGET_EXT_R(1);
SETGET_EXT_R(2);
SETGET_EXT_R(3);
SETGET_EXT_R(4);

#endif /* SYSARGS_EXT_H */
