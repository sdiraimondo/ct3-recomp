// Généré par tools/d3d9/gen_com.py à partir de d3d9.h (dxvk-native). Ne pas éditer.
#include "rt_d3d9.h"

#include <d3d9.h>

extern "C" {

uint32_t custom_IDirect3D9_CreateDevice(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3D9_GetAdapterMonitor(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DCubeTexture9_LockRect(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DCubeTexture9_UnlockRect(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DDevice9_CreateIndexBuffer(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DDevice9_CreateTexture(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DDevice9_CreateVertexBuffer(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DDevice9_GetCreationParameters(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DDevice9_Present(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DDevice9_Reset(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DIndexBuffer9_Lock(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DIndexBuffer9_Unlock(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DSurface9_LockRect(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DSurface9_UnlockRect(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DTexture9_LockRect(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DTexture9_UnlockRect(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DVertexBuffer9_Lock(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DVertexBuffer9_Unlock(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DVolume9_LockBox(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DVolume9_UnlockBox(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DVolumeTexture9_LockBox(cpu_t *c, const uint32_t *a);
uint32_t custom_IDirect3DVolumeTexture9_UnlockBox(cpu_t *c, const uint32_t *a);
uint32_t custom_IUnknown_QueryInterface(cpu_t *c, const uint32_t *a);
uint32_t custom_IUnknown_Release(cpu_t *c, const uint32_t *a);

static uint32_t m_IUnknown_AddRef(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IUnknown *self = rt_self<IUnknown>(a[0]);
    ULONG r = self->AddRef();
    return (uint32_t)r;
}
static uint32_t m_IDirect3D9_RegisterSoftwareDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3D9 *self = rt_self<IDirect3D9>(a[0]);
    HRESULT r = self->RegisterSoftwareDevice((void*)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3D9_GetAdapterCount(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3D9 *self = rt_self<IDirect3D9>(a[0]);
    UINT r = self->GetAdapterCount();
    return (uint32_t)r;
}
static uint32_t m_IDirect3D9_GetAdapterIdentifier(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3D9 *self = rt_self<IDirect3D9>(a[0]);
    HRESULT r = self->GetAdapterIdentifier((UINT)a[1], (DWORD)a[2], (D3DADAPTER_IDENTIFIER9*)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3D9_GetAdapterModeCount(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3D9 *self = rt_self<IDirect3D9>(a[0]);
    UINT r = self->GetAdapterModeCount((UINT)a[1], (D3DFORMAT)a[2]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3D9_EnumAdapterModes(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3D9 *self = rt_self<IDirect3D9>(a[0]);
    HRESULT r = self->EnumAdapterModes((UINT)a[1], (D3DFORMAT)a[2], (UINT)a[3], (D3DDISPLAYMODE*)rt_gptr(a[4]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3D9_GetAdapterDisplayMode(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3D9 *self = rt_self<IDirect3D9>(a[0]);
    HRESULT r = self->GetAdapterDisplayMode((UINT)a[1], (D3DDISPLAYMODE*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3D9_CheckDeviceType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3D9 *self = rt_self<IDirect3D9>(a[0]);
    HRESULT r = self->CheckDeviceType((UINT)a[1], (D3DDEVTYPE)a[2], (D3DFORMAT)a[3], (D3DFORMAT)a[4], (WINBOOL)a[5]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3D9_CheckDeviceFormat(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3D9 *self = rt_self<IDirect3D9>(a[0]);
    HRESULT r = self->CheckDeviceFormat((UINT)a[1], (D3DDEVTYPE)a[2], (D3DFORMAT)a[3], (DWORD)a[4], (D3DRESOURCETYPE)a[5], (D3DFORMAT)a[6]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3D9_CheckDeviceMultiSampleType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3D9 *self = rt_self<IDirect3D9>(a[0]);
    HRESULT r = self->CheckDeviceMultiSampleType((UINT)a[1], (D3DDEVTYPE)a[2], (D3DFORMAT)a[3], (WINBOOL)a[4], (D3DMULTISAMPLE_TYPE)a[5], (DWORD*)rt_gptr(a[6]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3D9_CheckDepthStencilMatch(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3D9 *self = rt_self<IDirect3D9>(a[0]);
    HRESULT r = self->CheckDepthStencilMatch((UINT)a[1], (D3DDEVTYPE)a[2], (D3DFORMAT)a[3], (D3DFORMAT)a[4], (D3DFORMAT)a[5]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3D9_CheckDeviceFormatConversion(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3D9 *self = rt_self<IDirect3D9>(a[0]);
    HRESULT r = self->CheckDeviceFormatConversion((UINT)a[1], (D3DDEVTYPE)a[2], (D3DFORMAT)a[3], (D3DFORMAT)a[4]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3D9_GetDeviceCaps(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3D9 *self = rt_self<IDirect3D9>(a[0]);
    HRESULT r = self->GetDeviceCaps((UINT)a[1], (D3DDEVTYPE)a[2], (D3DCAPS9*)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_TestCooperativeLevel(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->TestCooperativeLevel();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetAvailableTextureMem(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    UINT r = self->GetAvailableTextureMem();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_EvictManagedResources(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->EvictManagedResources();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetDirect3D(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3D9 *o1 = nullptr;
    HRESULT r = self->GetDirect3D(&o1);
    rt_com_out(a[1], o1, 0);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetDeviceCaps(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetDeviceCaps((D3DCAPS9*)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetDisplayMode(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetDisplayMode((UINT)a[1], (D3DDISPLAYMODE*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetCursorProperties(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetCursorProperties((UINT)a[1], (UINT)a[2], (IDirect3DSurface9 *)rt_com_host(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetCursorPosition(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    self->SetCursorPosition((int)a[1], (int)a[2], (DWORD)a[3]);
    return 0;
}
static uint32_t m_IDirect3DDevice9_ShowCursor(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    WINBOOL r = self->ShowCursor((WINBOOL)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_CreateAdditionalSwapChain(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    D3DPRESENT_PARAMETERS pp1;
    if (a[1]) rt_pp_in(a[1], pp1);
    IDirect3DSwapChain9 *o2 = nullptr;
    HRESULT r = self->CreateAdditionalSwapChain(a[1] ? &pp1 : nullptr, &o2);
    rt_com_out(a[2], o2, 3);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetSwapChain(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DSwapChain9 *o2 = nullptr;
    HRESULT r = self->GetSwapChain((UINT)a[1], &o2);
    rt_com_out(a[2], o2, 3);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetNumberOfSwapChains(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    UINT r = self->GetNumberOfSwapChains();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetBackBuffer(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DSurface9 *o4 = nullptr;
    HRESULT r = self->GetBackBuffer((UINT)a[1], (UINT)a[2], (D3DBACKBUFFER_TYPE)a[3], &o4);
    rt_com_out(a[4], o4, 4);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetRasterStatus(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetRasterStatus((UINT)a[1], (D3DRASTER_STATUS*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetDialogBoxMode(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetDialogBoxMode((WINBOOL)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetGammaRamp(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    self->SetGammaRamp((UINT)a[1], (DWORD)a[2], (const D3DGAMMARAMP *)rt_gptr(a[3]));
    return 0;
}
static uint32_t m_IDirect3DDevice9_GetGammaRamp(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    self->GetGammaRamp((UINT)a[1], (D3DGAMMARAMP*)rt_gptr(a[2]));
    return 0;
}
static uint32_t m_IDirect3DDevice9_CreateVolumeTexture(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DVolumeTexture9 *o8 = nullptr;
    HRESULT r = self->CreateVolumeTexture((UINT)a[1], (UINT)a[2], (UINT)a[3], (UINT)a[4], (DWORD)a[5], (D3DFORMAT)a[6], (D3DPOOL)a[7], &o8, (HANDLE*)rt_gptr(a[9]));
    rt_com_out(a[8], o8, 11);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_CreateCubeTexture(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DCubeTexture9 *o6 = nullptr;
    HRESULT r = self->CreateCubeTexture((UINT)a[1], (UINT)a[2], (DWORD)a[3], (D3DFORMAT)a[4], (D3DPOOL)a[5], &o6, (HANDLE*)rt_gptr(a[7]));
    rt_com_out(a[6], o6, 10);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_CreateRenderTarget(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DSurface9 *o7 = nullptr;
    HRESULT r = self->CreateRenderTarget((UINT)a[1], (UINT)a[2], (D3DFORMAT)a[3], (D3DMULTISAMPLE_TYPE)a[4], (DWORD)a[5], (WINBOOL)a[6], &o7, (HANDLE*)rt_gptr(a[8]));
    rt_com_out(a[7], o7, 4);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_CreateDepthStencilSurface(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DSurface9 *o7 = nullptr;
    HRESULT r = self->CreateDepthStencilSurface((UINT)a[1], (UINT)a[2], (D3DFORMAT)a[3], (D3DMULTISAMPLE_TYPE)a[4], (DWORD)a[5], (WINBOOL)a[6], &o7, (HANDLE*)rt_gptr(a[8]));
    rt_com_out(a[7], o7, 4);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_UpdateSurface(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->UpdateSurface((IDirect3DSurface9 *)rt_com_host(a[1]), (const RECT *)rt_gptr(a[2]), (IDirect3DSurface9 *)rt_com_host(a[3]), (const POINT *)rt_gptr(a[4]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_UpdateTexture(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->UpdateTexture((IDirect3DBaseTexture9 *)rt_com_host(a[1]), (IDirect3DBaseTexture9 *)rt_com_host(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetRenderTargetData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetRenderTargetData((IDirect3DSurface9 *)rt_com_host(a[1]), (IDirect3DSurface9 *)rt_com_host(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetFrontBufferData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetFrontBufferData((UINT)a[1], (IDirect3DSurface9 *)rt_com_host(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_StretchRect(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->StretchRect((IDirect3DSurface9 *)rt_com_host(a[1]), (const RECT *)rt_gptr(a[2]), (IDirect3DSurface9 *)rt_com_host(a[3]), (const RECT *)rt_gptr(a[4]), (D3DTEXTUREFILTERTYPE)a[5]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_ColorFill(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->ColorFill((IDirect3DSurface9 *)rt_com_host(a[1]), (const RECT *)rt_gptr(a[2]), (D3DCOLOR)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_CreateOffscreenPlainSurface(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DSurface9 *o5 = nullptr;
    HRESULT r = self->CreateOffscreenPlainSurface((UINT)a[1], (UINT)a[2], (D3DFORMAT)a[3], (D3DPOOL)a[4], &o5, (HANDLE*)rt_gptr(a[6]));
    rt_com_out(a[5], o5, 4);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetRenderTarget(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetRenderTarget((DWORD)a[1], (IDirect3DSurface9 *)rt_com_host(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetRenderTarget(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DSurface9 *o2 = nullptr;
    HRESULT r = self->GetRenderTarget((DWORD)a[1], &o2);
    rt_com_out(a[2], o2, 4);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetDepthStencilSurface(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetDepthStencilSurface((IDirect3DSurface9 *)rt_com_host(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetDepthStencilSurface(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DSurface9 *o1 = nullptr;
    HRESULT r = self->GetDepthStencilSurface(&o1);
    rt_com_out(a[1], o1, 4);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_BeginScene(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->BeginScene();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_EndScene(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->EndScene();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_Clear(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->Clear((DWORD)a[1], (const D3DRECT *)rt_gptr(a[2]), (DWORD)a[3], (D3DCOLOR)a[4], rt_u2f(a[5]), (DWORD)a[6]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetTransform(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetTransform((D3DTRANSFORMSTATETYPE)a[1], (const D3DMATRIX *)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetTransform(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetTransform((D3DTRANSFORMSTATETYPE)a[1], (D3DMATRIX*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_MultiplyTransform(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->MultiplyTransform((D3DTRANSFORMSTATETYPE)a[1], (const D3DMATRIX *)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetViewport(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetViewport((const D3DVIEWPORT9 *)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetViewport(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetViewport((D3DVIEWPORT9*)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetMaterial(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetMaterial((const D3DMATERIAL9 *)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetMaterial(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetMaterial((D3DMATERIAL9*)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetLight(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetLight((DWORD)a[1], (const D3DLIGHT9 *)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetLight(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetLight((DWORD)a[1], (D3DLIGHT9*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_LightEnable(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->LightEnable((DWORD)a[1], (WINBOOL)a[2]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetLightEnable(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetLightEnable((DWORD)a[1], (WINBOOL*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetClipPlane(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetClipPlane((DWORD)a[1], (const float *)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetClipPlane(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetClipPlane((DWORD)a[1], (float*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetRenderState(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetRenderState((D3DRENDERSTATETYPE)a[1], (DWORD)a[2]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetRenderState(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetRenderState((D3DRENDERSTATETYPE)a[1], (DWORD*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_CreateStateBlock(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DStateBlock9 *o2 = nullptr;
    HRESULT r = self->CreateStateBlock((D3DSTATEBLOCKTYPE)a[1], &o2);
    rt_com_out(a[2], o2, 15);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_BeginStateBlock(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->BeginStateBlock();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_EndStateBlock(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DStateBlock9 *o1 = nullptr;
    HRESULT r = self->EndStateBlock(&o1);
    rt_com_out(a[1], o1, 15);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetClipStatus(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetClipStatus((const D3DCLIPSTATUS9 *)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetClipStatus(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetClipStatus((D3DCLIPSTATUS9*)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetTexture(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DBaseTexture9 *o2 = nullptr;
    HRESULT r = self->GetTexture((DWORD)a[1], &o2);
    rt_com_out(a[2], o2, 8);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetTexture(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetTexture((DWORD)a[1], (IDirect3DBaseTexture9 *)rt_com_host(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetTextureStageState(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetTextureStageState((DWORD)a[1], (D3DTEXTURESTAGESTATETYPE)a[2], (DWORD*)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetTextureStageState(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetTextureStageState((DWORD)a[1], (D3DTEXTURESTAGESTATETYPE)a[2], (DWORD)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetSamplerState(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetSamplerState((DWORD)a[1], (D3DSAMPLERSTATETYPE)a[2], (DWORD*)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetSamplerState(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetSamplerState((DWORD)a[1], (D3DSAMPLERSTATETYPE)a[2], (DWORD)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_ValidateDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->ValidateDevice((DWORD*)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetPaletteEntries(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetPaletteEntries((UINT)a[1], (const PALETTEENTRY *)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetPaletteEntries(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetPaletteEntries((UINT)a[1], (PALETTEENTRY*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetCurrentTexturePalette(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetCurrentTexturePalette((UINT)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetCurrentTexturePalette(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetCurrentTexturePalette((UINT *)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetScissorRect(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetScissorRect((const RECT *)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetScissorRect(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetScissorRect((RECT*)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetSoftwareVertexProcessing(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetSoftwareVertexProcessing((WINBOOL)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetSoftwareVertexProcessing(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    WINBOOL r = self->GetSoftwareVertexProcessing();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetNPatchMode(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetNPatchMode(rt_u2f(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetNPatchMode(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    float r = self->GetNPatchMode();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_DrawPrimitive(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->DrawPrimitive((D3DPRIMITIVETYPE)a[1], (UINT)a[2], (UINT)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_DrawIndexedPrimitive(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->DrawIndexedPrimitive((D3DPRIMITIVETYPE)a[1], (INT)a[2], (UINT)a[3], (UINT)a[4], (UINT)a[5], (UINT)a[6]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_DrawPrimitiveUP(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->DrawPrimitiveUP((D3DPRIMITIVETYPE)a[1], (UINT)a[2], (const void *)rt_gptr(a[3]), (UINT)a[4]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_DrawIndexedPrimitiveUP(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->DrawIndexedPrimitiveUP((D3DPRIMITIVETYPE)a[1], (UINT)a[2], (UINT)a[3], (UINT)a[4], (const void *)rt_gptr(a[5]), (D3DFORMAT)a[6], (const void *)rt_gptr(a[7]), (UINT)a[8]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_ProcessVertices(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->ProcessVertices((UINT)a[1], (UINT)a[2], (UINT)a[3], (IDirect3DVertexBuffer9 *)rt_com_host(a[4]), (IDirect3DVertexDeclaration9 *)rt_com_host(a[5]), (DWORD)a[6]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_CreateVertexDeclaration(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DVertexDeclaration9 *o2 = nullptr;
    HRESULT r = self->CreateVertexDeclaration((const D3DVERTEXELEMENT9 *)rt_gptr(a[1]), &o2);
    rt_com_out(a[2], o2, 12);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetVertexDeclaration(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetVertexDeclaration((IDirect3DVertexDeclaration9 *)rt_com_host(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetVertexDeclaration(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DVertexDeclaration9 *o1 = nullptr;
    HRESULT r = self->GetVertexDeclaration(&o1);
    rt_com_out(a[1], o1, 12);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetFVF(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetFVF((DWORD)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetFVF(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetFVF((DWORD*)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_CreateVertexShader(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DVertexShader9 *o2 = nullptr;
    HRESULT r = self->CreateVertexShader((const DWORD *)rt_gptr(a[1]), &o2);
    rt_com_out(a[2], o2, 13);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetVertexShader(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetVertexShader((IDirect3DVertexShader9 *)rt_com_host(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetVertexShader(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DVertexShader9 *o1 = nullptr;
    HRESULT r = self->GetVertexShader(&o1);
    rt_com_out(a[1], o1, 13);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetVertexShaderConstantF(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetVertexShaderConstantF((UINT)a[1], (const float *)rt_gptr(a[2]), (UINT)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetVertexShaderConstantF(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetVertexShaderConstantF((UINT)a[1], (float*)rt_gptr(a[2]), (UINT)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetVertexShaderConstantI(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetVertexShaderConstantI((UINT)a[1], (const int *)rt_gptr(a[2]), (UINT)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetVertexShaderConstantI(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetVertexShaderConstantI((UINT)a[1], (int*)rt_gptr(a[2]), (UINT)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetVertexShaderConstantB(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetVertexShaderConstantB((UINT)a[1], (const WINBOOL *)rt_gptr(a[2]), (UINT)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetVertexShaderConstantB(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetVertexShaderConstantB((UINT)a[1], (WINBOOL*)rt_gptr(a[2]), (UINT)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetStreamSource(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetStreamSource((UINT)a[1], (IDirect3DVertexBuffer9 *)rt_com_host(a[2]), (UINT)a[3], (UINT)a[4]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetStreamSource(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DVertexBuffer9 *o2 = nullptr;
    HRESULT r = self->GetStreamSource((UINT)a[1], &o2, (UINT*)rt_gptr(a[3]), (UINT*)rt_gptr(a[4]));
    rt_com_out(a[2], o2, 6);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetStreamSourceFreq(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetStreamSourceFreq((UINT)a[1], (UINT)a[2]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetStreamSourceFreq(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetStreamSourceFreq((UINT)a[1], (UINT*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetIndices(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetIndices((IDirect3DIndexBuffer9 *)rt_com_host(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetIndices(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DIndexBuffer9 *o1 = nullptr;
    HRESULT r = self->GetIndices(&o1);
    rt_com_out(a[1], o1, 7);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_CreatePixelShader(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DPixelShader9 *o2 = nullptr;
    HRESULT r = self->CreatePixelShader((const DWORD *)rt_gptr(a[1]), &o2);
    rt_com_out(a[2], o2, 14);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetPixelShader(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetPixelShader((IDirect3DPixelShader9 *)rt_com_host(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetPixelShader(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DPixelShader9 *o1 = nullptr;
    HRESULT r = self->GetPixelShader(&o1);
    rt_com_out(a[1], o1, 14);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetPixelShaderConstantF(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetPixelShaderConstantF((UINT)a[1], (const float *)rt_gptr(a[2]), (UINT)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetPixelShaderConstantF(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetPixelShaderConstantF((UINT)a[1], (float*)rt_gptr(a[2]), (UINT)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetPixelShaderConstantI(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetPixelShaderConstantI((UINT)a[1], (const int *)rt_gptr(a[2]), (UINT)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetPixelShaderConstantI(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetPixelShaderConstantI((UINT)a[1], (int*)rt_gptr(a[2]), (UINT)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_SetPixelShaderConstantB(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->SetPixelShaderConstantB((UINT)a[1], (const WINBOOL *)rt_gptr(a[2]), (UINT)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_GetPixelShaderConstantB(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->GetPixelShaderConstantB((UINT)a[1], (WINBOOL*)rt_gptr(a[2]), (UINT)a[3]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_DrawRectPatch(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->DrawRectPatch((UINT)a[1], (const float *)rt_gptr(a[2]), (const D3DRECTPATCH_INFO *)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_DrawTriPatch(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->DrawTriPatch((UINT)a[1], (const float *)rt_gptr(a[2]), (const D3DTRIPATCH_INFO *)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_DeletePatch(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    HRESULT r = self->DeletePatch((UINT)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DDevice9_CreateQuery(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DDevice9 *self = rt_self<IDirect3DDevice9>(a[0]);
    IDirect3DQuery9 *o2 = nullptr;
    HRESULT r = self->CreateQuery((D3DQUERYTYPE)a[1], &o2);
    rt_com_out(a[2], o2, 16);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DResource9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DResource9 *self = rt_self<IDirect3DResource9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DResource9_SetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DResource9 *self = rt_self<IDirect3DResource9>(a[0]);
    HRESULT r = self->SetPrivateData((REFGUID)a[1], (const void *)rt_gptr(a[2]), (DWORD)a[3], (DWORD)a[4]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DResource9_GetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DResource9 *self = rt_self<IDirect3DResource9>(a[0]);
    HRESULT r = self->GetPrivateData((REFGUID)a[1], (void*)rt_gptr(a[2]), (DWORD*)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DResource9_FreePrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DResource9 *self = rt_self<IDirect3DResource9>(a[0]);
    HRESULT r = self->FreePrivateData((REFGUID)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DResource9_SetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DResource9 *self = rt_self<IDirect3DResource9>(a[0]);
    DWORD r = self->SetPriority((DWORD)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DResource9_GetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DResource9 *self = rt_self<IDirect3DResource9>(a[0]);
    DWORD r = self->GetPriority();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DResource9_PreLoad(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DResource9 *self = rt_self<IDirect3DResource9>(a[0]);
    self->PreLoad();
    return 0;
}
static uint32_t m_IDirect3DResource9_GetType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DResource9 *self = rt_self<IDirect3DResource9>(a[0]);
    D3DRESOURCETYPE r = self->GetType();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSwapChain9_Present(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSwapChain9 *self = rt_self<IDirect3DSwapChain9>(a[0]);
    HRESULT r = self->Present((const RECT *)rt_gptr(a[1]), (const RECT *)rt_gptr(a[2]), rt_hwnd(a[3]), (const RGNDATA *)rt_gptr(a[4]), (DWORD)a[5]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSwapChain9_GetFrontBufferData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSwapChain9 *self = rt_self<IDirect3DSwapChain9>(a[0]);
    HRESULT r = self->GetFrontBufferData((IDirect3DSurface9 *)rt_com_host(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSwapChain9_GetBackBuffer(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSwapChain9 *self = rt_self<IDirect3DSwapChain9>(a[0]);
    IDirect3DSurface9 *o3 = nullptr;
    HRESULT r = self->GetBackBuffer((UINT)a[1], (D3DBACKBUFFER_TYPE)a[2], &o3);
    rt_com_out(a[3], o3, 4);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSwapChain9_GetRasterStatus(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSwapChain9 *self = rt_self<IDirect3DSwapChain9>(a[0]);
    HRESULT r = self->GetRasterStatus((D3DRASTER_STATUS *)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSwapChain9_GetDisplayMode(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSwapChain9 *self = rt_self<IDirect3DSwapChain9>(a[0]);
    HRESULT r = self->GetDisplayMode((D3DDISPLAYMODE *)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSwapChain9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSwapChain9 *self = rt_self<IDirect3DSwapChain9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSwapChain9_GetPresentParameters(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSwapChain9 *self = rt_self<IDirect3DSwapChain9>(a[0]);
    D3DPRESENT_PARAMETERS pp1;
    if (a[1]) rt_pp_in(a[1], pp1);
    HRESULT r = self->GetPresentParameters(a[1] ? &pp1 : nullptr);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSurface9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSurface9 *self = rt_self<IDirect3DSurface9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSurface9_SetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSurface9 *self = rt_self<IDirect3DSurface9>(a[0]);
    HRESULT r = self->SetPrivateData((REFGUID)a[1], (const void *)rt_gptr(a[2]), (DWORD)a[3], (DWORD)a[4]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSurface9_GetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSurface9 *self = rt_self<IDirect3DSurface9>(a[0]);
    HRESULT r = self->GetPrivateData((REFGUID)a[1], (void*)rt_gptr(a[2]), (DWORD*)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSurface9_FreePrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSurface9 *self = rt_self<IDirect3DSurface9>(a[0]);
    HRESULT r = self->FreePrivateData((REFGUID)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSurface9_SetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSurface9 *self = rt_self<IDirect3DSurface9>(a[0]);
    DWORD r = self->SetPriority((DWORD)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSurface9_GetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSurface9 *self = rt_self<IDirect3DSurface9>(a[0]);
    DWORD r = self->GetPriority();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSurface9_PreLoad(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSurface9 *self = rt_self<IDirect3DSurface9>(a[0]);
    self->PreLoad();
    return 0;
}
static uint32_t m_IDirect3DSurface9_GetType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSurface9 *self = rt_self<IDirect3DSurface9>(a[0]);
    D3DRESOURCETYPE r = self->GetType();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSurface9_GetContainer(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSurface9 *self = rt_self<IDirect3DSurface9>(a[0]);
    HRESULT r = self->GetContainer((REFIID)a[1], (void**)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSurface9_GetDesc(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSurface9 *self = rt_self<IDirect3DSurface9>(a[0]);
    HRESULT r = self->GetDesc((D3DSURFACE_DESC*)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSurface9_GetDC(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSurface9 *self = rt_self<IDirect3DSurface9>(a[0]);
    HRESULT r = self->GetDC((HDC*)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DSurface9_ReleaseDC(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DSurface9 *self = rt_self<IDirect3DSurface9>(a[0]);
    HRESULT r = self->ReleaseDC((HDC)(uintptr_t)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolume9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolume9 *self = rt_self<IDirect3DVolume9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolume9_SetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolume9 *self = rt_self<IDirect3DVolume9>(a[0]);
    HRESULT r = self->SetPrivateData((REFGUID)a[1], (const void *)rt_gptr(a[2]), (DWORD)a[3], (DWORD)a[4]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolume9_GetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolume9 *self = rt_self<IDirect3DVolume9>(a[0]);
    HRESULT r = self->GetPrivateData((REFGUID)a[1], (void*)rt_gptr(a[2]), (DWORD*)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolume9_FreePrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolume9 *self = rt_self<IDirect3DVolume9>(a[0]);
    HRESULT r = self->FreePrivateData((REFGUID)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolume9_GetContainer(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolume9 *self = rt_self<IDirect3DVolume9>(a[0]);
    HRESULT r = self->GetContainer((REFIID)a[1], (void**)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolume9_GetDesc(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolume9 *self = rt_self<IDirect3DVolume9>(a[0]);
    HRESULT r = self->GetDesc((D3DVOLUME_DESC*)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVertexBuffer9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVertexBuffer9 *self = rt_self<IDirect3DVertexBuffer9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVertexBuffer9_SetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVertexBuffer9 *self = rt_self<IDirect3DVertexBuffer9>(a[0]);
    HRESULT r = self->SetPrivateData((REFGUID)a[1], (const void *)rt_gptr(a[2]), (DWORD)a[3], (DWORD)a[4]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVertexBuffer9_GetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVertexBuffer9 *self = rt_self<IDirect3DVertexBuffer9>(a[0]);
    HRESULT r = self->GetPrivateData((REFGUID)a[1], (void*)rt_gptr(a[2]), (DWORD*)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVertexBuffer9_FreePrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVertexBuffer9 *self = rt_self<IDirect3DVertexBuffer9>(a[0]);
    HRESULT r = self->FreePrivateData((REFGUID)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVertexBuffer9_SetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVertexBuffer9 *self = rt_self<IDirect3DVertexBuffer9>(a[0]);
    DWORD r = self->SetPriority((DWORD)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVertexBuffer9_GetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVertexBuffer9 *self = rt_self<IDirect3DVertexBuffer9>(a[0]);
    DWORD r = self->GetPriority();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVertexBuffer9_PreLoad(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVertexBuffer9 *self = rt_self<IDirect3DVertexBuffer9>(a[0]);
    self->PreLoad();
    return 0;
}
static uint32_t m_IDirect3DVertexBuffer9_GetType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVertexBuffer9 *self = rt_self<IDirect3DVertexBuffer9>(a[0]);
    D3DRESOURCETYPE r = self->GetType();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVertexBuffer9_GetDesc(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVertexBuffer9 *self = rt_self<IDirect3DVertexBuffer9>(a[0]);
    HRESULT r = self->GetDesc((D3DVERTEXBUFFER_DESC*)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DIndexBuffer9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DIndexBuffer9 *self = rt_self<IDirect3DIndexBuffer9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DIndexBuffer9_SetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DIndexBuffer9 *self = rt_self<IDirect3DIndexBuffer9>(a[0]);
    HRESULT r = self->SetPrivateData((REFGUID)a[1], (const void *)rt_gptr(a[2]), (DWORD)a[3], (DWORD)a[4]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DIndexBuffer9_GetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DIndexBuffer9 *self = rt_self<IDirect3DIndexBuffer9>(a[0]);
    HRESULT r = self->GetPrivateData((REFGUID)a[1], (void*)rt_gptr(a[2]), (DWORD*)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DIndexBuffer9_FreePrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DIndexBuffer9 *self = rt_self<IDirect3DIndexBuffer9>(a[0]);
    HRESULT r = self->FreePrivateData((REFGUID)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DIndexBuffer9_SetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DIndexBuffer9 *self = rt_self<IDirect3DIndexBuffer9>(a[0]);
    DWORD r = self->SetPriority((DWORD)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DIndexBuffer9_GetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DIndexBuffer9 *self = rt_self<IDirect3DIndexBuffer9>(a[0]);
    DWORD r = self->GetPriority();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DIndexBuffer9_PreLoad(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DIndexBuffer9 *self = rt_self<IDirect3DIndexBuffer9>(a[0]);
    self->PreLoad();
    return 0;
}
static uint32_t m_IDirect3DIndexBuffer9_GetType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DIndexBuffer9 *self = rt_self<IDirect3DIndexBuffer9>(a[0]);
    D3DRESOURCETYPE r = self->GetType();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DIndexBuffer9_GetDesc(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DIndexBuffer9 *self = rt_self<IDirect3DIndexBuffer9>(a[0]);
    HRESULT r = self->GetDesc((D3DINDEXBUFFER_DESC*)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DBaseTexture9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DBaseTexture9 *self = rt_self<IDirect3DBaseTexture9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DBaseTexture9_SetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DBaseTexture9 *self = rt_self<IDirect3DBaseTexture9>(a[0]);
    HRESULT r = self->SetPrivateData((REFGUID)a[1], (const void *)rt_gptr(a[2]), (DWORD)a[3], (DWORD)a[4]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DBaseTexture9_GetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DBaseTexture9 *self = rt_self<IDirect3DBaseTexture9>(a[0]);
    HRESULT r = self->GetPrivateData((REFGUID)a[1], (void*)rt_gptr(a[2]), (DWORD*)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DBaseTexture9_FreePrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DBaseTexture9 *self = rt_self<IDirect3DBaseTexture9>(a[0]);
    HRESULT r = self->FreePrivateData((REFGUID)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DBaseTexture9_SetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DBaseTexture9 *self = rt_self<IDirect3DBaseTexture9>(a[0]);
    DWORD r = self->SetPriority((DWORD)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DBaseTexture9_GetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DBaseTexture9 *self = rt_self<IDirect3DBaseTexture9>(a[0]);
    DWORD r = self->GetPriority();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DBaseTexture9_PreLoad(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DBaseTexture9 *self = rt_self<IDirect3DBaseTexture9>(a[0]);
    self->PreLoad();
    return 0;
}
static uint32_t m_IDirect3DBaseTexture9_GetType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DBaseTexture9 *self = rt_self<IDirect3DBaseTexture9>(a[0]);
    D3DRESOURCETYPE r = self->GetType();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DBaseTexture9_SetLOD(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DBaseTexture9 *self = rt_self<IDirect3DBaseTexture9>(a[0]);
    DWORD r = self->SetLOD((DWORD)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DBaseTexture9_GetLOD(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DBaseTexture9 *self = rt_self<IDirect3DBaseTexture9>(a[0]);
    DWORD r = self->GetLOD();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DBaseTexture9_GetLevelCount(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DBaseTexture9 *self = rt_self<IDirect3DBaseTexture9>(a[0]);
    DWORD r = self->GetLevelCount();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DBaseTexture9_SetAutoGenFilterType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DBaseTexture9 *self = rt_self<IDirect3DBaseTexture9>(a[0]);
    HRESULT r = self->SetAutoGenFilterType((D3DTEXTUREFILTERTYPE)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DBaseTexture9_GetAutoGenFilterType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DBaseTexture9 *self = rt_self<IDirect3DBaseTexture9>(a[0]);
    D3DTEXTUREFILTERTYPE r = self->GetAutoGenFilterType();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DBaseTexture9_GenerateMipSubLevels(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DBaseTexture9 *self = rt_self<IDirect3DBaseTexture9>(a[0]);
    self->GenerateMipSubLevels();
    return 0;
}
static uint32_t m_IDirect3DTexture9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DTexture9_SetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    HRESULT r = self->SetPrivateData((REFGUID)a[1], (const void *)rt_gptr(a[2]), (DWORD)a[3], (DWORD)a[4]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DTexture9_GetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    HRESULT r = self->GetPrivateData((REFGUID)a[1], (void*)rt_gptr(a[2]), (DWORD*)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DTexture9_FreePrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    HRESULT r = self->FreePrivateData((REFGUID)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DTexture9_SetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    DWORD r = self->SetPriority((DWORD)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DTexture9_GetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    DWORD r = self->GetPriority();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DTexture9_PreLoad(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    self->PreLoad();
    return 0;
}
static uint32_t m_IDirect3DTexture9_GetType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    D3DRESOURCETYPE r = self->GetType();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DTexture9_SetLOD(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    DWORD r = self->SetLOD((DWORD)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DTexture9_GetLOD(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    DWORD r = self->GetLOD();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DTexture9_GetLevelCount(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    DWORD r = self->GetLevelCount();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DTexture9_SetAutoGenFilterType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    HRESULT r = self->SetAutoGenFilterType((D3DTEXTUREFILTERTYPE)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DTexture9_GetAutoGenFilterType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    D3DTEXTUREFILTERTYPE r = self->GetAutoGenFilterType();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DTexture9_GenerateMipSubLevels(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    self->GenerateMipSubLevels();
    return 0;
}
static uint32_t m_IDirect3DTexture9_GetLevelDesc(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    HRESULT r = self->GetLevelDesc((UINT)a[1], (D3DSURFACE_DESC*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DTexture9_GetSurfaceLevel(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    IDirect3DSurface9 *o2 = nullptr;
    HRESULT r = self->GetSurfaceLevel((UINT)a[1], &o2);
    rt_com_out(a[2], o2, 4);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DTexture9_AddDirtyRect(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DTexture9 *self = rt_self<IDirect3DTexture9>(a[0]);
    HRESULT r = self->AddDirtyRect((const RECT *)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_SetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    HRESULT r = self->SetPrivateData((REFGUID)a[1], (const void *)rt_gptr(a[2]), (DWORD)a[3], (DWORD)a[4]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_GetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    HRESULT r = self->GetPrivateData((REFGUID)a[1], (void*)rt_gptr(a[2]), (DWORD*)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_FreePrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    HRESULT r = self->FreePrivateData((REFGUID)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_SetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    DWORD r = self->SetPriority((DWORD)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_GetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    DWORD r = self->GetPriority();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_PreLoad(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    self->PreLoad();
    return 0;
}
static uint32_t m_IDirect3DCubeTexture9_GetType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    D3DRESOURCETYPE r = self->GetType();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_SetLOD(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    DWORD r = self->SetLOD((DWORD)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_GetLOD(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    DWORD r = self->GetLOD();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_GetLevelCount(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    DWORD r = self->GetLevelCount();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_SetAutoGenFilterType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    HRESULT r = self->SetAutoGenFilterType((D3DTEXTUREFILTERTYPE)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_GetAutoGenFilterType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    D3DTEXTUREFILTERTYPE r = self->GetAutoGenFilterType();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_GenerateMipSubLevels(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    self->GenerateMipSubLevels();
    return 0;
}
static uint32_t m_IDirect3DCubeTexture9_GetLevelDesc(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    HRESULT r = self->GetLevelDesc((UINT)a[1], (D3DSURFACE_DESC*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_GetCubeMapSurface(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    IDirect3DSurface9 *o3 = nullptr;
    HRESULT r = self->GetCubeMapSurface((D3DCUBEMAP_FACES)a[1], (UINT)a[2], &o3);
    rt_com_out(a[3], o3, 4);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DCubeTexture9_AddDirtyRect(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DCubeTexture9 *self = rt_self<IDirect3DCubeTexture9>(a[0]);
    HRESULT r = self->AddDirtyRect((D3DCUBEMAP_FACES)a[1], (const RECT *)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_SetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    HRESULT r = self->SetPrivateData((REFGUID)a[1], (const void *)rt_gptr(a[2]), (DWORD)a[3], (DWORD)a[4]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_GetPrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    HRESULT r = self->GetPrivateData((REFGUID)a[1], (void*)rt_gptr(a[2]), (DWORD*)rt_gptr(a[3]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_FreePrivateData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    HRESULT r = self->FreePrivateData((REFGUID)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_SetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    DWORD r = self->SetPriority((DWORD)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_GetPriority(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    DWORD r = self->GetPriority();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_PreLoad(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    self->PreLoad();
    return 0;
}
static uint32_t m_IDirect3DVolumeTexture9_GetType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    D3DRESOURCETYPE r = self->GetType();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_SetLOD(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    DWORD r = self->SetLOD((DWORD)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_GetLOD(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    DWORD r = self->GetLOD();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_GetLevelCount(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    DWORD r = self->GetLevelCount();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_SetAutoGenFilterType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    HRESULT r = self->SetAutoGenFilterType((D3DTEXTUREFILTERTYPE)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_GetAutoGenFilterType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    D3DTEXTUREFILTERTYPE r = self->GetAutoGenFilterType();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_GenerateMipSubLevels(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    self->GenerateMipSubLevels();
    return 0;
}
static uint32_t m_IDirect3DVolumeTexture9_GetLevelDesc(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    HRESULT r = self->GetLevelDesc((UINT)a[1], (D3DVOLUME_DESC *)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_GetVolumeLevel(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    IDirect3DVolume9 *o2 = nullptr;
    HRESULT r = self->GetVolumeLevel((UINT)a[1], &o2);
    rt_com_out(a[2], o2, 5);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVolumeTexture9_AddDirtyBox(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVolumeTexture9 *self = rt_self<IDirect3DVolumeTexture9>(a[0]);
    HRESULT r = self->AddDirtyBox((const D3DBOX *)rt_gptr(a[1]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVertexDeclaration9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVertexDeclaration9 *self = rt_self<IDirect3DVertexDeclaration9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVertexDeclaration9_GetDeclaration(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVertexDeclaration9 *self = rt_self<IDirect3DVertexDeclaration9>(a[0]);
    HRESULT r = self->GetDeclaration((D3DVERTEXELEMENT9*)rt_gptr(a[1]), (UINT*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVertexShader9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVertexShader9 *self = rt_self<IDirect3DVertexShader9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DVertexShader9_GetFunction(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DVertexShader9 *self = rt_self<IDirect3DVertexShader9>(a[0]);
    HRESULT r = self->GetFunction((void*)rt_gptr(a[1]), (UINT*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DPixelShader9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DPixelShader9 *self = rt_self<IDirect3DPixelShader9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DPixelShader9_GetFunction(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DPixelShader9 *self = rt_self<IDirect3DPixelShader9>(a[0]);
    HRESULT r = self->GetFunction((void*)rt_gptr(a[1]), (UINT*)rt_gptr(a[2]));
    return (uint32_t)r;
}
static uint32_t m_IDirect3DStateBlock9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DStateBlock9 *self = rt_self<IDirect3DStateBlock9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DStateBlock9_Capture(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DStateBlock9 *self = rt_self<IDirect3DStateBlock9>(a[0]);
    HRESULT r = self->Capture();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DStateBlock9_Apply(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DStateBlock9 *self = rt_self<IDirect3DStateBlock9>(a[0]);
    HRESULT r = self->Apply();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DQuery9_GetDevice(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DQuery9 *self = rt_self<IDirect3DQuery9>(a[0]);
    IDirect3DDevice9 *o1 = nullptr;
    HRESULT r = self->GetDevice(&o1);
    rt_com_out(a[1], o1, 1);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DQuery9_GetType(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DQuery9 *self = rt_self<IDirect3DQuery9>(a[0]);
    D3DQUERYTYPE r = self->GetType();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DQuery9_GetDataSize(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DQuery9 *self = rt_self<IDirect3DQuery9>(a[0]);
    DWORD r = self->GetDataSize();
    return (uint32_t)r;
}
static uint32_t m_IDirect3DQuery9_Issue(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DQuery9 *self = rt_self<IDirect3DQuery9>(a[0]);
    HRESULT r = self->Issue((DWORD)a[1]);
    return (uint32_t)r;
}
static uint32_t m_IDirect3DQuery9_GetData(cpu_t *c, const uint32_t *a)
{
    (void)c;
    IDirect3DQuery9 *self = rt_self<IDirect3DQuery9>(a[0]);
    HRESULT r = self->GetData((void*)rt_gptr(a[1]), (DWORD)a[2], (DWORD)a[3]);
    return (uint32_t)r;
}

static const rt_shim_entry d3d9_IDirect3D9[] = {
    {"d3d9com::IDirect3D9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3D9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3D9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3D9::RegisterSoftwareDevice", 2, m_IDirect3D9_RegisterSoftwareDevice},
    {"d3d9com::IDirect3D9::GetAdapterCount", 1, m_IDirect3D9_GetAdapterCount},
    {"d3d9com::IDirect3D9::GetAdapterIdentifier", 4, m_IDirect3D9_GetAdapterIdentifier},
    {"d3d9com::IDirect3D9::GetAdapterModeCount", 3, m_IDirect3D9_GetAdapterModeCount},
    {"d3d9com::IDirect3D9::EnumAdapterModes", 5, m_IDirect3D9_EnumAdapterModes},
    {"d3d9com::IDirect3D9::GetAdapterDisplayMode", 3, m_IDirect3D9_GetAdapterDisplayMode},
    {"d3d9com::IDirect3D9::CheckDeviceType", 6, m_IDirect3D9_CheckDeviceType},
    {"d3d9com::IDirect3D9::CheckDeviceFormat", 7, m_IDirect3D9_CheckDeviceFormat},
    {"d3d9com::IDirect3D9::CheckDeviceMultiSampleType", 7, m_IDirect3D9_CheckDeviceMultiSampleType},
    {"d3d9com::IDirect3D9::CheckDepthStencilMatch", 6, m_IDirect3D9_CheckDepthStencilMatch},
    {"d3d9com::IDirect3D9::CheckDeviceFormatConversion", 5, m_IDirect3D9_CheckDeviceFormatConversion},
    {"d3d9com::IDirect3D9::GetDeviceCaps", 4, m_IDirect3D9_GetDeviceCaps},
    {"d3d9com::IDirect3D9::GetAdapterMonitor", 2, custom_IDirect3D9_GetAdapterMonitor},
    {"d3d9com::IDirect3D9::CreateDevice", 7, custom_IDirect3D9_CreateDevice},
};

static const rt_shim_entry d3d9_IDirect3DDevice9[] = {
    {"d3d9com::IDirect3DDevice9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DDevice9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DDevice9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DDevice9::TestCooperativeLevel", 1, m_IDirect3DDevice9_TestCooperativeLevel},
    {"d3d9com::IDirect3DDevice9::GetAvailableTextureMem", 1, m_IDirect3DDevice9_GetAvailableTextureMem},
    {"d3d9com::IDirect3DDevice9::EvictManagedResources", 1, m_IDirect3DDevice9_EvictManagedResources},
    {"d3d9com::IDirect3DDevice9::GetDirect3D", 2, m_IDirect3DDevice9_GetDirect3D},
    {"d3d9com::IDirect3DDevice9::GetDeviceCaps", 2, m_IDirect3DDevice9_GetDeviceCaps},
    {"d3d9com::IDirect3DDevice9::GetDisplayMode", 3, m_IDirect3DDevice9_GetDisplayMode},
    {"d3d9com::IDirect3DDevice9::GetCreationParameters", 2, custom_IDirect3DDevice9_GetCreationParameters},
    {"d3d9com::IDirect3DDevice9::SetCursorProperties", 4, m_IDirect3DDevice9_SetCursorProperties},
    {"d3d9com::IDirect3DDevice9::SetCursorPosition", 4, m_IDirect3DDevice9_SetCursorPosition},
    {"d3d9com::IDirect3DDevice9::ShowCursor", 2, m_IDirect3DDevice9_ShowCursor},
    {"d3d9com::IDirect3DDevice9::CreateAdditionalSwapChain", 3, m_IDirect3DDevice9_CreateAdditionalSwapChain},
    {"d3d9com::IDirect3DDevice9::GetSwapChain", 3, m_IDirect3DDevice9_GetSwapChain},
    {"d3d9com::IDirect3DDevice9::GetNumberOfSwapChains", 1, m_IDirect3DDevice9_GetNumberOfSwapChains},
    {"d3d9com::IDirect3DDevice9::Reset", 2, custom_IDirect3DDevice9_Reset},
    {"d3d9com::IDirect3DDevice9::Present", 5, custom_IDirect3DDevice9_Present},
    {"d3d9com::IDirect3DDevice9::GetBackBuffer", 5, m_IDirect3DDevice9_GetBackBuffer},
    {"d3d9com::IDirect3DDevice9::GetRasterStatus", 3, m_IDirect3DDevice9_GetRasterStatus},
    {"d3d9com::IDirect3DDevice9::SetDialogBoxMode", 2, m_IDirect3DDevice9_SetDialogBoxMode},
    {"d3d9com::IDirect3DDevice9::SetGammaRamp", 4, m_IDirect3DDevice9_SetGammaRamp},
    {"d3d9com::IDirect3DDevice9::GetGammaRamp", 3, m_IDirect3DDevice9_GetGammaRamp},
    {"d3d9com::IDirect3DDevice9::CreateTexture", 9, custom_IDirect3DDevice9_CreateTexture},
    {"d3d9com::IDirect3DDevice9::CreateVolumeTexture", 10, m_IDirect3DDevice9_CreateVolumeTexture},
    {"d3d9com::IDirect3DDevice9::CreateCubeTexture", 8, m_IDirect3DDevice9_CreateCubeTexture},
    {"d3d9com::IDirect3DDevice9::CreateVertexBuffer", 7, custom_IDirect3DDevice9_CreateVertexBuffer},
    {"d3d9com::IDirect3DDevice9::CreateIndexBuffer", 7, custom_IDirect3DDevice9_CreateIndexBuffer},
    {"d3d9com::IDirect3DDevice9::CreateRenderTarget", 9, m_IDirect3DDevice9_CreateRenderTarget},
    {"d3d9com::IDirect3DDevice9::CreateDepthStencilSurface", 9, m_IDirect3DDevice9_CreateDepthStencilSurface},
    {"d3d9com::IDirect3DDevice9::UpdateSurface", 5, m_IDirect3DDevice9_UpdateSurface},
    {"d3d9com::IDirect3DDevice9::UpdateTexture", 3, m_IDirect3DDevice9_UpdateTexture},
    {"d3d9com::IDirect3DDevice9::GetRenderTargetData", 3, m_IDirect3DDevice9_GetRenderTargetData},
    {"d3d9com::IDirect3DDevice9::GetFrontBufferData", 3, m_IDirect3DDevice9_GetFrontBufferData},
    {"d3d9com::IDirect3DDevice9::StretchRect", 6, m_IDirect3DDevice9_StretchRect},
    {"d3d9com::IDirect3DDevice9::ColorFill", 4, m_IDirect3DDevice9_ColorFill},
    {"d3d9com::IDirect3DDevice9::CreateOffscreenPlainSurface", 7, m_IDirect3DDevice9_CreateOffscreenPlainSurface},
    {"d3d9com::IDirect3DDevice9::SetRenderTarget", 3, m_IDirect3DDevice9_SetRenderTarget},
    {"d3d9com::IDirect3DDevice9::GetRenderTarget", 3, m_IDirect3DDevice9_GetRenderTarget},
    {"d3d9com::IDirect3DDevice9::SetDepthStencilSurface", 2, m_IDirect3DDevice9_SetDepthStencilSurface},
    {"d3d9com::IDirect3DDevice9::GetDepthStencilSurface", 2, m_IDirect3DDevice9_GetDepthStencilSurface},
    {"d3d9com::IDirect3DDevice9::BeginScene", 1, m_IDirect3DDevice9_BeginScene},
    {"d3d9com::IDirect3DDevice9::EndScene", 1, m_IDirect3DDevice9_EndScene},
    {"d3d9com::IDirect3DDevice9::Clear", 7, m_IDirect3DDevice9_Clear},
    {"d3d9com::IDirect3DDevice9::SetTransform", 3, m_IDirect3DDevice9_SetTransform},
    {"d3d9com::IDirect3DDevice9::GetTransform", 3, m_IDirect3DDevice9_GetTransform},
    {"d3d9com::IDirect3DDevice9::MultiplyTransform", 3, m_IDirect3DDevice9_MultiplyTransform},
    {"d3d9com::IDirect3DDevice9::SetViewport", 2, m_IDirect3DDevice9_SetViewport},
    {"d3d9com::IDirect3DDevice9::GetViewport", 2, m_IDirect3DDevice9_GetViewport},
    {"d3d9com::IDirect3DDevice9::SetMaterial", 2, m_IDirect3DDevice9_SetMaterial},
    {"d3d9com::IDirect3DDevice9::GetMaterial", 2, m_IDirect3DDevice9_GetMaterial},
    {"d3d9com::IDirect3DDevice9::SetLight", 3, m_IDirect3DDevice9_SetLight},
    {"d3d9com::IDirect3DDevice9::GetLight", 3, m_IDirect3DDevice9_GetLight},
    {"d3d9com::IDirect3DDevice9::LightEnable", 3, m_IDirect3DDevice9_LightEnable},
    {"d3d9com::IDirect3DDevice9::GetLightEnable", 3, m_IDirect3DDevice9_GetLightEnable},
    {"d3d9com::IDirect3DDevice9::SetClipPlane", 3, m_IDirect3DDevice9_SetClipPlane},
    {"d3d9com::IDirect3DDevice9::GetClipPlane", 3, m_IDirect3DDevice9_GetClipPlane},
    {"d3d9com::IDirect3DDevice9::SetRenderState", 3, m_IDirect3DDevice9_SetRenderState},
    {"d3d9com::IDirect3DDevice9::GetRenderState", 3, m_IDirect3DDevice9_GetRenderState},
    {"d3d9com::IDirect3DDevice9::CreateStateBlock", 3, m_IDirect3DDevice9_CreateStateBlock},
    {"d3d9com::IDirect3DDevice9::BeginStateBlock", 1, m_IDirect3DDevice9_BeginStateBlock},
    {"d3d9com::IDirect3DDevice9::EndStateBlock", 2, m_IDirect3DDevice9_EndStateBlock},
    {"d3d9com::IDirect3DDevice9::SetClipStatus", 2, m_IDirect3DDevice9_SetClipStatus},
    {"d3d9com::IDirect3DDevice9::GetClipStatus", 2, m_IDirect3DDevice9_GetClipStatus},
    {"d3d9com::IDirect3DDevice9::GetTexture", 3, m_IDirect3DDevice9_GetTexture},
    {"d3d9com::IDirect3DDevice9::SetTexture", 3, m_IDirect3DDevice9_SetTexture},
    {"d3d9com::IDirect3DDevice9::GetTextureStageState", 4, m_IDirect3DDevice9_GetTextureStageState},
    {"d3d9com::IDirect3DDevice9::SetTextureStageState", 4, m_IDirect3DDevice9_SetTextureStageState},
    {"d3d9com::IDirect3DDevice9::GetSamplerState", 4, m_IDirect3DDevice9_GetSamplerState},
    {"d3d9com::IDirect3DDevice9::SetSamplerState", 4, m_IDirect3DDevice9_SetSamplerState},
    {"d3d9com::IDirect3DDevice9::ValidateDevice", 2, m_IDirect3DDevice9_ValidateDevice},
    {"d3d9com::IDirect3DDevice9::SetPaletteEntries", 3, m_IDirect3DDevice9_SetPaletteEntries},
    {"d3d9com::IDirect3DDevice9::GetPaletteEntries", 3, m_IDirect3DDevice9_GetPaletteEntries},
    {"d3d9com::IDirect3DDevice9::SetCurrentTexturePalette", 2, m_IDirect3DDevice9_SetCurrentTexturePalette},
    {"d3d9com::IDirect3DDevice9::GetCurrentTexturePalette", 2, m_IDirect3DDevice9_GetCurrentTexturePalette},
    {"d3d9com::IDirect3DDevice9::SetScissorRect", 2, m_IDirect3DDevice9_SetScissorRect},
    {"d3d9com::IDirect3DDevice9::GetScissorRect", 2, m_IDirect3DDevice9_GetScissorRect},
    {"d3d9com::IDirect3DDevice9::SetSoftwareVertexProcessing", 2, m_IDirect3DDevice9_SetSoftwareVertexProcessing},
    {"d3d9com::IDirect3DDevice9::GetSoftwareVertexProcessing", 1, m_IDirect3DDevice9_GetSoftwareVertexProcessing},
    {"d3d9com::IDirect3DDevice9::SetNPatchMode", 2, m_IDirect3DDevice9_SetNPatchMode},
    {"d3d9com::IDirect3DDevice9::GetNPatchMode", 1, m_IDirect3DDevice9_GetNPatchMode},
    {"d3d9com::IDirect3DDevice9::DrawPrimitive", 4, m_IDirect3DDevice9_DrawPrimitive},
    {"d3d9com::IDirect3DDevice9::DrawIndexedPrimitive", 7, m_IDirect3DDevice9_DrawIndexedPrimitive},
    {"d3d9com::IDirect3DDevice9::DrawPrimitiveUP", 5, m_IDirect3DDevice9_DrawPrimitiveUP},
    {"d3d9com::IDirect3DDevice9::DrawIndexedPrimitiveUP", 9, m_IDirect3DDevice9_DrawIndexedPrimitiveUP},
    {"d3d9com::IDirect3DDevice9::ProcessVertices", 7, m_IDirect3DDevice9_ProcessVertices},
    {"d3d9com::IDirect3DDevice9::CreateVertexDeclaration", 3, m_IDirect3DDevice9_CreateVertexDeclaration},
    {"d3d9com::IDirect3DDevice9::SetVertexDeclaration", 2, m_IDirect3DDevice9_SetVertexDeclaration},
    {"d3d9com::IDirect3DDevice9::GetVertexDeclaration", 2, m_IDirect3DDevice9_GetVertexDeclaration},
    {"d3d9com::IDirect3DDevice9::SetFVF", 2, m_IDirect3DDevice9_SetFVF},
    {"d3d9com::IDirect3DDevice9::GetFVF", 2, m_IDirect3DDevice9_GetFVF},
    {"d3d9com::IDirect3DDevice9::CreateVertexShader", 3, m_IDirect3DDevice9_CreateVertexShader},
    {"d3d9com::IDirect3DDevice9::SetVertexShader", 2, m_IDirect3DDevice9_SetVertexShader},
    {"d3d9com::IDirect3DDevice9::GetVertexShader", 2, m_IDirect3DDevice9_GetVertexShader},
    {"d3d9com::IDirect3DDevice9::SetVertexShaderConstantF", 4, m_IDirect3DDevice9_SetVertexShaderConstantF},
    {"d3d9com::IDirect3DDevice9::GetVertexShaderConstantF", 4, m_IDirect3DDevice9_GetVertexShaderConstantF},
    {"d3d9com::IDirect3DDevice9::SetVertexShaderConstantI", 4, m_IDirect3DDevice9_SetVertexShaderConstantI},
    {"d3d9com::IDirect3DDevice9::GetVertexShaderConstantI", 4, m_IDirect3DDevice9_GetVertexShaderConstantI},
    {"d3d9com::IDirect3DDevice9::SetVertexShaderConstantB", 4, m_IDirect3DDevice9_SetVertexShaderConstantB},
    {"d3d9com::IDirect3DDevice9::GetVertexShaderConstantB", 4, m_IDirect3DDevice9_GetVertexShaderConstantB},
    {"d3d9com::IDirect3DDevice9::SetStreamSource", 5, m_IDirect3DDevice9_SetStreamSource},
    {"d3d9com::IDirect3DDevice9::GetStreamSource", 5, m_IDirect3DDevice9_GetStreamSource},
    {"d3d9com::IDirect3DDevice9::SetStreamSourceFreq", 3, m_IDirect3DDevice9_SetStreamSourceFreq},
    {"d3d9com::IDirect3DDevice9::GetStreamSourceFreq", 3, m_IDirect3DDevice9_GetStreamSourceFreq},
    {"d3d9com::IDirect3DDevice9::SetIndices", 2, m_IDirect3DDevice9_SetIndices},
    {"d3d9com::IDirect3DDevice9::GetIndices", 2, m_IDirect3DDevice9_GetIndices},
    {"d3d9com::IDirect3DDevice9::CreatePixelShader", 3, m_IDirect3DDevice9_CreatePixelShader},
    {"d3d9com::IDirect3DDevice9::SetPixelShader", 2, m_IDirect3DDevice9_SetPixelShader},
    {"d3d9com::IDirect3DDevice9::GetPixelShader", 2, m_IDirect3DDevice9_GetPixelShader},
    {"d3d9com::IDirect3DDevice9::SetPixelShaderConstantF", 4, m_IDirect3DDevice9_SetPixelShaderConstantF},
    {"d3d9com::IDirect3DDevice9::GetPixelShaderConstantF", 4, m_IDirect3DDevice9_GetPixelShaderConstantF},
    {"d3d9com::IDirect3DDevice9::SetPixelShaderConstantI", 4, m_IDirect3DDevice9_SetPixelShaderConstantI},
    {"d3d9com::IDirect3DDevice9::GetPixelShaderConstantI", 4, m_IDirect3DDevice9_GetPixelShaderConstantI},
    {"d3d9com::IDirect3DDevice9::SetPixelShaderConstantB", 4, m_IDirect3DDevice9_SetPixelShaderConstantB},
    {"d3d9com::IDirect3DDevice9::GetPixelShaderConstantB", 4, m_IDirect3DDevice9_GetPixelShaderConstantB},
    {"d3d9com::IDirect3DDevice9::DrawRectPatch", 4, m_IDirect3DDevice9_DrawRectPatch},
    {"d3d9com::IDirect3DDevice9::DrawTriPatch", 4, m_IDirect3DDevice9_DrawTriPatch},
    {"d3d9com::IDirect3DDevice9::DeletePatch", 2, m_IDirect3DDevice9_DeletePatch},
    {"d3d9com::IDirect3DDevice9::CreateQuery", 3, m_IDirect3DDevice9_CreateQuery},
};

static const rt_shim_entry d3d9_IDirect3DResource9[] = {
    {"d3d9com::IDirect3DResource9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DResource9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DResource9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DResource9::GetDevice", 2, m_IDirect3DResource9_GetDevice},
    {"d3d9com::IDirect3DResource9::SetPrivateData", 5, m_IDirect3DResource9_SetPrivateData},
    {"d3d9com::IDirect3DResource9::GetPrivateData", 4, m_IDirect3DResource9_GetPrivateData},
    {"d3d9com::IDirect3DResource9::FreePrivateData", 2, m_IDirect3DResource9_FreePrivateData},
    {"d3d9com::IDirect3DResource9::SetPriority", 2, m_IDirect3DResource9_SetPriority},
    {"d3d9com::IDirect3DResource9::GetPriority", 1, m_IDirect3DResource9_GetPriority},
    {"d3d9com::IDirect3DResource9::PreLoad", 1, m_IDirect3DResource9_PreLoad},
    {"d3d9com::IDirect3DResource9::GetType", 1, m_IDirect3DResource9_GetType},
};

static const rt_shim_entry d3d9_IDirect3DSwapChain9[] = {
    {"d3d9com::IDirect3DSwapChain9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DSwapChain9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DSwapChain9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DSwapChain9::Present", 6, m_IDirect3DSwapChain9_Present},
    {"d3d9com::IDirect3DSwapChain9::GetFrontBufferData", 2, m_IDirect3DSwapChain9_GetFrontBufferData},
    {"d3d9com::IDirect3DSwapChain9::GetBackBuffer", 4, m_IDirect3DSwapChain9_GetBackBuffer},
    {"d3d9com::IDirect3DSwapChain9::GetRasterStatus", 2, m_IDirect3DSwapChain9_GetRasterStatus},
    {"d3d9com::IDirect3DSwapChain9::GetDisplayMode", 2, m_IDirect3DSwapChain9_GetDisplayMode},
    {"d3d9com::IDirect3DSwapChain9::GetDevice", 2, m_IDirect3DSwapChain9_GetDevice},
    {"d3d9com::IDirect3DSwapChain9::GetPresentParameters", 2, m_IDirect3DSwapChain9_GetPresentParameters},
};

static const rt_shim_entry d3d9_IDirect3DSurface9[] = {
    {"d3d9com::IDirect3DSurface9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DSurface9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DSurface9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DSurface9::GetDevice", 2, m_IDirect3DSurface9_GetDevice},
    {"d3d9com::IDirect3DSurface9::SetPrivateData", 5, m_IDirect3DSurface9_SetPrivateData},
    {"d3d9com::IDirect3DSurface9::GetPrivateData", 4, m_IDirect3DSurface9_GetPrivateData},
    {"d3d9com::IDirect3DSurface9::FreePrivateData", 2, m_IDirect3DSurface9_FreePrivateData},
    {"d3d9com::IDirect3DSurface9::SetPriority", 2, m_IDirect3DSurface9_SetPriority},
    {"d3d9com::IDirect3DSurface9::GetPriority", 1, m_IDirect3DSurface9_GetPriority},
    {"d3d9com::IDirect3DSurface9::PreLoad", 1, m_IDirect3DSurface9_PreLoad},
    {"d3d9com::IDirect3DSurface9::GetType", 1, m_IDirect3DSurface9_GetType},
    {"d3d9com::IDirect3DSurface9::GetContainer", 3, m_IDirect3DSurface9_GetContainer},
    {"d3d9com::IDirect3DSurface9::GetDesc", 2, m_IDirect3DSurface9_GetDesc},
    {"d3d9com::IDirect3DSurface9::LockRect", 4, custom_IDirect3DSurface9_LockRect},
    {"d3d9com::IDirect3DSurface9::UnlockRect", 1, custom_IDirect3DSurface9_UnlockRect},
    {"d3d9com::IDirect3DSurface9::GetDC", 2, m_IDirect3DSurface9_GetDC},
    {"d3d9com::IDirect3DSurface9::ReleaseDC", 2, m_IDirect3DSurface9_ReleaseDC},
};

static const rt_shim_entry d3d9_IDirect3DVolume9[] = {
    {"d3d9com::IDirect3DVolume9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DVolume9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DVolume9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DVolume9::GetDevice", 2, m_IDirect3DVolume9_GetDevice},
    {"d3d9com::IDirect3DVolume9::SetPrivateData", 5, m_IDirect3DVolume9_SetPrivateData},
    {"d3d9com::IDirect3DVolume9::GetPrivateData", 4, m_IDirect3DVolume9_GetPrivateData},
    {"d3d9com::IDirect3DVolume9::FreePrivateData", 2, m_IDirect3DVolume9_FreePrivateData},
    {"d3d9com::IDirect3DVolume9::GetContainer", 3, m_IDirect3DVolume9_GetContainer},
    {"d3d9com::IDirect3DVolume9::GetDesc", 2, m_IDirect3DVolume9_GetDesc},
    {"d3d9com::IDirect3DVolume9::LockBox", 4, custom_IDirect3DVolume9_LockBox},
    {"d3d9com::IDirect3DVolume9::UnlockBox", 1, custom_IDirect3DVolume9_UnlockBox},
};

static const rt_shim_entry d3d9_IDirect3DVertexBuffer9[] = {
    {"d3d9com::IDirect3DVertexBuffer9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DVertexBuffer9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DVertexBuffer9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DVertexBuffer9::GetDevice", 2, m_IDirect3DVertexBuffer9_GetDevice},
    {"d3d9com::IDirect3DVertexBuffer9::SetPrivateData", 5, m_IDirect3DVertexBuffer9_SetPrivateData},
    {"d3d9com::IDirect3DVertexBuffer9::GetPrivateData", 4, m_IDirect3DVertexBuffer9_GetPrivateData},
    {"d3d9com::IDirect3DVertexBuffer9::FreePrivateData", 2, m_IDirect3DVertexBuffer9_FreePrivateData},
    {"d3d9com::IDirect3DVertexBuffer9::SetPriority", 2, m_IDirect3DVertexBuffer9_SetPriority},
    {"d3d9com::IDirect3DVertexBuffer9::GetPriority", 1, m_IDirect3DVertexBuffer9_GetPriority},
    {"d3d9com::IDirect3DVertexBuffer9::PreLoad", 1, m_IDirect3DVertexBuffer9_PreLoad},
    {"d3d9com::IDirect3DVertexBuffer9::GetType", 1, m_IDirect3DVertexBuffer9_GetType},
    {"d3d9com::IDirect3DVertexBuffer9::Lock", 5, custom_IDirect3DVertexBuffer9_Lock},
    {"d3d9com::IDirect3DVertexBuffer9::Unlock", 1, custom_IDirect3DVertexBuffer9_Unlock},
    {"d3d9com::IDirect3DVertexBuffer9::GetDesc", 2, m_IDirect3DVertexBuffer9_GetDesc},
};

static const rt_shim_entry d3d9_IDirect3DIndexBuffer9[] = {
    {"d3d9com::IDirect3DIndexBuffer9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DIndexBuffer9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DIndexBuffer9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DIndexBuffer9::GetDevice", 2, m_IDirect3DIndexBuffer9_GetDevice},
    {"d3d9com::IDirect3DIndexBuffer9::SetPrivateData", 5, m_IDirect3DIndexBuffer9_SetPrivateData},
    {"d3d9com::IDirect3DIndexBuffer9::GetPrivateData", 4, m_IDirect3DIndexBuffer9_GetPrivateData},
    {"d3d9com::IDirect3DIndexBuffer9::FreePrivateData", 2, m_IDirect3DIndexBuffer9_FreePrivateData},
    {"d3d9com::IDirect3DIndexBuffer9::SetPriority", 2, m_IDirect3DIndexBuffer9_SetPriority},
    {"d3d9com::IDirect3DIndexBuffer9::GetPriority", 1, m_IDirect3DIndexBuffer9_GetPriority},
    {"d3d9com::IDirect3DIndexBuffer9::PreLoad", 1, m_IDirect3DIndexBuffer9_PreLoad},
    {"d3d9com::IDirect3DIndexBuffer9::GetType", 1, m_IDirect3DIndexBuffer9_GetType},
    {"d3d9com::IDirect3DIndexBuffer9::Lock", 5, custom_IDirect3DIndexBuffer9_Lock},
    {"d3d9com::IDirect3DIndexBuffer9::Unlock", 1, custom_IDirect3DIndexBuffer9_Unlock},
    {"d3d9com::IDirect3DIndexBuffer9::GetDesc", 2, m_IDirect3DIndexBuffer9_GetDesc},
};

static const rt_shim_entry d3d9_IDirect3DBaseTexture9[] = {
    {"d3d9com::IDirect3DBaseTexture9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DBaseTexture9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DBaseTexture9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DBaseTexture9::GetDevice", 2, m_IDirect3DBaseTexture9_GetDevice},
    {"d3d9com::IDirect3DBaseTexture9::SetPrivateData", 5, m_IDirect3DBaseTexture9_SetPrivateData},
    {"d3d9com::IDirect3DBaseTexture9::GetPrivateData", 4, m_IDirect3DBaseTexture9_GetPrivateData},
    {"d3d9com::IDirect3DBaseTexture9::FreePrivateData", 2, m_IDirect3DBaseTexture9_FreePrivateData},
    {"d3d9com::IDirect3DBaseTexture9::SetPriority", 2, m_IDirect3DBaseTexture9_SetPriority},
    {"d3d9com::IDirect3DBaseTexture9::GetPriority", 1, m_IDirect3DBaseTexture9_GetPriority},
    {"d3d9com::IDirect3DBaseTexture9::PreLoad", 1, m_IDirect3DBaseTexture9_PreLoad},
    {"d3d9com::IDirect3DBaseTexture9::GetType", 1, m_IDirect3DBaseTexture9_GetType},
    {"d3d9com::IDirect3DBaseTexture9::SetLOD", 2, m_IDirect3DBaseTexture9_SetLOD},
    {"d3d9com::IDirect3DBaseTexture9::GetLOD", 1, m_IDirect3DBaseTexture9_GetLOD},
    {"d3d9com::IDirect3DBaseTexture9::GetLevelCount", 1, m_IDirect3DBaseTexture9_GetLevelCount},
    {"d3d9com::IDirect3DBaseTexture9::SetAutoGenFilterType", 2, m_IDirect3DBaseTexture9_SetAutoGenFilterType},
    {"d3d9com::IDirect3DBaseTexture9::GetAutoGenFilterType", 1, m_IDirect3DBaseTexture9_GetAutoGenFilterType},
    {"d3d9com::IDirect3DBaseTexture9::GenerateMipSubLevels", 1, m_IDirect3DBaseTexture9_GenerateMipSubLevels},
};

static const rt_shim_entry d3d9_IDirect3DTexture9[] = {
    {"d3d9com::IDirect3DTexture9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DTexture9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DTexture9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DTexture9::GetDevice", 2, m_IDirect3DTexture9_GetDevice},
    {"d3d9com::IDirect3DTexture9::SetPrivateData", 5, m_IDirect3DTexture9_SetPrivateData},
    {"d3d9com::IDirect3DTexture9::GetPrivateData", 4, m_IDirect3DTexture9_GetPrivateData},
    {"d3d9com::IDirect3DTexture9::FreePrivateData", 2, m_IDirect3DTexture9_FreePrivateData},
    {"d3d9com::IDirect3DTexture9::SetPriority", 2, m_IDirect3DTexture9_SetPriority},
    {"d3d9com::IDirect3DTexture9::GetPriority", 1, m_IDirect3DTexture9_GetPriority},
    {"d3d9com::IDirect3DTexture9::PreLoad", 1, m_IDirect3DTexture9_PreLoad},
    {"d3d9com::IDirect3DTexture9::GetType", 1, m_IDirect3DTexture9_GetType},
    {"d3d9com::IDirect3DTexture9::SetLOD", 2, m_IDirect3DTexture9_SetLOD},
    {"d3d9com::IDirect3DTexture9::GetLOD", 1, m_IDirect3DTexture9_GetLOD},
    {"d3d9com::IDirect3DTexture9::GetLevelCount", 1, m_IDirect3DTexture9_GetLevelCount},
    {"d3d9com::IDirect3DTexture9::SetAutoGenFilterType", 2, m_IDirect3DTexture9_SetAutoGenFilterType},
    {"d3d9com::IDirect3DTexture9::GetAutoGenFilterType", 1, m_IDirect3DTexture9_GetAutoGenFilterType},
    {"d3d9com::IDirect3DTexture9::GenerateMipSubLevels", 1, m_IDirect3DTexture9_GenerateMipSubLevels},
    {"d3d9com::IDirect3DTexture9::GetLevelDesc", 3, m_IDirect3DTexture9_GetLevelDesc},
    {"d3d9com::IDirect3DTexture9::GetSurfaceLevel", 3, m_IDirect3DTexture9_GetSurfaceLevel},
    {"d3d9com::IDirect3DTexture9::LockRect", 5, custom_IDirect3DTexture9_LockRect},
    {"d3d9com::IDirect3DTexture9::UnlockRect", 2, custom_IDirect3DTexture9_UnlockRect},
    {"d3d9com::IDirect3DTexture9::AddDirtyRect", 2, m_IDirect3DTexture9_AddDirtyRect},
};

static const rt_shim_entry d3d9_IDirect3DCubeTexture9[] = {
    {"d3d9com::IDirect3DCubeTexture9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DCubeTexture9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DCubeTexture9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DCubeTexture9::GetDevice", 2, m_IDirect3DCubeTexture9_GetDevice},
    {"d3d9com::IDirect3DCubeTexture9::SetPrivateData", 5, m_IDirect3DCubeTexture9_SetPrivateData},
    {"d3d9com::IDirect3DCubeTexture9::GetPrivateData", 4, m_IDirect3DCubeTexture9_GetPrivateData},
    {"d3d9com::IDirect3DCubeTexture9::FreePrivateData", 2, m_IDirect3DCubeTexture9_FreePrivateData},
    {"d3d9com::IDirect3DCubeTexture9::SetPriority", 2, m_IDirect3DCubeTexture9_SetPriority},
    {"d3d9com::IDirect3DCubeTexture9::GetPriority", 1, m_IDirect3DCubeTexture9_GetPriority},
    {"d3d9com::IDirect3DCubeTexture9::PreLoad", 1, m_IDirect3DCubeTexture9_PreLoad},
    {"d3d9com::IDirect3DCubeTexture9::GetType", 1, m_IDirect3DCubeTexture9_GetType},
    {"d3d9com::IDirect3DCubeTexture9::SetLOD", 2, m_IDirect3DCubeTexture9_SetLOD},
    {"d3d9com::IDirect3DCubeTexture9::GetLOD", 1, m_IDirect3DCubeTexture9_GetLOD},
    {"d3d9com::IDirect3DCubeTexture9::GetLevelCount", 1, m_IDirect3DCubeTexture9_GetLevelCount},
    {"d3d9com::IDirect3DCubeTexture9::SetAutoGenFilterType", 2, m_IDirect3DCubeTexture9_SetAutoGenFilterType},
    {"d3d9com::IDirect3DCubeTexture9::GetAutoGenFilterType", 1, m_IDirect3DCubeTexture9_GetAutoGenFilterType},
    {"d3d9com::IDirect3DCubeTexture9::GenerateMipSubLevels", 1, m_IDirect3DCubeTexture9_GenerateMipSubLevels},
    {"d3d9com::IDirect3DCubeTexture9::GetLevelDesc", 3, m_IDirect3DCubeTexture9_GetLevelDesc},
    {"d3d9com::IDirect3DCubeTexture9::GetCubeMapSurface", 4, m_IDirect3DCubeTexture9_GetCubeMapSurface},
    {"d3d9com::IDirect3DCubeTexture9::LockRect", 6, custom_IDirect3DCubeTexture9_LockRect},
    {"d3d9com::IDirect3DCubeTexture9::UnlockRect", 3, custom_IDirect3DCubeTexture9_UnlockRect},
    {"d3d9com::IDirect3DCubeTexture9::AddDirtyRect", 3, m_IDirect3DCubeTexture9_AddDirtyRect},
};

static const rt_shim_entry d3d9_IDirect3DVolumeTexture9[] = {
    {"d3d9com::IDirect3DVolumeTexture9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DVolumeTexture9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DVolumeTexture9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DVolumeTexture9::GetDevice", 2, m_IDirect3DVolumeTexture9_GetDevice},
    {"d3d9com::IDirect3DVolumeTexture9::SetPrivateData", 5, m_IDirect3DVolumeTexture9_SetPrivateData},
    {"d3d9com::IDirect3DVolumeTexture9::GetPrivateData", 4, m_IDirect3DVolumeTexture9_GetPrivateData},
    {"d3d9com::IDirect3DVolumeTexture9::FreePrivateData", 2, m_IDirect3DVolumeTexture9_FreePrivateData},
    {"d3d9com::IDirect3DVolumeTexture9::SetPriority", 2, m_IDirect3DVolumeTexture9_SetPriority},
    {"d3d9com::IDirect3DVolumeTexture9::GetPriority", 1, m_IDirect3DVolumeTexture9_GetPriority},
    {"d3d9com::IDirect3DVolumeTexture9::PreLoad", 1, m_IDirect3DVolumeTexture9_PreLoad},
    {"d3d9com::IDirect3DVolumeTexture9::GetType", 1, m_IDirect3DVolumeTexture9_GetType},
    {"d3d9com::IDirect3DVolumeTexture9::SetLOD", 2, m_IDirect3DVolumeTexture9_SetLOD},
    {"d3d9com::IDirect3DVolumeTexture9::GetLOD", 1, m_IDirect3DVolumeTexture9_GetLOD},
    {"d3d9com::IDirect3DVolumeTexture9::GetLevelCount", 1, m_IDirect3DVolumeTexture9_GetLevelCount},
    {"d3d9com::IDirect3DVolumeTexture9::SetAutoGenFilterType", 2, m_IDirect3DVolumeTexture9_SetAutoGenFilterType},
    {"d3d9com::IDirect3DVolumeTexture9::GetAutoGenFilterType", 1, m_IDirect3DVolumeTexture9_GetAutoGenFilterType},
    {"d3d9com::IDirect3DVolumeTexture9::GenerateMipSubLevels", 1, m_IDirect3DVolumeTexture9_GenerateMipSubLevels},
    {"d3d9com::IDirect3DVolumeTexture9::GetLevelDesc", 3, m_IDirect3DVolumeTexture9_GetLevelDesc},
    {"d3d9com::IDirect3DVolumeTexture9::GetVolumeLevel", 3, m_IDirect3DVolumeTexture9_GetVolumeLevel},
    {"d3d9com::IDirect3DVolumeTexture9::LockBox", 5, custom_IDirect3DVolumeTexture9_LockBox},
    {"d3d9com::IDirect3DVolumeTexture9::UnlockBox", 2, custom_IDirect3DVolumeTexture9_UnlockBox},
    {"d3d9com::IDirect3DVolumeTexture9::AddDirtyBox", 2, m_IDirect3DVolumeTexture9_AddDirtyBox},
};

static const rt_shim_entry d3d9_IDirect3DVertexDeclaration9[] = {
    {"d3d9com::IDirect3DVertexDeclaration9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DVertexDeclaration9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DVertexDeclaration9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DVertexDeclaration9::GetDevice", 2, m_IDirect3DVertexDeclaration9_GetDevice},
    {"d3d9com::IDirect3DVertexDeclaration9::GetDeclaration", 3, m_IDirect3DVertexDeclaration9_GetDeclaration},
};

static const rt_shim_entry d3d9_IDirect3DVertexShader9[] = {
    {"d3d9com::IDirect3DVertexShader9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DVertexShader9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DVertexShader9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DVertexShader9::GetDevice", 2, m_IDirect3DVertexShader9_GetDevice},
    {"d3d9com::IDirect3DVertexShader9::GetFunction", 3, m_IDirect3DVertexShader9_GetFunction},
};

static const rt_shim_entry d3d9_IDirect3DPixelShader9[] = {
    {"d3d9com::IDirect3DPixelShader9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DPixelShader9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DPixelShader9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DPixelShader9::GetDevice", 2, m_IDirect3DPixelShader9_GetDevice},
    {"d3d9com::IDirect3DPixelShader9::GetFunction", 3, m_IDirect3DPixelShader9_GetFunction},
};

static const rt_shim_entry d3d9_IDirect3DStateBlock9[] = {
    {"d3d9com::IDirect3DStateBlock9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DStateBlock9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DStateBlock9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DStateBlock9::GetDevice", 2, m_IDirect3DStateBlock9_GetDevice},
    {"d3d9com::IDirect3DStateBlock9::Capture", 1, m_IDirect3DStateBlock9_Capture},
    {"d3d9com::IDirect3DStateBlock9::Apply", 1, m_IDirect3DStateBlock9_Apply},
};

static const rt_shim_entry d3d9_IDirect3DQuery9[] = {
    {"d3d9com::IDirect3DQuery9::QueryInterface", 3, custom_IUnknown_QueryInterface},
    {"d3d9com::IDirect3DQuery9::AddRef", 1, m_IUnknown_AddRef},
    {"d3d9com::IDirect3DQuery9::Release", 1, custom_IUnknown_Release},
    {"d3d9com::IDirect3DQuery9::GetDevice", 2, m_IDirect3DQuery9_GetDevice},
    {"d3d9com::IDirect3DQuery9::GetType", 1, m_IDirect3DQuery9_GetType},
    {"d3d9com::IDirect3DQuery9::GetDataSize", 1, m_IDirect3DQuery9_GetDataSize},
    {"d3d9com::IDirect3DQuery9::Issue", 2, m_IDirect3DQuery9_Issue},
    {"d3d9com::IDirect3DQuery9::GetData", 4, m_IDirect3DQuery9_GetData},
};

const rt_com_iface rt_com_ifaces[] = {
    {"IDirect3D9", d3d9_IDirect3D9, 17},
    {"IDirect3DDevice9", d3d9_IDirect3DDevice9, 119},
    {"IDirect3DResource9", d3d9_IDirect3DResource9, 11},
    {"IDirect3DSwapChain9", d3d9_IDirect3DSwapChain9, 10},
    {"IDirect3DSurface9", d3d9_IDirect3DSurface9, 17},
    {"IDirect3DVolume9", d3d9_IDirect3DVolume9, 11},
    {"IDirect3DVertexBuffer9", d3d9_IDirect3DVertexBuffer9, 14},
    {"IDirect3DIndexBuffer9", d3d9_IDirect3DIndexBuffer9, 14},
    {"IDirect3DBaseTexture9", d3d9_IDirect3DBaseTexture9, 17},
    {"IDirect3DTexture9", d3d9_IDirect3DTexture9, 22},
    {"IDirect3DCubeTexture9", d3d9_IDirect3DCubeTexture9, 22},
    {"IDirect3DVolumeTexture9", d3d9_IDirect3DVolumeTexture9, 22},
    {"IDirect3DVertexDeclaration9", d3d9_IDirect3DVertexDeclaration9, 5},
    {"IDirect3DVertexShader9", d3d9_IDirect3DVertexShader9, 5},
    {"IDirect3DPixelShader9", d3d9_IDirect3DPixelShader9, 5},
    {"IDirect3DStateBlock9", d3d9_IDirect3DStateBlock9, 6},
    {"IDirect3DQuery9", d3d9_IDirect3DQuery9, 8},
};
const unsigned rt_com_iface_count = 17;

}  // extern "C"
