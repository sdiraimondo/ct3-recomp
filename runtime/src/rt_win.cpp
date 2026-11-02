// Fenêtres et messages Win32 (user32), sur SDL2. Voir runtime/include/rt_win.h.

#include "rt_win.h"
#include "rt_proc.h"
#include "rt_shim.h"
#include "rt_thread.h"

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>

static void dik_set(int sc, bool down);
#include <strings.h>
#include <vector>

namespace {

void pad_test_attach();
void pad_load_config();

constexpr uint32_t kWmCreate = 0x0001;
constexpr uint32_t kWmDestroy = 0x0002;
constexpr uint32_t kWmMove = 0x0003;
constexpr uint32_t kWmSize = 0x0005;
constexpr uint32_t kWmActivate = 0x0006;
constexpr uint32_t kWmClose = 0x0010;
constexpr uint32_t kWmQuit = 0x0012;
constexpr uint32_t kWmNcCreate = 0x0081;
constexpr uint32_t kWmKeyDown = 0x0100;
constexpr uint32_t kWmKeyUp = 0x0101;
constexpr uint32_t kWmChar = 0x0102;
constexpr uint32_t kWmSysKeyDown = 0x0104;
constexpr uint32_t kWmSysKeyUp = 0x0105;
constexpr uint32_t kWmMouseMove = 0x0200;
constexpr uint32_t kWmLButtonDown = 0x0201;
constexpr uint32_t kWmLButtonUp = 0x0202;
constexpr uint32_t kWmRButtonDown = 0x0204;
constexpr uint32_t kWmRButtonUp = 0x0205;
constexpr uint32_t kWmMButtonDown = 0x0207;
constexpr uint32_t kWmMButtonUp = 0x0208;

constexpr uint32_t kWsPopup = 0x80000000u;
constexpr uint32_t kWsVisible = 0x10000000u;
constexpr uint32_t kCwUseDefault = 0x80000000u;

constexpr uint32_t kHwndBase = 0x00010000u;
constexpr uint32_t kFakeIcon = 0x00020001u;
constexpr uint32_t kFakeDc = 0x00030001u;

struct wndclass {
    uint32_t wndproc;
    uint32_t hicon;
    uint32_t hbrush;
    uint32_t name_ptr;
    std::string name;
    uint32_t style;
};

struct window {
    uint32_t hwnd;
    SDL_Window *sdl;
    uint32_t wndproc;
    uint32_t style;
    uint32_t exstyle;
    uint32_t userdata;
    uint32_t id;
    uint32_t hinstance;
    int x, y, w, h;
    bool visible;
    bool destroyed;
};

struct gmsg {
    uint32_t hwnd;
    uint32_t message;
    uint32_t wparam;
    uint32_t lparam;
};

std::vector<wndclass> classes;
std::vector<window *> windows;   // index = (hwnd - kHwndBase) / 4 - 1
std::deque<gmsg> queue;
std::vector<std::string> registered_messages;
bool sdl_ready;
uint32_t mouse_guest_x, mouse_guest_y;

window *find_window(uint32_t hwnd)
{
    if (hwnd < kHwndBase || (hwnd - kHwndBase) % 4 != 0) return nullptr;
    uint32_t i = (hwnd - kHwndBase) / 4 - 1;
    if (i >= windows.size() || windows[i]->destroyed) return nullptr;
    return windows[i];
}

window *window_by_sdl(uint32_t id)
{
    for (window *w : windows) {
        if (!w->destroyed && w->sdl && SDL_GetWindowID(w->sdl) == id) return w;
    }
    return nullptr;
}

uint32_t get32(uint32_t addr)
{
    uint32_t v;
    memcpy(&v, rt_mem_base + addr, 4);
    return v;
}

void put32(uint32_t addr, uint32_t v)
{
    memcpy(rt_mem_base + addr, &v, 4);
}

void put_rect(uint32_t addr, int l, int t, int r, int b)
{
    put32(addr + 0, (uint32_t)l);
    put32(addr + 4, (uint32_t)t);
    put32(addr + 8, (uint32_t)r);
    put32(addr + 12, (uint32_t)b);
}

std::string guest_string(uint32_t addr, size_t max = 1024)
{
    std::string s;
    if (!addr) return s;
    for (size_t i = 0; i < max; i++) {
        char ch = (char)rt_mem_base[addr + i];
        if (!ch) break;
        s.push_back(ch);
    }
    return s;
}

void ensure_sdl()
{
    if (sdl_ready) return;
    setenv("DXVK_WSI_DRIVER", "SDL2", 1);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "rt: SDL_Init : %s\n", SDL_GetError());
        exit(4);
    }
    sdl_ready = true;
    pad_load_config();
    if (getenv("CT3_PAD_TEST")) pad_test_attach();
}

const wndclass *find_class(const std::string &name)
{
    for (const wndclass &c : classes) {
        if (strcasecmp(c.name.c_str(), name.c_str()) == 0) return &c;
    }
    return nullptr;
}

