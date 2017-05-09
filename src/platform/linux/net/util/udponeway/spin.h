#ifndef SPIN_H
#define SPIN_H

#define ITERS_SPIN (500) //1usecs on my test machine. lets see.
#define CYCS_PER_USEC (3393)

typedef unsigned long long u64_t;
typedef unsigned long long cycles_t;

extern u64_t cycs_per_spin_iters;
extern u64_t usecs_per_spin_iters;
extern void spin_calib(void);
extern void spin_usecs(cycles_t usecs);
extern void spin_cycles(cycles_t cycs);
extern void spin_std_iters(void);
extern void spin_iters(unsigned int iters);

#endif /* SPIN_H */
