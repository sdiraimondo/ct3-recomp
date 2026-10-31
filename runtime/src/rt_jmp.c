#include "rt_jmp.h"

#include <stdlib.h>

#define RT_JMP_MAX 512

typedef struct rt_jmp_frame {
    jmp_buf *jb;
    uint32_t guest_buf;
    uint32_t resume;
    uint32_t esp;
    unsigned depth;   /* profondeur de la pile des appels au moment du setjmp */
} rt_jmp_frame;

/* Propres à chaque thread hôte : chacun a sa pile C. */
static __thread rt_jmp_frame frames[RT_JMP_MAX];
static __thread unsigned frame_count;

/* Appelé juste après le __setjmp3 invité : ESP vaut alors l'ESP de reprise. */
void rt_jmp_register(cpu_t *c, jmp_buf *jb, uint32_t guest_buf, uint32_t resume)
{
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
    /* Cadres morts : ESP plus bas que le cadre courant (ils appartiennent à des appels terminés). */
    unsigned keep = 0;
    for (unsigned i = 0; i < frame_count; i++) {
        if (frames[i].esp >= esp && !(frames[i].guest_buf == guest_buf && frames[i].resume == resume)) {
            frames[keep++] = frames[i];
        }
    }
    frame_count = keep;
    if (frame_count >= RT_JMP_MAX) rt_fatal("trop de cadres setjmp");
    frames[frame_count++] = (rt_jmp_frame){jb, guest_buf, resume, esp, rt_cs_depth};
}

/* Appelé par dispatch avant chaque transfert de contrôle : reprise d'un longjmp invité. */
void rt_jmp_try(cpu_t *c, uint32_t addr)
{
    if (frame_count == 0) return;
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4);
    for (unsigned i = frame_count; i-- > 0;) {
        if (frames[i].resume == addr && frames[i].esp == esp) {
            jmp_buf *jb = frames[i].jb;
            frame_count = i;  /* les cadres au-dessus sont abandonnés */
            rt_cs_depth = frames[i].depth;
            longjmp(*jb, 1);
        }
    }
}