// Appel synchrone de la procédure de fenêtre invitée (stdcall : hwnd, msg, wParam, lParam).
uint32_t send_to_guest(cpu_t *c, const window *w, uint32_t msg, uint32_t wparam, uint32_t lparam)
{
    if (!w || !w->wndproc) return 0;
    uint32_t args[4] = {w->hwnd, msg, wparam, lparam};
    return rt_call_guest(c, w->wndproc, args, 4);
}

// Met en file un message ; WM_QUIT et les messages de fermeture passent par la même file.
void post(uint32_t hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam)
{
    queue.push_back({hwnd, msg, wparam, lparam});
}

uint32_t vk_from_sdl(SDL_Keycode k)
{
    if (k >= SDLK_a && k <= SDLK_z) return 'A' + (k - SDLK_a);
    if (k >= SDLK_0 && k <= SDLK_9) return '0' + (k - SDLK_0);
    if (k >= SDLK_F1 && k <= SDLK_F12) return 0x70 + (k - SDLK_F1);
    switch (k) {
    case SDLK_ESCAPE: return 0x1B;
    case SDLK_RETURN: return 0x0D;
    case SDLK_SPACE: return 0x20;
    case SDLK_TAB: return 0x09;
    case SDLK_BACKSPACE: return 0x08;
    case SDLK_LEFT: return 0x25;
    case SDLK_UP: return 0x26;
    case SDLK_RIGHT: return 0x27;
    case SDLK_DOWN: return 0x28;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return 0x10;
    case SDLK_LCTRL: case SDLK_RCTRL: return 0x11;
    case SDLK_LALT: case SDLK_RALT: return 0x12;
    default: return 0;
    }
}

/* Manette (SDL GameController). Le jeu ne lit que le clavier DirectInput (DIK) : chaque entrée de la manette
 * est reliée à une touche du jeu par la section [Controller] de Config.ini (défaut : mapping PS2).
 * Entrées : a b x y back guide start lstick rstick lb rb dpad_up dpad_down dpad_left dpad_right (boutons SDL,
 * dans l'ordre de SDL_GameControllerButton), puis lt rt (déclencheurs), lstick_left lstick_right lstick_up
 * lstick_down (stick gauche). Touches : up down left right enter escape x s z c space tab, ou none. */
enum { kPadInputs = 21, kPadTriggerLt = 15, kPadTriggerRt = 16, kPadStickLeft = 17, kPadStickRight = 18,
       kPadStickUp = 19, kPadStickDown = 20 };
static const char *const kPadInputNames[kPadInputs] = {
    "a", "b", "x", "y", "back", "guide", "start", "lstick", "rstick", "lb", "rb", "dpad_up", "dpad_down",
    "dpad_left", "dpad_right", "lt", "rt", "lstick_left", "lstick_right", "lstick_up", "lstick_down"};
struct pad_keydef {
    const char *name;
    uint8_t dik;
    uint32_t vk;
};
static const pad_keydef kPadKeys[] = {
    {"up", 0xC8, 0x26}, {"down", 0xD0, 0x28}, {"left", 0xCB, 0x25}, {"right", 0xCD, 0x27},
    {"enter", 0x1C, 0x0D}, {"escape", 0x01, 0x1B}, {"x", 0x2D, 0x58}, {"s", 0x1F, 0x53},
    {"z", 0x2C, 0x5A}, {"c", 0x2E, 0x43}, {"space", 0x39, 0x20}, {"tab", 0x0F, 0x09}};
/* Mapping PS2 par défaut (voir Config.ini à la racine du dépôt). */
static const char *const kPadDefaultConfig =
    "[Controller]\n"
    "DeadZone=16000\n"
    "TriggerThreshold=16000\n"
    "a=s\n"        /* croix : mode Drive */
    "b=escape\n"   /* rond : retour (klaxon à définir) */
    "x=none\n"     /* carré : klaxon à définir */
    "y=space\n"    /* triangle : Crazy Dash / changement de vitesse (à valider) */
    "start=enter\n"
    "lb=x\n"       /* L1 : marche arrière */
    "rb=up\n"      /* R1 : accélérateur */
    "lt=none\n"
    "rt=down\n"    /* R2 : frein */
    "dpad_up=up\n"
    "dpad_down=down\n"
    "dpad_left=left\n"
    "dpad_right=right\n"
    "lstick_left=left\n"
    "lstick_right=right\n"
    "lstick_up=none\n"
    "lstick_down=none\n";
static uint8_t pad_bind_dik[kPadInputs];
static uint32_t pad_bind_vk[kPadInputs];
static bool pad_in_on[kPadInputs];
static uint8_t pad_count[256];
static int pad_deadzone = 16000;
static int pad_trigger_threshold = 16000;

static int pad_find_input(const char *name)
{
    for (int i = 0; i < kPadInputs; i++) {
        if (strcasecmp(kPadInputNames[i], name) == 0) return i;
    }
    return -1;
}

static bool pad_find_key(const char *name, pad_keydef &out)
{
    if (strcasecmp(name, "none") == 0 || *name == '\0') {
        out = {"none", 0, 0};
        return true;
    }
    for (const pad_keydef &k : kPadKeys) {
        if (strcasecmp(k.name, name) == 0) {
            out = k;
            return true;
        }
    }
    return false;
}

