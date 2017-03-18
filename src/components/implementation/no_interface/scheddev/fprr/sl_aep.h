#ifndef SL_AEP_H
#define SL_AEP_H

#include <cos_defkernel_api.h>

struct cos_aep_info *sl_aep_alloc(void);
void sl_aep_free(struct cos_aep_info *);
void sl_aep_init(void);

#endif /* SL_AEP_H */
