#include "dx11.h"
#include <d3d11.h>
#include <algorithm>

#include <MinHook/MinHook.h>

#include "../scs_logging.h"
using namespace scs_logging;

#include "../screens.h"

typedef HRESULT(__stdcall* CreateTexture2D_t)(ID3D11Device*, const D3D11_TEXTURE2D_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Texture2D**);
static CreateTexture2D_t CreateTexture2D_Original = nullptr;

HRESULT HookedCreateTexture2D(ID3D11Device* pDevice, const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D)
{
    if (!g_screen_source_creation_in_progress.load()) {
        std::lock_guard<std::mutex> lock(g_screens_mutex);

        for (screen_t& screen : g_screens)
        {
            if (!screen.source.get()) continue;

            if (!pDesc) continue;
            if (pDesc->Width != screen.override_texture_size_w) continue;
            if (pDesc->Height != screen.override_texture_size_h) continue;
            if (pDesc->Format != DXGI_FORMAT_BC3_UNORM) continue;
            if (pDesc->Usage != D3D11_USAGE_DEFAULT) continue;
            if ((pDesc->BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0) continue;
            if (pInitialData) continue;
            if (pDesc->MipLevels != 1) continue;

            D3D11_TEXTURE2D_DESC modifiedDesc = *pDesc;
            modifiedDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            modifiedDesc.Usage = D3D11_USAGE_DYNAMIC;
            modifiedDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            modifiedDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            modifiedDesc.MiscFlags = 0;
            modifiedDesc.Width = (std::max)(64u, (std::min)(4096u, screen.targetLiveTextureWidth));
            modifiedDesc.Height = (std::max)(64u, (std::min)(4096u, screen.targetLiveTextureHeight));
            modifiedDesc.MipLevels = 1;

            // Always pass nullptr for pInitialData on resized dynamic textures to prevent buffer over-read crashes
            HRESULT hr = CreateTexture2D_Original(pDevice, &modifiedDesc, nullptr, ppTexture2D);
            if (SUCCEEDED(hr) && ppTexture2D && *ppTexture2D)
            {
                if (screen.liveTexture) { screen.liveTexture->Release(); screen.liveTexture = nullptr; }
                if (screen.immediateContext) { screen.immediateContext->Release(); screen.immediateContext = nullptr; }

                screen.liveTextureWidth = modifiedDesc.Width;
                screen.liveTextureHeight = modifiedDesc.Height;
                screen.firstFrameLogged = false;

                screen.liveTexture = *ppTexture2D;
                screen.liveTexture->AddRef();
                pDevice->GetImmediateContext(&screen.immediateContext);

                scs_log(0, "[dx11::create_texture_2d] matched texture '%s' (%ux%u)", screen.original_texture.c_str(), modifiedDesc.Width, modifiedDesc.Height);
            }
            else {
                scs_log(2, "[dx11::create_texture_2d] rewrite of %s FAILED, hr=0x%08X", screen.original_texture.c_str(), hr);
            }
            return hr;
        }
    }

    return CreateTexture2D_Original(pDevice, pDesc, pInitialData, ppTexture2D);
}

void new_frame()
{
    update_screen_patches();

    std::lock_guard<std::mutex> lock(g_screens_mutex);
    for (auto& screen : g_screens)
    {
        if (!screen.source.get())
            continue;

        if (!screen.liveTexture || !screen.immediateContext)
            continue;

        if (!screen.source->CopyLatestFrame(screen.frameScratch))
            continue;

        const UINT srcWidth = screen.source->GetWidth();
        const UINT srcHeight = screen.source->GetHeight();
        const UINT dstWidth = screen.liveTextureWidth;
        const UINT dstHeight = screen.liveTextureHeight;
        if (srcWidth == 0 || srcHeight == 0 || dstWidth == 0 || dstHeight == 0)
            continue;
        if (screen.frameScratch.size() < static_cast<size_t>(srcWidth) * srcHeight * 4)
            continue;

        float zoom = (screen.sourceZoom < 1.0f) ? 1.0f : screen.sourceZoom;
        UINT cropW = static_cast<UINT>(srcWidth / zoom);
        UINT cropH = static_cast<UINT>(srcHeight / zoom);
        if (cropW < 1) cropW = 1;
        if (cropH < 1) cropH = 1;

        int maxPanX = static_cast<int>(srcWidth - cropW);
        int maxPanY = static_cast<int>(srcHeight - cropH);
        int relCropX = static_cast<int>(maxPanX / 2 + screen.sourceOffsetX * (maxPanX / 2));
        int relCropY = static_cast<int>(maxPanY / 2 + screen.sourceOffsetY * (maxPanY / 2));

        if (relCropX < 0) relCropX = 0;
        if (relCropY < 0) relCropY = 0;
        if (relCropX + cropW > srcWidth) relCropX = srcWidth - cropW;
        if (relCropY + cropH > srcHeight) relCropY = srcHeight - cropH;

        const UINT cropX = static_cast<UINT>(relCropX);
        const UINT cropY = static_cast<UINT>(relCropY);
        const UINT effSrcW = cropW;
        const UINT effSrcH = cropH;

        D3D11_MAPPED_SUBRESOURCE mapped;
        if (FAILED(screen.immediateContext->Map(screen.liveTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)) || !mapped.pData)
            continue;

        const uint8_t* src = screen.frameScratch.data();
        uint8_t* dstBase = static_cast<uint8_t*>(mapped.pData);

        if (screen.splitAlignment == split_alignment_t::FIT || screen.splitAlignment == split_alignment_t::FILL)
        {
            float srcAspect = static_cast<float>(effSrcW) / static_cast<float>(effSrcH);
            float dstAspect = static_cast<float>(dstWidth) / static_cast<float>(dstHeight);

            UINT renderW, renderH, offsetX, offsetY;

            if (screen.splitAlignment == split_alignment_t::FIT) {
                if (srcAspect > dstAspect) {
                    renderW = dstWidth;
                    renderH = (std::max)(1u, static_cast<UINT>(dstWidth / srcAspect));
                } else {
                    renderH = dstHeight;
                    renderW = (std::max)(1u, static_cast<UINT>(dstHeight * srcAspect));
                }
            } else {
                if (srcAspect > dstAspect) {
                    renderH = dstHeight;
                    renderW = (std::max)(1u, static_cast<UINT>(dstHeight * srcAspect));
                } else {
                    renderW = dstWidth;
                    renderH = (std::max)(1u, static_cast<UINT>(dstWidth / srcAspect));
                }
            }

            offsetX = (renderW > dstWidth) ? 0 : (dstWidth - renderW) / 2;
            offsetY = (renderH > dstHeight) ? 0 : (dstHeight - renderH) / 2;

            int fillCropX = 0, fillCropY = 0;
            UINT sampleW = effSrcW, sampleH = effSrcH;
            if (screen.splitAlignment == split_alignment_t::FILL && (renderW > dstWidth || renderH > dstHeight)) {
                if (renderW > dstWidth) {
                    float cropRatio = static_cast<float>(dstWidth) / static_cast<float>(renderW);
                    sampleW = (std::max)(1u, static_cast<UINT>(effSrcW * cropRatio));
                    fillCropX = (effSrcW - sampleW) / 2;
                }
                if (renderH > dstHeight) {
                    float cropRatio = static_cast<float>(dstHeight) / static_cast<float>(renderH);
                    sampleH = (std::max)(1u, static_cast<UINT>(effSrcH * cropRatio));
                    fillCropY = (effSrcH - sampleH) / 2;
                }
                renderW = (renderW > dstWidth) ? dstWidth : renderW;
                renderH = (renderH > dstHeight) ? dstHeight : renderH;
                offsetX = (dstWidth - renderW) / 2;
                offsetY = (dstHeight - renderH) / 2;
            }

            for (UINT y = 0; y < dstHeight; ++y)
            {
                const UINT dstRow = screen.flipVertical ? (dstHeight - 1 - y) : y;
                uint8_t* dstRowPtr = dstBase + static_cast<size_t>(dstRow) * mapped.RowPitch;

                if (y < offsetY || y >= offsetY + renderH) {
                    for (UINT b = 0; b < dstWidth; ++b) {
                        dstRowPtr[b * 4 + 0] = 0;
                        dstRowPtr[b * 4 + 1] = 0;
                        dstRowPtr[b * 4 + 2] = 0;
                        dstRowPtr[b * 4 + 3] = 255;
                    }
                    continue;
                }

                if (offsetX > 0) {
                    for (UINT b = 0; b < offsetX; ++b) {
                        dstRowPtr[b * 4 + 0] = 0;
                        dstRowPtr[b * 4 + 1] = 0;
                        dstRowPtr[b * 4 + 2] = 0;
                        dstRowPtr[b * 4 + 3] = 255;
                    }
                }
                UINT rightStart = offsetX + renderW;
                if (rightStart < dstWidth) {
                    for (UINT b = rightStart; b < dstWidth; ++b) {
                        dstRowPtr[b * 4 + 0] = 0;
                        dstRowPtr[b * 4 + 1] = 0;
                        dstRowPtr[b * 4 + 2] = 0;
                        dstRowPtr[b * 4 + 3] = 255;
                    }
                }

                const UINT localY = y - offsetY;
                const UINT effY = (std::min)(srcHeight - 1, cropY + fillCropY + static_cast<UINT>(static_cast<uint64_t>(localY) * sampleH / renderH));
                const uint8_t* srcRow = src + static_cast<size_t>(effY) * srcWidth * 4;

                for (UINT x = 0; x < renderW; ++x) {
                    const UINT effX = (std::min)(srcWidth - 1, cropX + fillCropX + static_cast<UINT>(static_cast<uint64_t>(x) * sampleW / renderW));
                    const uint8_t* p = srcRow + static_cast<size_t>(effX) * 4;
                    uint8_t* d = dstRowPtr + static_cast<size_t>(offsetX + x) * 4;
                    d[0] = p[0];
                    d[1] = p[1];
                    d[2] = p[2];
                    d[3] = 255;
                }
            }
        }
        else
        {
            for (UINT y = 0; y < dstHeight; ++y)
            {
                const UINT effY = (std::min)(srcHeight - 1, cropY + static_cast<UINT>(static_cast<uint64_t>(y) * effSrcH / dstHeight));
                const UINT dstRow = screen.flipVertical ? (dstHeight - 1 - y) : y;
                const uint8_t* srcRow = src + static_cast<size_t>(effY) * srcWidth * 4;
                uint8_t* dstRowPtr = dstBase + static_cast<size_t>(dstRow) * mapped.RowPitch;

                UINT startX = 0;
                UINT endX = dstWidth;
                if (screen.splitAlignment == split_alignment_t::LEFT_HALF) {
                    endX = dstWidth / 2;
                    for (UINT b = endX; b < dstWidth; ++b) {
                        dstRowPtr[b * 4 + 0] = 0;
                        dstRowPtr[b * 4 + 1] = 0;
                        dstRowPtr[b * 4 + 2] = 0;
                        dstRowPtr[b * 4 + 3] = 255;
                    }
                } else if (screen.splitAlignment == split_alignment_t::RIGHT_HALF) {
                    startX = dstWidth / 2;
                    for (UINT b = 0; b < startX; ++b) {
                        dstRowPtr[b * 4 + 0] = 0;
                        dstRowPtr[b * 4 + 1] = 0;
                        dstRowPtr[b * 4 + 2] = 0;
                        dstRowPtr[b * 4 + 3] = 255;
                    }
                }

                UINT renderWidth = (std::max)(1u, endX - startX);
                for (UINT x = startX; x < endX; ++x) {
                    const UINT effX = (std::min)(srcWidth - 1, cropX + static_cast<UINT>(static_cast<uint64_t>(x - startX) * effSrcW / renderWidth));
                    const uint8_t* p = srcRow + static_cast<size_t>(effX) * 4;
                    uint8_t* d = dstRowPtr + static_cast<size_t>(x) * 4;
                    d[0] = p[0];
                    d[1] = p[1];
                    d[2] = p[2];
                    d[3] = 255;
                }
            }
        }

        screen.immediateContext->Unmap(screen.liveTexture, 0);

        if (!screen.firstFrameLogged) {
            screen.firstFrameLogged = true;
            scs_log(0, "[dx11::create_texture_2d] First frame uploaded for '%s' (src %ux%u -> dst %ux%u)",
                screen.original_texture.c_str(), srcWidth, srcHeight, dstWidth, dstHeight);
        }
    }
}

namespace dx11::create_texture_2d {
	bool init()
	{
        dx11::present::on_frame(new_frame);

        ID3D11Device* pDummyDevice = nullptr;
        ID3D11DeviceContext* pDummyContext = nullptr;

        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION,
            &pDummyDevice, nullptr, &pDummyContext);

        if (FAILED(hr)) {
            hr = D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                nullptr, 0, D3D11_SDK_VERSION,
                &pDummyDevice, nullptr, &pDummyContext);
        }

        if (FAILED(hr) || !pDummyDevice)
        {
            scs_log(2, "[dx11::create_texture_2d] D3D11CreateDevice failed (hr=0x%08X)", hr);
            return false;
        }

        void** deviceVtbl = *reinterpret_cast<void***>(pDummyDevice);
        void* createTexture2DAddr = deviceVtbl[5];

        MH_CreateHook(createTexture2DAddr, &HookedCreateTexture2D, reinterpret_cast<LPVOID*>(&CreateTexture2D_Original));
        MH_EnableHook(createTexture2DAddr);

        pDummyContext->Release();
        pDummyDevice->Release();

        return true;
	}
}