static void pad_parse_config(const char *text, const char *origin)
{
    bool in_controller = false;
    char line[512];
    const char *p = text;
    int lineno = 0;
    while (*p) {
        size_t n = 0;
        while (p[n] && p[n] != '\n' && n < sizeof line - 1) n++;
        memcpy(line, p, n);
        line[n] = '\0';
        p += n;
        if (*p == '\n') p++;
        lineno++;
        char *c = line;
        while (*c == ' ' || *c == '\t' || *c == '\r') c++;
        char *cut = strpbrk(c, ";#");
        if (cut && (cut == c || cut[-1] == ' ' || cut[-1] == '\t')) *cut = '\0';
        size_t len = strlen(c);
        while (len && (c[len - 1] == ' ' || c[len - 1] == '\t' || c[len - 1] == '\r')) c[--len] = '\0';
        if (len == 0) continue;
        if (c[0] == '[') {
            in_controller = strcasecmp(c, "[Controller]") == 0;
            continue;
        }
        if (!in_controller) continue;
        char *eq = strchr(c, '=');
        if (!eq) {
            fprintf(stderr, "rt: %s:%d : ligne ignorée\n", origin, lineno);
            continue;
        }
        *eq = '\0';
        char *key = c, *val = eq + 1;
        while (*val == ' ' || *val == '\t') val++;
        size_t vl = strlen(val);
        while (vl && (val[vl - 1] == ' ' || val[vl - 1] == '\t')) val[--vl] = '\0';
        for (size_t i = strlen(key); i && (key[i - 1] == ' ' || key[i - 1] == '\t'); i--) key[i - 1] = '\0';
        if (strcasecmp(key, "DeadZone") == 0) {
            pad_deadzone = atoi(val);
            continue;
        }
        if (strcasecmp(key, "TriggerThreshold") == 0) {
            pad_trigger_threshold = atoi(val);
            continue;
        }
        int input = pad_find_input(key);
        pad_keydef kd;
        if (input < 0 || !pad_find_key(val, kd)) {
            fprintf(stderr, "rt: %s:%d : entrée ou touche inconnue (%s = %s)\n", origin, lineno, key, val);
            continue;
        }
        pad_bind_dik[input] = kd.dik;
        pad_bind_vk[input] = kd.vk;
    }
}

/* Charge la section [Controller] : fichier CT3_CONFIG, sinon Config.ini (répertoire courant), sinon défauts. */
void pad_load_config()
{
    pad_parse_config(kPadDefaultConfig, "défauts");
    const char *path = getenv("CT3_CONFIG");
    if (!path) path = "Config.ini";
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (getenv("CT3_CONFIG")) fprintf(stderr, "rt: fichier de configuration absent : %s\n", path);
        return;
    }
    std::string text;
    char buf[4096];
    size_t got;
    while ((got = fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, got);
    fclose(f);
    /* Les réglages du fichier remplacent les défauts entrée par entrée. */
    pad_parse_config(text.c_str(), path);
}

static void pad_send(uint8_t dik, uint32_t vk, bool down)
{
    if (!dik) return;
    if (down) {
        pad_count[dik]++;
    } else if (pad_count[dik]) {
        pad_count[dik]--;
    }
    window *w = nullptr;
    for (window *x : windows) {
        if (!x->destroyed) w = x;
    }
    if (w) post(w->hwnd, down ? kWmKeyDown : kWmKeyUp, vk, 1u | ((uint32_t)dik << 16) | (down ? 0u : 0xC0000000u));
}

static void pad_input(int input, bool on)
{
    if (pad_in_on[input] == on) return;
    pad_in_on[input] = on;
    if (getenv("CT3_PAD_LOG")) fprintf(stderr, "[pad] %s %s\n", kPadInputNames[input], on ? "appuyé" : "relâché");
    pad_send(pad_bind_dik[input], pad_bind_vk[input], on);
}

static void pad_event(const SDL_Event &e)
{
    switch (e.type) {
    case SDL_CONTROLLERDEVICEADDED:
        SDL_GameControllerOpen((int)e.cdevice.which);
        if (getenv("CT3_PAD_LOG")) fprintf(stderr, "[pad] ajoutée (index %d)\n", (int)e.cdevice.which);
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        for (int i = 0; i < kPadInputs; i++) pad_input(i, false);
        break;
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        if (e.cbutton.button < 15) pad_input(e.cbutton.button, e.type == SDL_CONTROLLERBUTTONDOWN);
        break;
    case SDL_CONTROLLERAXISMOTION:
        if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
            pad_input(kPadTriggerLt, e.caxis.value > pad_trigger_threshold);
        } else if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
            pad_input(kPadTriggerRt, e.caxis.value > pad_trigger_threshold);
        } else if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
            pad_input(kPadStickLeft, e.caxis.value < -pad_deadzone);
            pad_input(kPadStickRight, e.caxis.value > pad_deadzone);
        } else if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            pad_input(kPadStickUp, e.caxis.value < -pad_deadzone);
            pad_input(kPadStickDown, e.caxis.value > pad_deadzone);
        }
        break;
    default:
        break;
    }
}

