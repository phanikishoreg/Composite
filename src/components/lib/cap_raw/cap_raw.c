#include <capmgr.h>

/* Need to implement some bookkeeping data structures and perhaps an init API..to init child structures for most API here. */
thdcap_t
capmgr_initthd_create(spdid_t child, thdid_t *tid)
{
	assert(0);

	return 0;
}

thdcap_t
capmgr_initaep_create(spdid_t child, struct cos_aep_info *aep, int owntc, cos_aepkey_t key, asndcap_t *sndret)
{
	assert(0);

	return 0;
}

thdcap_t
capmgr_thd_create(cos_thd_fn_t fn, void *data, thdid_t *tid)
{
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	thdcap_t thd;

	thd = cos_thd_alloc(ci, ci->comp_cap, fn, data);
	if (!thd) return 0;

	*tid = cos_introspect(ci, thd, THD_GET_TID);

	return thd;
}

thdcap_t
capmgr_aep_create(struct cos_aep_info *a, cos_aepthd_fn_t fn, void *data, int owntc, cos_aepkey_t key)
{
	assert(0);

	return 0;
}

thdcap_t
capmgr_thd_create_ext(spdid_t child, thdclosure_index_t idx, thdid_t *tid)
{
	assert(0);

	return 0;
}

thdcap_t
capmgr_aep_create_ext(spdid_t child, struct cos_aep_info *a, thdclosure_index_t idx, int owntc, cos_aepkey_t key, arcvcap_t *extrcv)
{
	assert(0);

	return 0;
}

thdcap_t
capmgr_thd_retrieve(spdid_t child, thdid_t t)
{
	assert(0);

	return 0;
}

thdcap_t
capmgr_thd_retrieve_next(spdid_t child, thdid_t *tid)
{
	assert(0);

	return 0;
}

asndcap_t
capmgr_asnd_create(spdid_t child, thdid_t t)
{
	assert(0);

	return 0;
}

asndcap_t
capmgr_asnd_key_create(cos_aepkey_t key)
{
	assert(0);

	return 0;
}

