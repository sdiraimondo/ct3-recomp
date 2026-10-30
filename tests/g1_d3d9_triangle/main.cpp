// Gate G1 : un triangle D3D9 rendu par dxvk-native (Vulkan) dans une fenêtre SDL2.
// Aucun asset du jeu. Le résultat est vérifié en relisant un pixel du back buffer.

#include <SDL.h>
#include <d3d9.h>

#include <cstdio>
#include <cstdlib>

namespace {

struct Vertex {
    float x, y, z, rhw;
    DWORD color;
};

constexpr DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
constexpr int kWidth = 640;
constexpr int kHeight = 480;
constexpr int kFrames = 300;
constexpr DWORD kClearColor = 0xFF202020;

}  // namespace

int main() {
    setenv("DXVK_WSI_DRIVER", "SDL2", 1);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init : %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("CT3 G1 : triangle D3D9",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          kWidth, kHeight, SDL_WINDOW_VULKAN);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow : %s\n", SDL_GetError());
        return 1;
    }

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) {
        std::fprintf(stderr, "Direct3DCreate9 a échoué\n");
        return 1;
    }

    HWND hwnd = reinterpret_cast<HWND>(window);

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = kWidth;
    pp.BackBufferHeight = kHeight;
    pp.BackBufferCount = 1;
    pp.hDeviceWindow = hwnd;

    IDirect3DDevice9* device = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                   D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    if (FAILED(hr) || !device) {
        std::fprintf(stderr, "CreateDevice : HRESULT 0x%08lx\n", static_cast<unsigned long>(hr));
        return 1;
    }

    // Triangle à cheval sur le centre de l'écran : le pixel (320, 300) est à l'intérieur.
    const Vertex triangle[3] = {
        {320.0f, 80.0f, 0.5f, 1.0f, 0xFFFF0000},
        {520.0f, 400.0f, 0.5f, 1.0f, 0xFF00FF00},
        {120.0f, 400.0f, 0.5f, 1.0f, 0xFF0000FF},
    };

    device->SetFVF(kFvf);

    bool quit = false;
    for (int frame = 0; frame < kFrames && !quit; ++frame) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            }
        }

        device->Clear(0, nullptr, D3DCLEAR_TARGET, kClearColor, 1.0f, 0);
        device->BeginScene();
        device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, sizeof(Vertex));
        device->EndScene();

        if (frame == kFrames - 1) {
            IDirect3DSurface9* backBuffer = nullptr;
            IDirect3DSurface9* readback = nullptr;
            device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
            device->CreateOffscreenPlainSurface(kWidth, kHeight, D3DFMT_X8R8G8B8,
                                                D3DPOOL_SYSTEMMEM, &readback, nullptr);
            device->GetRenderTargetData(backBuffer, readback);

            D3DLOCKED_RECT locked = {};
            readback->LockRect(&locked, nullptr, D3DLOCK_READONLY);
            const DWORD pixel = *reinterpret_cast<const DWORD*>(
                static_cast<const BYTE*>(locked.pBits) + 300 * locked.Pitch + 320 * 4);
            readback->UnlockRect();

            std::printf("pixel(320,300) = 0x%08lx (fond 0x%08lx)\n",
                        static_cast<unsigned long>(pixel),
                        static_cast<unsigned long>(kClearColor));

            readback->Release();
            backBuffer->Release();

            const bool ok = (pixel & 0x00FFFFFF) != (kClearColor & 0x00FFFFFF);
            std::printf(ok ? "G1 OK\n" : "G1 ECHEC : le triangle n'est pas rendu\n");
            device->Release();
            d3d->Release();
            SDL_DestroyWindow(window);
            SDL_Quit();
            return ok ? 0 : 2;
        }

        device->Present(nullptr, nullptr, nullptr, nullptr);
    }

    device->Release();
    d3d->Release();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 3;
}
