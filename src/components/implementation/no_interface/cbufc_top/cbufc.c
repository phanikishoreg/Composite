#include <cos_component.h>
#include <print.h>

#include <periodic_wake.h>
#include <sched.h>
#include <timed_blk.h>
#include <cbuf_mid.h>

#define PERIODIC 100

void cos_init(void)
{

	printc("\n****** TOP: thread %d in spd %ld ******\n",cos_get_thd_id(), cos_spd_id());

	periodic_wake_create(cos_spd_id(), PERIODIC);

	int k = 0;
	int th;
	
	while(1){
		k++;
		th = cos_get_thd_id();

		cbuf_call('a');
		periodic_wake_wait(cos_spd_id());
	}

	return;

}
