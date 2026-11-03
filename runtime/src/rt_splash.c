/* Écran de chargement : lecture de avi/loading.avi (FFmpeg) pendant le démarrage du jeu, jusqu'à la première
 * présentation (rt_probe_present). L'affichage tourne dans un processus enfant (fenêtre SDL sans bordure, au-dessus
 * des autres, agrandie) : SDL n'est jamais initialisé en même temps dans les deux processus, et le jeu ne partage
 * pas la file d'événements. CT3_SPLASH=0 : désactivé. Sans FFmpeg (CT3_HAVE_FFMPEG=0) : fonctions vides. */

#include "rt.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if CT3_HAVE_FFMPEG
#include <SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SPLASH_SCALE 3
#define SPLASH_MAX_FRAMES 1024

static pid_t splash_pid;

typedef struct {
    uint8_t **px;
    int n, cap, w, h, frame_ms;
} splash_clip;

static int clip_add(splash_clip *k, const uint8_t *src_bgra)
{
    if (k->n >= SPLASH_MAX_FRAMES) return -1;
    if (k->n == k->cap) {
        int cap = k->cap ? 2 * k->cap : 64;
        uint8_t **p = realloc(k->px, sizeof *p * (size_t)cap);
        if (!p) return -1;
        k->px = p;
        k->cap = cap;
    }
    k->px[k->n] = malloc((size_t)k->w * (size_t)k->h * 4);
    if (!k->px[k->n]) return -1;
    memcpy(k->px[k->n], src_bgra, (size_t)k->w * (size_t)k->h * 4);
    k->n++;
    return 0;
}

static void clip_free(splash_clip *k)
{
    for (int i = 0; i < k->n; i++) free(k->px[i]);
    free(k->px);
    k->px = NULL;
    k->n = k->cap = 0;
}

/* Décode toutes les images de la piste vidéo (3 s pour loading.avi) en BGRA, à la taille d'origine. */
static int clip_decode(const char *path, splash_clip *k)
{
    AVFormatContext *fmt = NULL;
    AVCodecContext *dec = NULL;
    AVPacket *pkt = NULL;
    AVFrame *frm = NULL;
    struct SwsContext *sws = NULL;
    uint8_t *bgra = NULL;
    int vs, rc = -1;

    if (avformat_open_input(&fmt, path, NULL, NULL) < 0) return -1;
    if (avformat_find_stream_info(fmt, NULL) < 0) goto out;
    vs = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (vs < 0) goto out;
    {
        AVStream *st = fmt->streams[vs];
        const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
        AVRational r = st->avg_frame_rate.num > 0 ? st->avg_frame_rate : st->r_frame_rate;
        if (!codec) goto out;
        dec = avcodec_alloc_context3(codec);
        if (!dec || avcodec_parameters_to_context(dec, st->codecpar) < 0) goto out;
        if (avcodec_open2(dec, codec, NULL) < 0) goto out;
        k->w = dec->width;
        k->h = dec->height;
        k->frame_ms = (r.num > 0 && r.den > 0) ? (int)(1000 * (int64_t)r.den / r.num) : 33;
        if (k->frame_ms < 10) k->frame_ms = 10;
        if (k->frame_ms > 200) k->frame_ms = 200;
    }
    pkt = av_packet_alloc();
    frm = av_frame_alloc();
    bgra = malloc((size_t)k->w * (size_t)k->h * 4);
    if (!pkt || !frm || !bgra) goto out;

    for (;;) {
        int eof = av_read_frame(fmt, pkt) < 0;
        if (!eof && pkt->stream_index != vs) {
            av_packet_unref(pkt);
            continue;
        }
        if (avcodec_send_packet(dec, eof ? NULL : pkt) < 0 && !eof) {
            av_packet_unref(pkt);
            break;
        }
        if (!eof) av_packet_unref(pkt);
        while (avcodec_receive_frame(dec, frm) == 0) {
            uint8_t *dst[4] = { bgra, NULL, NULL, NULL };
            int dst_stride[4] = { k->w * 4, 0, 0, 0 };
            sws = sws_getCachedContext(sws, frm->width, frm->height, frm->format, k->w, k->h, AV_PIX_FMT_BGRA,
                                       SWS_BILINEAR, NULL, NULL, NULL);
            if (!sws) goto out;
            sws_scale(sws, (const uint8_t *const *)frm->data, frm->linesize, 0, frm->height, dst, dst_stride);
            if (clip_add(k, bgra) < 0) goto done;
            av_frame_unref(frm);
        }
        if (eof) break;
    }
done:
    rc = k->n > 0 ? 0 : -1;

out:
    if (sws) sws_freeContext(sws);
    free(bgra);
    av_frame_free(&frm);
    av_packet_free(&pkt);
    avcodec_free_context(&dec);
    avformat_close_input(&fmt);
    return rc;
}

/* Processus enfant : SDL et la fenêtre appartiennent à ce processus seulement. Ne revient jamais. */
static void splash_child(const char *path)
{
    splash_clip clip = { 0 };
    SDL_Window *win = NULL;

    if (clip_decode(path, &clip) != 0) {
        fprintf(stderr, "rt: écran de chargement : lecture de %s impossible\n", path);
        goto done;
    }
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "rt: écran de chargement : SDL_InitSubSystem : %s\n", SDL_GetError());
        goto done;
    }
    win = SDL_CreateWindow("Crazy Taxi 3", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           clip.w * SPLASH_SCALE, clip.h * SPLASH_SCALE,
                           SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_SKIP_TASKBAR);
    if (!win) goto done;

    {
        int i = 0;
        for (;;) {
            SDL_Surface *dst = SDL_GetWindowSurface(win);
            SDL_Surface *src = SDL_CreateRGBSurfaceFrom(clip.px[i], clip.w, clip.h, 32, clip.w * 4,
                                                        0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
            if (!dst || !src) break;
            SDL_FillRect(dst, NULL, 0);
            SDL_BlitScaled(src, NULL, dst, NULL);
            SDL_UpdateWindowSurface(win);
            SDL_FreeSurface(src);
            SDL_Delay((uint32_t)clip.frame_ms);
            i = (i + 1) % clip.n;
        }
    }

done:
    if (win) SDL_DestroyWindow(win);
    clip_free(&clip);
}

void rt_splash_start(const char *ct3_exe)
{
    const char *off = getenv("CT3_SPLASH");
    char path[4096];
    const char *slash;
    size_t n;
    pid_t pid;

    if (off && strcmp(off, "0") == 0) return;
    if (!ct3_exe) return;
    slash = strrchr(ct3_exe, '/');
    n = slash ? (size_t)(slash - ct3_exe) : 0;
    if (n + 32 >= sizeof path) return;
    snprintf(path, sizeof path, "%.*s/avi/loading.avi", slash ? (int)n : 1, slash ? ct3_exe : ".");
    if (access(path, R_OK) != 0) return;

    fflush(stdout);
    fflush(stderr);
    pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        /* L'enfant disparaît avec le jeu. */
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        splash_child(path);
        _exit(0);
    }
    splash_pid = pid;
}

void rt_splash_stop(void)
{
    if (splash_pid <= 0) return;
    /* SDL intercepte SIGTERM dans l'enfant : SIGKILL est nécessaire. */
    kill(splash_pid, SIGKILL);
    waitpid(splash_pid, NULL, 0);
    splash_pid = 0;
}

#else /* sans FFmpeg */

void rt_splash_start(const char *ct3_exe)
{
    (void)ct3_exe;
}

void rt_splash_stop(void)
{
}

#endif