/* Manette virtuelle pour les tests (CT3_PAD_TEST=1) : bouton Start poussé par rt_win_pad_test. */
static SDL_Joystick *pad_test_js;

void pad_test_attach()
{
    SDL_VirtualJoystickDesc desc;
    memset(&desc, 0, sizeof desc);
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    desc.name = "CT3 pad de test";
    int idx = SDL_JoystickAttachVirtualEx(&desc);
    if (idx < 0) {
        fprintf(stderr, "rt: manette virtuelle indisponible : %s\n", SDL_GetError());
        return;
    }
    if (SDL_IsGameController(idx)) SDL_GameControllerOpen(idx);
    pad_test_js = SDL_JoystickOpen(idx);
}


// Traduit les événements SDL en messages de fenêtre (appelé avant chaque lecture de la file).
void pump_events()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
        case SDL_CONTROLLERAXISMOTION:
            pad_event(e);
            break;
        case SDL_QUIT:
            for (window *w : windows) {
                if (!w->destroyed) post(w->hwnd, kWmClose, 0, 0);
            }
            break;
        case SDL_WINDOWEVENT: {
            window *w = window_by_sdl(e.window.windowID);
            if (!w) break;
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_RESIZED) {
                w->w = e.window.data1;
                w->h = e.window.data2;
                post(w->hwnd, kWmSize, 0, ((uint32_t)w->h << 16) | (uint32_t)w->w);
            } else if (e.window.event == SDL_WINDOWEVENT_MOVED) {
                w->x = e.window.data1;
                w->y = e.window.data2;
                post(w->hwnd, kWmMove, 0, ((uint32_t)w->y << 16) | (uint32_t)w->x);
            } else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                post(w->hwnd, kWmActivate, 1, 0);
            } else if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                post(w->hwnd, kWmActivate, 0, 0);
            }
            break;
        }
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            window *w = nullptr;
            for (window *x : windows) {
                if (!x->destroyed) w = x;
            }
            if (!w) break;
            uint32_t vk = vk_from_sdl(e.key.keysym.sym);
            bool down = e.type == SDL_KEYDOWN;
            dik_set(e.key.keysym.scancode, down);
            uint32_t lp = 1u | ((uint32_t)e.key.keysym.scancode << 16);
            if (!down) lp |= 0xC0000000u;
            post(w->hwnd, down ? kWmKeyDown : kWmKeyUp, vk, lp);
            break;
        }
        case SDL_MOUSEMOTION: {
            window *w = nullptr;
            for (window *x : windows) {
                if (!x->destroyed) w = x;
            }
            if (!w) break;
            mouse_guest_x = (uint32_t)e.motion.x;
            mouse_guest_y = (uint32_t)e.motion.y;
            post(w->hwnd, kWmMouseMove, 0, ((uint32_t)e.motion.y << 16) | (uint32_t)e.motion.x);
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            window *w = nullptr;
            for (window *x : windows) {
                if (!x->destroyed) w = x;
            }
            if (!w) break;
            bool down = e.type == SDL_MOUSEBUTTONDOWN;
            uint32_t msg = 0;
            if (e.button.button == SDL_BUTTON_LEFT) msg = down ? kWmLButtonDown : kWmLButtonUp;
            else if (e.button.button == SDL_BUTTON_RIGHT) msg = down ? kWmRButtonDown : kWmRButtonUp;
            else if (e.button.button == SDL_BUTTON_MIDDLE) msg = down ? kWmMButtonDown : kWmMButtonUp;
            if (msg) post(w->hwnd, msg, 0, ((uint32_t)e.button.y << 16) | (uint32_t)e.button.x);
            break;
        }
        default:
            break;
        }
    }
}

// Lit un message (remove = true) ou le consulte. Retourne true si un message a été trouvé.
bool take_message(uint32_t out, uint32_t hwnd_filter, bool remove)
{
    pump_events();
    for (size_t i = 0; i < queue.size(); i++) {
        gmsg m = queue[i];
        if (hwnd_filter && m.hwnd != hwnd_filter) continue;
        if (remove) queue.erase(queue.begin() + (long)i);
        if (out) {
            put32(out + 0, m.hwnd);
            put32(out + 4, m.message);
            put32(out + 8, m.wparam);
            put32(out + 12, m.lparam);
            put32(out + 16, 0);
            put32(out + 20, mouse_guest_x);
            put32(out + 24, mouse_guest_y);
        }
        return true;
    }
    return false;
}

}  // namespace

// ---- API hôte ------------------------------------------------------------

extern "C" {

void rt_win_init(void)
{
    ensure_sdl();
}

void rt_win_fit(uint32_t hwnd, uint32_t width, uint32_t height, int windowed)
{
    (void)windowed;  /* plein écran : même traitement (pas de changement de mode d'affichage) */
    window *w = find_window(hwnd);
    if (!w || !w->sdl || !width || !height) return;
    SDL_SetWindowResizable(w->sdl, SDL_TRUE);
    SDL_SetWindowSize(w->sdl, (int)width, (int)height);
    w->w = (int)width;
    w->h = (int)height;
}

void *rt_win_host(uint32_t hwnd)
{
    window *w = find_window(hwnd);
    return w ? static_cast<void *>(w->sdl) : nullptr;
}

uint32_t rt_win_guest(void *host)
{
    for (window *w : windows) {
        if (!w->destroyed && w->sdl == host) return w->hwnd;
    }
    return 0;
}

}  // extern "C"

