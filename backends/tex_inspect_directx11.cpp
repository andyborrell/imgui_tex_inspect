// ImGuiTexInspect, a texture inspector widget for dear imgui

#include "imgui.h"
#include "imgui_tex_inspect_internal.h"
#include <d3d11.h>
#include <d3dcompiler.h>

namespace ImGuiTexInspect
{

/* Constant buffer used in pixel shader. Size must be multiple of 16 bytes.
 * Layout must match the layout in the pixel shader. */
struct PIXEL_CONSTANT_BUFFER
{
    float ColorTransform[16];
    float ColorOffset[4];
    float Grid[4];
    float GridWidth[2];
    float PremultiplyAlpha;
    float DisableFinalAlpha;
    float BackgroundColor[3];
    bool  forceNearestSampling;
    float TextureSize[2];
    float padding[2];
};

struct ImGuiTexInspect_ImplDX11_Data
{
    ID3D11Device        *pd3dDevice           = NULL;
    ID3D11DeviceContext *pd3dDeviceContext    = NULL;
    ID3D11PixelShader   *pPixelShader         = NULL;
    ID3D11Buffer        *pPixelConstantBuffer = NULL;
};

static ImGuiTexInspect_ImplDX11_Data GImplData;


bool ImplDX11_Init(ID3D11Device *device, ID3D11DeviceContext *device_context)
{
    // Check we're not initializing more than once
    assert(GImplData.pd3dDevice == NULL);

    // Save pointers to DirectX device and context
    GImplData.pd3dDevice = device;
    GImplData.pd3dDevice->AddRef();

    GImplData.pd3dDeviceContext = device_context;
    GImplData.pd3dDeviceContext->AddRef();

    // Create our pixel shader
    {
        static const char *pixelShader = 
            "cbuffer pixelBuffer :register(b0)\n\
            {\n\
                float4x4 ColorTransform;\n\
                float4   ColorOffset;\n\
                float4   Grid;\n\
                float2   GridWidth;\n\
                float    PremultiplyAlpha;\n\
                float    DisableFinalAlpha;\n\
                float3   BackgroundColor;\n\
                bool     ForceNearestSampling;\n\
                float2   TextureSize;\n\
            };\n\
            struct PS_INPUT\n\
            {\n\
                float4 pos : SV_POSITION;\n\
                float4 col : COLOR0;\n\
                float2 uv  : TEXCOORD0;\n\
            };\n\
            sampler sampler0;\n\
            Texture2D texture0;\n\
            \n\
            float4 main(PS_INPUT input) : SV_Target\n\
            {\n\
                float2 uv;\n\
                float2 texel = input.uv * TextureSize;\n\
                if (ForceNearestSampling)\n\
                    uv = (floor(texel) + float2(0.5, 0.5)) / TextureSize;\n\
                else\n\
                    uv = input.uv;\n\
                float2 texelEdge = step(texel - floor(texel), GridWidth);\n\
                float isGrid = max(texelEdge.x, texelEdge.y);\n\
                float4 ct = mul(ColorTransform, texture0.Sample(sampler0, uv)) + ColorOffset;\n\
                ct.rgb = ct.rgb * lerp(1.0, ct.a, PremultiplyAlpha);\n\
                ct.rgb += BackgroundColor * (1.0 - ct.a);\n\
                ct.a = lerp(ct.a, 1.0, DisableFinalAlpha);\n\
                ct = lerp(ct, float4(Grid.rgb, 1), Grid.a * isGrid);\n\
                return ct;\n\
            }";

        ID3DBlob *pixelShaderBlob;
        ID3DBlob *errorBlob;
        if (FAILED(D3DCompile(pixelShader, strlen(pixelShader), NULL, NULL, NULL, "main", "ps_4_0", 0, 0, &pixelShaderBlob, &errorBlob)))
        {
            fprintf(stderr, "ImGuiTexInspect pixel shader failed. Diagnostic:\n%s\n", (const char *)errorBlob->GetBufferPointer());
            errorBlob->Release();
            return false;
        }

        if (GImplData.pd3dDevice->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), NULL,
                                                    &GImplData.pPixelShader) != S_OK)
        {
            pixelShaderBlob->Release();
            return false;
        }
        pixelShaderBlob->Release();
    }

    // Create pixel shader constant buffer
    {
        D3D11_BUFFER_DESC desc;
        desc.ByteWidth      = sizeof(PIXEL_CONSTANT_BUFFER);
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags      = 0;
        GImplData.pd3dDevice->CreateBuffer(&desc, NULL, &GImplData.pPixelConstantBuffer);
    }

    return true;
}

