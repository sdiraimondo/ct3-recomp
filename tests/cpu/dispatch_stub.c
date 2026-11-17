/* test_cpu ne charge aucun code traduit. Ce symbole satisfait les références de
 * ct3_rt (rt_shims.c, rt_thread.c) sans fournir de dispatch réel : aucun appel
 * ne doit l'atteindre. */
#include "rt.h"

void dispatch(cpu_t *c, uint32_t addr)
{
    (void)c;
    (void)addr;
    rt_fatal("dispatch appelé sans code traduit");
}