// ---- Shims user32 ---------------------------------------------------------

namespace {

uint32_t sh_RegisterClassA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    wndclass k;
    k.style = get32(a[0] + 0);
    k.wndproc = get32(a[0] + 4);
    k.hicon = get32(a[0] + 20);
    k.hbrush = get32(a[0] + 24);
    k.name_ptr = get32(a[0] + 36);
    k.name = guest_string(k.name_ptr);
    classes.push_back(k);
    return (uint32_t)(classes.size());
}

uint32_t sh_CreateWindowExA(cpu_t *c, const uint32_t *a)
{
    // (dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam)
    const wndclass *k = find_class(guest_string(a[1]));
    if (!k) {
        fprintf(stderr, "rt: CreateWindowExA : classe inconnue « %s »\n", guest_string(a[1]).c_str());
        return 0;
    }
    ensure_sdl();
    window *w = new window();
    w->hwnd = kHwndBase + 4 * (uint32_t)(windows.size() + 1);
    w->wndproc = k->wndproc;
    w->style = a[3];
    w->exstyle = a[0];
    w->userdata = 0;
    w->id = a[9];
    w->hinstance = a[10];
    w->w = (int)a[6];
    w->h = (int)a[7];
    w->x = a[4] == kCwUseDefault ? SDL_WINDOWPOS_CENTERED : (int)a[4];
    w->y = a[5] == kCwUseDefault ? SDL_WINDOWPOS_CENTERED : (int)a[5];
    w->visible = (a[3] & kWsVisible) != 0;
    w->destroyed = false;
    std::string title = guest_string(a[2]);
    Uint32 flags = SDL_WINDOW_VULKAN;
    if (a[3] & kWsPopup) flags |= SDL_WINDOW_BORDERLESS;
    if (!w->visible) flags |= SDL_WINDOW_HIDDEN;
    w->sdl = SDL_CreateWindow(title.c_str(), w->x, w->y, w->w, w->h, flags);
    if (!w->sdl) {
        fprintf(stderr, "rt: SDL_CreateWindow : %s\n", SDL_GetError());
        delete w;
        return 0;
    }
    windows.push_back(w);

    // CREATESTRUCTA (48 octets) passé à WM_NCCREATE / WM_CREATE.
    uint32_t cs = rt_heap_alloc(48, 1);
    put32(cs + 0, a[11]);
    put32(cs + 4, a[10]);
    put32(cs + 8, a[9]);
    put32(cs + 12, a[8]);
    put32(cs + 16, (uint32_t)w->h);
    put32(cs + 20, (uint32_t)w->w);
    put32(cs + 24, (uint32_t)w->y);
    put32(cs + 28, (uint32_t)w->x);
    put32(cs + 32, a[3]);
    put32(cs + 36, a[2]);
    put32(cs + 40, a[1]);
    put32(cs + 44, a[0]);
    if (send_to_guest(c, w, kWmNcCreate, 0, cs) == 0) {
        // WM_NCCREATE refusé : la fenêtre est détruite.
    }
    if ((int32_t)send_to_guest(c, w, kWmCreate, 0, cs) == -1) {
        w->destroyed = true;
        SDL_DestroyWindow(w->sdl);
        w->sdl = nullptr;
        return 0;
    }
    if (w->visible) post(w->hwnd, kWmSize, 0, ((uint32_t)w->h << 16) | (uint32_t)w->w);
    return w->hwnd;
}

uint32_t sh_ShowWindow(cpu_t *c, const uint32_t *a)
{
    (void)c;
    window *w = find_window(a[0]);
    if (!w) return 0;
    bool was = w->visible;
    w->visible = a[1] != 0;
    if (w->sdl) {
        if (w->visible) SDL_ShowWindow(w->sdl);
        else SDL_HideWindow(w->sdl);
    }
    return was ? 1 : 0;
}

uint32_t sh_UpdateWindow(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 1;
}

uint32_t sh_DestroyWindow(cpu_t *c, const uint32_t *a)
{
    window *w = find_window(a[0]);
    if (!w) return 0;
    send_to_guest(c, w, kWmDestroy, 0, 0);
    w->destroyed = true;
    if (w->sdl) SDL_DestroyWindow(w->sdl);
    w->sdl = nullptr;
    return 1;
}

uint32_t sh_GetClientRect(cpu_t *c, const uint32_t *a)
{
    (void)c;
    window *w = find_window(a[0]);
    if (!w) return 0;
    int ww = w->w, hh = w->h;
    if (w->sdl) SDL_GetWindowSize(w->sdl, &ww, &hh);
    put_rect(a[1], 0, 0, ww, hh);
    return 1;
}