void ImplDX11_Shutdown()
{
    if (GImplData.pd3dDevice)
        GImplData.pd3dDevice->Release();

    if (GImplData.pd3dDeviceContext)
        GImplData.pd3dDeviceContext->Release();

    if (GImplData.pPixelConstantBuffer)
        GImplData.pPixelConstantBuffer->Release();

    if (GImplData.pPixelShader)
        GImplData.pPixelShader->Release();

    GImplData.pd3dDevice = NULL;
}

void GiveNotInitializedWarning()
{
    static bool warningGiven = false;
    if (!warningGiven)
    {
        fprintf(stderr, "ERROR: ImGuiTexInspect backend not initialized\n");
        warningGiven = true;
    }
}

struct DX11FormatDesc
{
    ImGuiDataType_ type;
    int channelCount;

    int GetComponentSize()
    {
        switch (type)
        {
            case ImGuiDataType_S8:     return 1;
            case ImGuiDataType_U8:     return 1;
            case ImGuiDataType_S16:    return 2;
            case ImGuiDataType_U16:    return 2;
            case ImGuiDataType_S32:    return 4;
            case ImGuiDataType_U32:    return 4;
            case ImGuiDataType_S64:    return 8;
            case ImGuiDataType_U64:    return 8;
            case ImGuiDataType_Float:  return 4;
            case ImGuiDataType_Double: return 8;
            default:                   
                // Shouldn't get here
                assert(false);    
                return 0;
        }
    }
};

bool DecodeDXGIFormat(DXGI_FORMAT format, DX11FormatDesc *desc)
{
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
        desc->type = ImGuiDataType_U8;
        desc->channelCount = 4;
        return true;

    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
        desc->type = ImGuiDataType_U8;
        desc->channelCount = 2;
        return true;

    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
        desc->type = ImGuiDataType_U8;
        desc->channelCount = 1;
        return true;

    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        desc->type = ImGuiDataType_Float;
        desc->channelCount = 4;
        return true;

    case DXGI_FORMAT_R32G32B32_FLOAT:
        desc->type = ImGuiDataType_Float;
        desc->channelCount = 3;
        return true;

    case DXGI_FORMAT_R32G32_FLOAT:
        desc->type = ImGuiDataType_Float;
        desc->channelCount = 2;
        return true;

    case DXGI_FORMAT_R32_FLOAT:
        desc->type = ImGuiDataType_Float;
        desc->channelCount = 1;
        return true;

    default:
        return false;
    }
}

//-------------------------------------------------------------------------
// [SECTION] BackEnd functions declared in imgui_tex_inspect_internal.h
//-------------------------------------------------------------------------

void BackEnd_SetShader(const ImDrawList *, const ImDrawCmd *, const Inspector *inspector)
{
    if (GImplData.pPixelShader == NULL)
    {
        GiveNotInitializedWarning();
        return;
    }

    const ShaderOptions *shaderOptions = &inspector->CachedShaderOptions;

    // Map the pixel shader constant buffer and fill values
    {
        D3D11_MAPPED_SUBRESOURCE mapped_resource;
        if (GImplData.pd3dDeviceContext->Map(GImplData.pPixelConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource) != S_OK)
            return;


        // Transfer shader options from shaderOptions to our backend specific pixel shader constant buffer
        PIXEL_CONSTANT_BUFFER *constant_buffer = (PIXEL_CONSTANT_BUFFER *)mapped_resource.pData;

        memcpy(constant_buffer->ColorTransform,  shaderOptions->ColorTransform,   sizeof(shaderOptions->ColorTransform));
        memcpy(constant_buffer->ColorOffset,     shaderOptions->ColorOffset,      sizeof(shaderOptions->ColorOffset));
        memcpy(constant_buffer->Grid,            &shaderOptions->GridColor,       sizeof(shaderOptions->GridColor));
        memcpy(constant_buffer->GridWidth,       &shaderOptions->GridWidth,       sizeof(shaderOptions->GridWidth));
        memcpy(constant_buffer->BackgroundColor, &shaderOptions->BackgroundColor, sizeof(shaderOptions->BackgroundColor));
        memcpy(constant_buffer->TextureSize,     &inspector->TextureSize,         sizeof(inspector->TextureSize));

        constant_buffer->PremultiplyAlpha     = shaderOptions->PremultiplyAlpha;
        constant_buffer->DisableFinalAlpha    = shaderOptions->DisableFinalAlpha;
        constant_buffer->forceNearestSampling = shaderOptions->ForceNearestSampling;

        GImplData.pd3dDeviceContext->Unmap(GImplData.pPixelConstantBuffer, 0);
    }

    // Activate shader and buffer
    GImplData.pd3dDeviceContext->PSSetShader(GImplData.pPixelShader, NULL, 0);
    GImplData.pd3dDeviceContext->PSSetConstantBuffers(0, 1, &GImplData.pPixelConstantBuffer);
}

