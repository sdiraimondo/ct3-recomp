/* Correctif d'interface (Config.ini [Patches] UiTexcoord) : avant chaque vidage de sprites, le jeu ne réinitialise pas
 * D3DTSS_TEXCOORDINDEX ; un matériau à réflexion d'environnement laisse alors le mode de génération de coordonnées
 * actif et déforme l'interface (DXVK). Réinitialisation à 0 sur les quatre étages, comme avant le vidage. */

#include "rt.h"
#include "rt_d3d9.h"

#include <d3d9.h>
#include <string.h>

/* Render::D3D_DEVICE (pointeur invité vers le périphérique), offset 0x314E74 de l'image. */
static const uint32_t kRenderDeviceAddr = 0x00714E74u;

extern "C" void rt_patch_before_flush(cpu_t *c)
{
    uint32_t dev_guest;
    IDirect3DDevice9 *dev;

    (void)c;
    if (!rt_patch_ui_texcoord_enabled()) return;
    memcpy(&dev_guest, rt_mem_base + kRenderDeviceAddr, 4);
    if (!dev_guest) return;
    dev = rt_self<IDirect3DDevice9>(dev_guest);
    if (!dev) return;
    for (DWORD stage = 0; stage < 4; stage++) {
        dev->SetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, 0);
    }
}