uint32_t sh_GetWindowRect(cpu_t *c, const uint32_t *a)
{
    (void)c;
    window *w = find_window(a[0]);
    if (!w) return 0;
    int x = w->x, y = w->y, ww = w->w, hh = w->h;
    if (w->sdl) {
        SDL_GetWindowPosition(w->sdl, &x, &y);
        SDL_GetWindowSize(w->sdl, &ww, &hh);
    }
    put_rect(a[1], x, y, x + ww, y + hh);
    return 1;
}

uint32_t sh_SetWindowPos(cpu_t *c, const uint32_t *a)
{
    (void)c;
    // (hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags)
    window *w = find_window(a[0]);
    if (!w) return 0;
    if (w->sdl) {
        SDL_SetWindowPosition(w->sdl, (int)a[2], (int)a[3]);
        SDL_SetWindowSize(w->sdl, (int)a[4], (int)a[5]);
    }
    w->x = (int)a[2];
    w->y = (int)a[3];
    w->w = (int)a[4];
    w->h = (int)a[5];
    return 1;
}

uint32_t sh_AdjustWindowRect(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 1;
}

uint32_t sh_SetRect(cpu_t *c, const uint32_t *a)
{
    (void)c;
    put_rect(a[0], (int)a[1], (int)a[2], (int)a[3], (int)a[4]);
    return 1;
}

uint32_t sh_GetWindowLongA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    window *w = find_window(a[0]);
    if (!w) return 0;
    switch ((int32_t)a[1]) {
    case -4: return w->wndproc;     // GWL_WNDPROC
    case -6: return w->hinstance;   // GWL_HINSTANCE
    case -8: return w->id;          // GWL_HWNDPARENT
    case -12: return w->id;         // GWL_ID
    case -16: return w->style;      // GWL_STYLE
    case -20: return w->exstyle;    // GWL_EXSTYLE
    case -21: return w->userdata;   // GWL_USERDATA
    default: return 0;
    }
}

uint32_t sh_SetWindowLongA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    window *w = find_window(a[0]);
    if (!w) return 0;
    uint32_t old = 0;
    switch ((int32_t)a[1]) {
    case -4: old = w->wndproc; w->wndproc = a[2]; break;
    case -16: old = w->style; w->style = a[2]; break;
    case -20: old = w->exstyle; w->exstyle = a[2]; break;
    case -21: old = w->userdata; w->userdata = a[2]; break;
    default: break;
    }
    return old;
}

uint32_t sh_GetClassLongA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 0;
}

uint32_t sh_LoadIconA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return kFakeIcon;
}

uint32_t sh_SetCursor(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return kFakeIcon;
}

uint32_t sh_GetCursorPos(cpu_t *c, const uint32_t *a)
{
    (void)c;
    int x = 0, y = 0;
    SDL_GetMouseState(&x, &y);
    put32(a[0] + 0, (uint32_t)x);
    put32(a[0] + 4, (uint32_t)y);
    return 1;
}

uint32_t sh_ScreenToClient(cpu_t *c, const uint32_t *a)
{
    (void)c;
    window *w = find_window(a[0]);
    if (!w) return 0;
    int wx = 0, wy = 0;
    if (w->sdl) SDL_GetWindowPosition(w->sdl, &wx, &wy);
    put32(a[1] + 0, get32(a[1] + 0) - (uint32_t)wx);
    put32(a[1] + 4, get32(a[1] + 4) - (uint32_t)wy);
    return 1;
}

uint32_t sh_ClipCursor(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 1;
}

uint32_t sh_GetDC(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return kFakeDc;
}

uint32_t sh_ReleaseDC(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 1;
}

uint32_t sh_DefWindowProcA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    if (a[1] == kWmClose) {
        window *w = find_window(a[0]);
        if (w) {
            w->destroyed = true;
            if (w->sdl) SDL_DestroyWindow(w->sdl);
            w->sdl = nullptr;
        }
        post(a[0], kWmDestroy, 0, 0);
    } else if (a[1] == kWmDestroy) {
        post(0, kWmQuit, 0, 0);
    }
    return 0;
}

uint32_t sh_PostQuitMessage(cpu_t *c, const uint32_t *a)
{
    (void)c;
    post(0, kWmQuit, a[0], 0);
    return 0;
}

uint32_t sh_PostMessageA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    post(a[0], a[1], a[2], a[3]);
    return 1;
}

uint32_t sh_PostThreadMessageA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    post(0, a[1], a[2], a[3]);
    return 1;
}

uint32_t sh_SendMessageA(cpu_t *c, const uint32_t *a)
{
    window *w = find_window(a[0]);
    if (!w) return 0;
    return send_to_guest(c, w, a[1], a[2], a[3]);
}

uint32_t sh_PeekMessageA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    // (lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg)
    bool remove = (a[4] & 1) != 0;
    return take_message(a[0], a[1], remove) ? 1 : 0;
}

uint32_t sh_GetMessageA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    // Attente active : le verrou est relâché entre deux interrogations (rt_sleep_ms).
    while (!take_message(a[0], a[1], true)) {
        rt_sleep_ms(1);
    }
    return get32(a[0] + 4) == kWmQuit ? 0 : 1;
}

uint32_t sh_TranslateMessage(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 0;
}

