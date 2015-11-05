#include "shared/cos_config.h"
#ifdef COS_ASSERTIONS_ACTIVE
#define COS_DEBUG
#endif

#ifndef unlikely
#define unlikely(x)     __builtin_expect(!!(x), 0)
#endif

#ifndef likely
#define likely(x)       __builtin_expect(!!(x), 1)
#endif

#ifndef die
__attribute__ ((noreturn)) static inline void __kern_noret(void) { while (1) ; }
#define die(fmt, ...) do {                 \
    printk(fmt, ##__VA_ARGS__);            \
    (*(int *)0) = 0;                       \
    __kern_noret();                        \
} while (0)
#endif

#ifdef COS_DEBUG
#define printd(str,args...) printk(str, ## args)
#define assert(node) do { \
	if(unlikely(!(node))) die("%s:%s:%d - Assertion '%s' failed\n", __FILE__, __func__, __LINE__, #node); \
} while (0)
#else
#define assert(a) (void)0
#define printd(str,args...) 
#endif
