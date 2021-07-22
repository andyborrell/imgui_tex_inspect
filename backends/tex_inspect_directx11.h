// ImGuiTexInspect, a texture inspector widget for dear imgui

#pragma once
struct ID3D11Device;
struct ID3D11DeviceContext;
namespace ImGuiTexInspect
{
bool ImplDX11_Init(ID3D11Device *device, ID3D11DeviceContext *device_context); // Will add references
void ImplDX11_Shutdown();                                                      // Will release references
} // namespace ImGuiTexInspect