uint32_t sh_DispatchMessageA(cpu_t *c, const uint32_t *a)
{
    uint32_t hwnd = get32(a[0] + 0), msg = get32(a[0] + 4);
    uint32_t wp = get32(a[0] + 8), lp = get32(a[0] + 12);
    window *w = find_window(hwnd);
    if (!w) return 0;
    return send_to_guest(c, w, msg, wp, lp);
}

uint32_t sh_GetQueueStatus(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    pump_events();
    return queue.empty() ? 0 : 0x00000100u;
}

uint32_t sh_MsgWaitForMultipleObjects(cpu_t *c, const uint32_t *a)
{
    (void)c;
    // (nCount, pHandles, fWaitAll, dwMilliseconds, dwWakeMask)
    uint32_t timeout = a[3];
    uint32_t waited = 0;
    while (true) {
        pump_events();
        if (!queue.empty()) return a[0];  // WAIT_OBJECT_0 + nCount : message disponible
        if (timeout != 0xFFFFFFFFu && waited >= timeout) return 0x102;  // WAIT_TIMEOUT
        rt_sleep_ms(1);
        waited++;
    }
}

uint32_t sh_RegisterWindowMessageA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::string name = guest_string(a[0]);
    for (size_t i = 0; i < registered_messages.size(); i++) {
        if (registered_messages[i] == name) return 0xC000u + (uint32_t)i;
    }
    registered_messages.push_back(name);
    return 0xC000u + (uint32_t)(registered_messages.size() - 1);
}

uint32_t sh_MessageBoxA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    fprintf(stderr, "rt: MessageBoxA « %s » : « %s »\n", guest_string(a[2]).c_str(),
            guest_string(a[1]).c_str());
    return 1;  // IDOK
}

// Menu de la ressource (CT3.exe en contient un) : handle fictif non nul, le jeu le transmet à CreateWindowExA.
uint32_t sh_LoadMenuA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 0x00090001u;
}

uint32_t sh_ret1(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 1;
}

uint32_t sh_ret0(cpu_t *c, const uint32_t *a)
{
    (void)c;
    (void)a;
    return 0;
}

}  // namespace

// Table des talons user32 (consultée par rt_shims.c).
extern "C" const rt_shim_entry rt_user32_table[] = {
    {"USER32.dll::RegisterClassA", 1, sh_RegisterClassA},
    {"USER32.dll::CreateWindowExA", 12, sh_CreateWindowExA},
    {"USER32.dll::ShowWindow", 2, sh_ShowWindow},
    {"USER32.dll::UpdateWindow", 1, sh_UpdateWindow},
    {"USER32.dll::DestroyWindow", 1, sh_DestroyWindow},
    {"USER32.dll::GetClientRect", 2, sh_GetClientRect},
    {"USER32.dll::GetWindowRect", 2, sh_GetWindowRect},
    {"USER32.dll::SetWindowPos", 7, sh_SetWindowPos},
    {"USER32.dll::AdjustWindowRect", 3, sh_AdjustWindowRect},
    {"USER32.dll::SetRect", 5, sh_SetRect},
    {"USER32.dll::GetWindowLongA", 2, sh_GetWindowLongA},
    {"USER32.dll::SetWindowLongA", 3, sh_SetWindowLongA},
    {"USER32.dll::GetClassLongA", 2, sh_GetClassLongA},
    {"USER32.dll::LoadIconA", 2, sh_LoadIconA},
    {"USER32.dll::LoadCursorA", 2, sh_LoadIconA},
    {"USER32.dll::SetCursor", 1, sh_SetCursor},
    {"USER32.dll::GetCursorPos", 1, sh_GetCursorPos},
    {"USER32.dll::ScreenToClient", 2, sh_ScreenToClient},
    {"USER32.dll::ClipCursor", 1, sh_ClipCursor},
    {"USER32.dll::GetDC", 1, sh_GetDC},
    {"USER32.dll::ReleaseDC", 2, sh_ReleaseDC},
    {"USER32.dll::DefWindowProcA", 4, sh_DefWindowProcA},
    {"USER32.dll::PostQuitMessage", 1, sh_PostQuitMessage},
    {"USER32.dll::PostMessageA", 4, sh_PostMessageA},
    {"USER32.dll::PostThreadMessageA", 4, sh_PostThreadMessageA},
    {"USER32.dll::SendMessageA", 4, sh_SendMessageA},
    {"USER32.dll::PeekMessageA", 5, sh_PeekMessageA},
    {"USER32.dll::GetMessageA", 4, sh_GetMessageA},
    {"USER32.dll::TranslateMessage", 1, sh_TranslateMessage},
    {"USER32.dll::DispatchMessageA", 1, sh_DispatchMessageA},
    {"USER32.dll::GetQueueStatus", 1, sh_GetQueueStatus},
    {"USER32.dll::MsgWaitForMultipleObjects", 5, sh_MsgWaitForMultipleObjects},
    {"USER32.dll::RegisterWindowMessageA", 1, sh_RegisterWindowMessageA},
    {"USER32.dll::MessageBoxA", 4, sh_MessageBoxA},
    {"USER32.dll::GetMenu", 1, sh_ret0},
    {"USER32.dll::SetMenu", 2, sh_ret1},
    {"USER32.dll::LoadMenuA", 2, sh_LoadMenuA},
    {"USER32.dll::LoadAcceleratorsA", 2, sh_ret0},
    {"USER32.dll::TranslateAcceleratorA", 3, sh_ret0},
    {"USER32.dll::DestroyAcceleratorTable", 1, sh_ret1},
    {"USER32.dll::EnableWindow", 2, sh_ret1},
};