bool BackEnd_GetData(Inspector *inspector, ImTextureID texture, int /*x*/, int /*y*/, int /*width*/, int /*height*/, BufferDesc *bufferDesc)
{
    if (GImplData.pd3dDevice == NULL)
    {
        GiveNotInitializedWarning();
        return false;
    }
    ID3D11Texture2D *pTexture = NULL;

    // Get a pointer to the texture
    {
        ID3D11ShaderResourceView *pTextureView = (ID3D11ShaderResourceView *)texture;
        ID3D11Resource *pResource;
        pTextureView->GetResource(&pResource);

        if (pResource == NULL)
            return false;

        pResource->QueryInterface<ID3D11Texture2D>(&pTexture);

        if (pTexture == NULL)
        {
            pResource->Release();
            return false;
        }
        pResource->Release();
    }

    int texWidth  = (int)inspector->TextureSize.x;
    int texHeight = (int)inspector->TextureSize.y;

    ID3D11Texture2D *pStagingTexture = nullptr;
    DX11FormatDesc formatDesc;

    // Create a CPU accessible texture to copy src texture into
    {
        D3D11_TEXTURE2D_DESC texDesc;
        pTexture->GetDesc(&texDesc);

        if (!DecodeDXGIFormat(texDesc.Format, &formatDesc))
        {
            // Not a supoorted format
            pTexture->Release();
            return false;
        }

        // We'll keep all settings as is except for these:
        texDesc.Usage = D3D11_USAGE_STAGING;
        texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        texDesc.BindFlags = 0;
        texDesc.MiscFlags = 0;

        /* Yes we could be keeping this staging texture around rather than creating it
         * every time.  But in profiling it was apparent that the cost of creating the
         * texture is not significant compared to copying the data.  So it's not worth the
         * complexity of tracking and releasing a texture for each Inspector.
         */
        HRESULT hr = GImplData.pd3dDevice->CreateTexture2D(&texDesc, NULL, &pStagingTexture);

        if (FAILED(hr))
        {
            pTexture->Release();
            return false;
        }
    }

    // Oopy texture data to CPU accessible texture
    GImplData.pd3dDeviceContext->CopyResource(pStagingTexture, pTexture);

    D3D11_MAPPED_SUBRESOURCE resourceDesc = {};
    HRESULT hr = GImplData.pd3dDeviceContext->Map(pStagingTexture, 0, D3D11_MAP_READ, 0, &resourceDesc);
    bool success = false;
    if (SUCCEEDED(hr))
    {
        int componentSize = formatDesc.GetComponentSize();
        int channelCount = formatDesc.channelCount;

        void *copyDst = NULL;
        if (formatDesc.type == ImGuiDataType_Float)
        {
            bufferDesc->Data_float =
                (float *)ImGuiTexInspect::GetBuffer(inspector, (size_t)texWidth * texHeight * channelCount * componentSize);
            bufferDesc->Data_uint8_t = NULL;
            copyDst = bufferDesc->Data_float;
        }
        else // ImGuiDataType_U8 is only other supported type at the moment
        {
            bufferDesc->Data_uint8_t = ImGuiTexInspect::GetBuffer(inspector, (size_t)texWidth * texHeight * channelCount * componentSize);
            bufferDesc->Data_float = NULL;
            copyDst = bufferDesc->Data_uint8_t;
        }

        if (copyDst)
        {
            int const bytesPerPixel = componentSize * channelCount;
            for (int i = 0; i < texHeight; ++i)
            {
                /* TODO : An obvious optimization would be to return a pointer
                 * directly into the DirectX ResourceDesc data.  We would need
                 * another callback to know when to unmap it.
                 */
                memcpy((byte *)copyDst + texWidth * bytesPerPixel * i, (byte *)resourceDesc.pData + resourceDesc.RowPitch * i,
                       (size_t)texWidth * bytesPerPixel);
            }
            bufferDesc->BufferByteSize = (size_t)texWidth * texHeight * bytesPerPixel;
            bufferDesc->Red            = 0;
            bufferDesc->Green          = (ImU8)ImMin(1, channelCount - 1);
            bufferDesc->Blue           = (ImU8)ImMin(2, channelCount - 1);
            bufferDesc->Alpha          = (ImU8)ImMin(3, channelCount - 1);
            bufferDesc->ChannelCount   = (ImU8)channelCount;

            bufferDesc->LineStride = (int)inspector->TextureSize.x * channelCount;
            bufferDesc->Stride     = channelCount;
            bufferDesc->StartX     = 0;
            bufferDesc->StartY     = 0;
            bufferDesc->Width      = texWidth;
            bufferDesc->Height     = texHeight;

            success = true;
        }
        GImplData.pd3dDeviceContext->Unmap(pStagingTexture, 0);
    }

    pTexture->Release();

    pStagingTexture->Release();

    return success;
}
} // namespace ImGuiTexInspect