extern "C" const unsigned rt_user32_count = sizeof rt_user32_table / sizeof rt_user32_table[0];

/* État clavier pour DirectInput : indexé par code DIK (dinput.h), mis à jour par les événements SDL. */
static uint8_t dik_state[256];

static uint8_t dik_of_sdl(int sc)
{
    static const uint8_t letters[26] = {0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
                                        0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C};
    static const uint8_t digits[10] = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B};  /* 1..9, 0 */
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) return letters[sc - SDL_SCANCODE_A];
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0) return digits[sc - SDL_SCANCODE_1];
    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F10) return (uint8_t)(0x3B + (sc - SDL_SCANCODE_F1));
    switch (sc) {
    case SDL_SCANCODE_RETURN: return 0x1C;
    case SDL_SCANCODE_ESCAPE: return 0x01;
    case SDL_SCANCODE_BACKSPACE: return 0x0E;
    case SDL_SCANCODE_TAB: return 0x0F;
    case SDL_SCANCODE_SPACE: return 0x39;
    case SDL_SCANCODE_MINUS: return 0x0C;
    case SDL_SCANCODE_EQUALS: return 0x0D;
    case SDL_SCANCODE_LEFTBRACKET: return 0x1A;
    case SDL_SCANCODE_RIGHTBRACKET: return 0x1B;
    case SDL_SCANCODE_BACKSLASH: return 0x2B;
    case SDL_SCANCODE_SEMICOLON: return 0x27;
    case SDL_SCANCODE_APOSTROPHE: return 0x28;
    case SDL_SCANCODE_GRAVE: return 0x29;
    case SDL_SCANCODE_COMMA: return 0x33;
    case SDL_SCANCODE_PERIOD: return 0x34;
    case SDL_SCANCODE_SLASH: return 0x35;
    case SDL_SCANCODE_CAPSLOCK: return 0x3A;
    case SDL_SCANCODE_F11: return 0x57;
    case SDL_SCANCODE_F12: return 0x58;
    case SDL_SCANCODE_HOME: return 0xC7;
    case SDL_SCANCODE_PAGEUP: return 0xC9;
    case SDL_SCANCODE_DELETE: return 0xD3;
    case SDL_SCANCODE_END: return 0xCF;
    case SDL_SCANCODE_PAGEDOWN: return 0xD1;
    case SDL_SCANCODE_RIGHT: return 0xCD;
    case SDL_SCANCODE_LEFT: return 0xCB;
    case SDL_SCANCODE_DOWN: return 0xD0;
    case SDL_SCANCODE_UP: return 0xC8;
    case SDL_SCANCODE_LCTRL: return 0x1D;
    case SDL_SCANCODE_LSHIFT: return 0x2A;
    case SDL_SCANCODE_LALT: return 0x38;
    case SDL_SCANCODE_RCTRL: return 0x9D;
    case SDL_SCANCODE_RSHIFT: return 0x36;
    case SDL_SCANCODE_RALT: return 0xB8;
    default: return 0;
    }
}

static void dik_set(int sc, bool down)
{
    uint8_t k = dik_of_sdl(sc);
    if (k) dik_state[k] = down ? 0x80 : 0;
}

void rt_win_key_states(uint8_t out[256])
{
    memcpy(out, dik_state, 256);
    for (int k = 0; k < 256; k++) {
        if (pad_count[k]) out[k] = 0x80;
    }
}

void rt_win_pad_test(int down, const char *name)
{
    static const struct { const char *name; int button; } kNames[] = {
        {"start", SDL_CONTROLLER_BUTTON_START}, {"a", SDL_CONTROLLER_BUTTON_A}, {"b", SDL_CONTROLLER_BUTTON_B},
        {"x", SDL_CONTROLLER_BUTTON_X}, {"y", SDL_CONTROLLER_BUTTON_Y}, {"up", SDL_CONTROLLER_BUTTON_DPAD_UP},
        {"down", SDL_CONTROLLER_BUTTON_DPAD_DOWN}, {"left", SDL_CONTROLLER_BUTTON_DPAD_LEFT},
        {"right", SDL_CONTROLLER_BUTTON_DPAD_RIGHT}, {"lb", SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
        {"rb", SDL_CONTROLLER_BUTTON_RIGHTSHOULDER}};
    if (!pad_test_js) return;
    for (const auto &n : kNames) {
        if (strcmp(n.name, name) == 0) SDL_JoystickSetVirtualButton(pad_test_js, n.button, down ? 1 : 0);
    }
}

void rt_win_test_key(int down)
{
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    ev.key.keysym.sym = SDLK_RETURN;
    ev.key.keysym.scancode = SDL_SCANCODE_RETURN;
    SDL_PushEvent(&ev);
}

