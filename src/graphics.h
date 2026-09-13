#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <stdexcept>
inline void check(HRESULT hr,const char*where){if(FAILED(hr)){char s[256];sprintf_s(s,"%s (0x%08X)",where,unsigned(hr));throw std::runtime_error(s);}}
inline std::string resource(int id){auto r=FindResourceW(nullptr,MAKEINTRESOURCEW(id),RT_RCDATA);if(!r)throw std::runtime_error("Missing embedded resource");auto h=LoadResource(nullptr,r);return std::string(static_cast<const char*>(LockResource(h)),SizeofResource(nullptr,r));}
struct Scene { float view[4],tint[4],options[4],grid[4],rhythm[4],interior[4],waveSettings[4],rotation[4],atmosphere[4],waves[16],kicks[16],bands[64],framing[4]; };
class Graphics {
    ComPtr<ID3D11Device>device;ComPtr<ID3D11DeviceContext>context;ComPtr<IDXGISwapChain1>swap;
    ComPtr<IDCompositionDevice>composition;ComPtr<IDCompositionTarget>target;ComPtr<IDCompositionVisual>visual;
    ComPtr<ID3D11RenderTargetView>rtv;ComPtr<ID3D11VertexShader>vs;ComPtr<ID3D11PixelShader>ps;
    ComPtr<ID3D11Buffer>constants;ComPtr<ID3D11BlendState>blend;ComPtr<ID3D11RasterizerState>raster;
    struct Surface { ComPtr<ID3D11Texture2D>texture;ComPtr<ID3D11RenderTargetView>rt;ComPtr<ID3D11ShaderResourceView>srv; };
    Surface energySurface,brightSurface,blurSurface,bloomSurface,resolvedSurface;
    ComPtr<ID3D11VertexShader>fullVS;ComPtr<ID3D11PixelShader>extractPS,scatterHPS,scatterPS,compositePS,fxaaPS;
    ComPtr<ID3D11SamplerState>sampler;ComPtr<ID3D11Buffer>postBuffer;
    unsigned dimension=0;
    void surface(Surface&s,unsigned size,bool mipmaps=false,DXGI_FORMAT format=DXGI_FORMAT_R16G16B16A16_FLOAT){s={};D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=size;d.MipLevels=mipmaps?0:1;d.ArraySize=1;d.MiscFlags=mipmaps?D3D11_RESOURCE_MISC_GENERATE_MIPS:0;d.Format=format;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;check(device->CreateTexture2D(&d,nullptr,&s.texture),"HDR surface");check(device->CreateRenderTargetView(s.texture.Get(),nullptr,&s.rt),"HDR target");check(device->CreateShaderResourceView(s.texture.Get(),nullptr,&s.srv),"HDR sampling");}
    void fullPass(ID3D11RenderTargetView*destination,ID3D11PixelShader*shader,ID3D11ShaderResourceView*input,ID3D11ShaderResourceView*second,unsigned size){ID3D11ShaderResourceView*empty[2]={};context->PSSetShaderResources(0,2,empty);context->OMSetRenderTargets(1,&destination,nullptr);ID3D11ShaderResourceView*views[2]={input,second};context->PSSetShaderResources(0,2,views);D3D11_VIEWPORT vp{0,0,float(size),float(size),0,1};context->RSSetViewports(1,&vp);context->PSSetShader(shader,nullptr,0);context->Draw(3,0);context->PSSetShaderResources(0,2,empty);}

public:
    bool software=false;
    bool diagnosticNoDither=false;
    std::wstring snapshotPath;
    void snapshot(const std::wstring&path){
        ComPtr<ID3D11Texture2D>buffer;check(swap->GetBuffer(0,IID_PPV_ARGS(&buffer)),"Snapshot buffer");D3D11_TEXTURE2D_DESC d;buffer->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;ComPtr<ID3D11Texture2D>staging;check(device->CreateTexture2D(&d,nullptr,&staging),"Snapshot staging");context->CopyResource(staging.Get(),buffer.Get());D3D11_MAPPED_SUBRESOURCE m;check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m),"Snapshot readback");std::vector<BYTE>pixels(size_t(d.Width)*d.Height*4);for(unsigned y=0;y<d.Height;++y){auto src=static_cast<const BYTE*>(m.pData)+y*m.RowPitch;auto dst=pixels.data()+y*d.Width*4;for(unsigned x=0;x<d.Width;++x){unsigned a=src[x*4+3];for(int k=0;k<3;++k)dst[x*4+k]=a?BYTE(std::min(255u,unsigned(src[x*4+k])*255/a)):0;dst[x*4+3]=BYTE(a);}}context->Unmap(staging.Get(),0);
        ComPtr<IWICImagingFactory>factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"PNG factory");ComPtr<IWICStream>stream;check(factory->CreateStream(&stream),"PNG stream");check(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE),"PNG file");ComPtr<IWICBitmapEncoder>encoder;check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder),"PNG encoder");check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"PNG init");ComPtr<IWICBitmapFrameEncode>frame;check(encoder->CreateNewFrame(&frame,nullptr),"PNG frame");check(frame->Initialize(nullptr),"PNG frame init");check(frame->SetSize(d.Width,d.Height),"PNG size");WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;check(frame->SetPixelFormat(&format),"PNG format");check(frame->WritePixels(d.Height,d.Width*4,UINT(pixels.size()),pixels.data()),"PNG pixels");check(frame->Commit(),"PNG frame commit");check(encoder->Commit(),"PNG commit");
    }
    void init(HWND hwnd) {
        D3D_FEATURE_LEVEL level;auto hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
        if(FAILED(hr)){software=true;check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context),"Create graphics device");}
        ComPtr<IDXGIDevice>dxgi;check(device.As(&dxgi),"DXGI device");ComPtr<IDXGIAdapter>adapter;check(dxgi->GetAdapter(&adapter),"Graphics adapter");ComPtr<IDXGIFactory2>factory;check(adapter->GetParent(IID_PPV_ARGS(&factory)),"DXGI factory");
        DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=desc.Height=600;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;desc.Scaling=DXGI_SCALING_STRETCH;
        check(factory->CreateSwapChainForComposition(device.Get(),&desc,nullptr,&swap),"Transparent swap chain");
        check(DCompositionCreateDevice(dxgi.Get(),IID_PPV_ARGS(&composition)),"DirectComposition");check(composition->CreateTargetForHwnd(hwnd,TRUE,&target),"Composition target");check(composition->CreateVisual(&visual),"Visual");check(visual->SetContent(swap.Get()),"Visual content");check(target->SetRoot(visual.Get()),"Visual root");check(composition->Commit(),"Composition commit");
        auto shader=resource(102);ComPtr<ID3DBlob>v,p,errors;
        hr=D3DCompile(shader.data(),shader.size(),"particles.hlsl",nullptr,nullptr,"VS","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&v,&errors);if(FAILED(hr))throw std::runtime_error(errors?static_cast<char*>(errors->GetBufferPointer()):"Vertex shader failed");
        check(D3DCompile(shader.data(),shader.size(),"particles.hlsl",nullptr,nullptr,"PS","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&p,&errors),"Pixel shader");
        check(device->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs),"Vertex shader creation");check(device->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps),"Pixel shader creation");
        D3D11_BUFFER_DESC b{};b.ByteWidth=sizeof(Scene);b.Usage=D3D11_USAGE_DEFAULT;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;check(device->CreateBuffer(&b,nullptr,&constants),"Constants");
        D3D11_BLEND_DESC bd{};auto&rt=bd.RenderTarget[0];rt.BlendEnable=TRUE;rt.SrcBlend=D3D11_BLEND_ONE;rt.DestBlend=D3D11_BLEND_ONE;rt.BlendOp=D3D11_BLEND_OP_ADD;rt.SrcBlendAlpha=D3D11_BLEND_ONE;rt.DestBlendAlpha=D3D11_BLEND_ONE;rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;check(device->CreateBlendState(&bd,&blend),"Blending");
        auto post=resource(103);auto compile=[&](const char*entry,const char*profile){ComPtr<ID3DBlob>code,err;HRESULT result=D3DCompile(post.data(),post.size(),"post.hlsl",nullptr,nullptr,entry,profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&err);if(FAILED(result))throw std::runtime_error(err?static_cast<char*>(err->GetBufferPointer()):"Post shader failed");return code;};
        auto fv=compile("FullVS","vs_5_0");check(device->CreateVertexShader(fv->GetBufferPointer(),fv->GetBufferSize(),nullptr,&fullVS),"Post VS");
        auto ep=compile("ExtractPS","ps_5_0"),sp=compile("ScatterPS","ps_5_0"),hp=compile("ScatterHPS","ps_5_0"),cp=compile("CompositePS","ps_5_0");check(device->CreatePixelShader(ep->GetBufferPointer(),ep->GetBufferSize(),nullptr,&extractPS),"Extract PS");check(device->CreatePixelShader(sp->GetBufferPointer(),sp->GetBufferSize(),nullptr,&scatterPS),"Scatter PS");check(device->CreatePixelShader(hp->GetBufferPointer(),hp->GetBufferSize(),nullptr,&scatterHPS),"Horizontal scatter PS");check(device->CreatePixelShader(cp->GetBufferPointer(),cp->GetBufferSize(),nullptr,&compositePS),"Composite PS");
        auto fx=compile("FxaaPS","ps_5_0");check(device->CreatePixelShader(fx->GetBufferPointer(),fx->GetBufferSize(),nullptr,&fxaaPS),"FXAA PS");
        D3D11_BUFFER_DESC pb{};pb.ByteWidth=48;pb.Usage=D3D11_USAGE_DEFAULT;pb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;check(device->CreateBuffer(&pb,nullptr,&postBuffer),"Post constants");
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_BORDER;sd.MaxLOD=D3D11_FLOAT32_MAX;check(device->CreateSamplerState(&sd,&sampler),"Post sampler");
        D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;check(device->CreateRasterizerState(&rs,&raster),"Rasterizer");
    }
    void resize(unsigned size){if(size==dimension)return;context->OMSetRenderTargets(0,nullptr,nullptr);rtv.Reset();check(swap->ResizeBuffers(0,size,size,DXGI_FORMAT_UNKNOWN,0),"Resize surface");ComPtr<ID3D11Texture2D>buffer;check(swap->GetBuffer(0,IID_PPV_ARGS(&buffer)),"Frame buffer");check(device->CreateRenderTargetView(buffer.Get(),nullptr,&rtv),"Render target");surface(energySurface,size);surface(brightSurface,std::max(1u,size/2),true);surface(blurSurface,std::max(1u,size/2),true);surface(bloomSurface,std::max(1u,size/2));resolvedSurface={};dimension=size;}
    void render(Scene&scene){
        resize(unsigned(scene.framing[0]>0?scene.framing[0]:scene.view[0]));float clear[4]={};context->ClearRenderTargetView(energySurface.rt.Get(),clear);context->OMSetRenderTargets(1,energySurface.rt.GetAddressOf(),nullptr);context->OMSetBlendState(blend.Get(),nullptr,0xffffffff);context->RSSetState(raster.Get());D3D11_VIEWPORT vp{0,0,float(dimension),float(dimension),0,1};context->RSSetViewports(1,&vp);
        context->UpdateSubresource(constants.Get(),0,nullptr,&scene,0,0);context->VSSetConstantBuffers(0,1,constants.GetAddressOf());context->PSSetConstantBuffers(0,1,constants.GetAddressOf());context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);unsigned count=unsigned(scene.grid[0]*scene.grid[1]);context->DrawInstanced(6,count+count/2,0,0);
        float data[12]={float(dimension),float(dimension),scene.atmosphere[1],scene.atmosphere[2],scene.tint[0],scene.tint[1],scene.tint[2],scene.atmosphere[3],scene.interior[1]*1.25f,scene.tint[3],scene.waveSettings[3],diagnosticNoDither?-1.f:0.f};
        context->UpdateSubresource(postBuffer.Get(),0,nullptr,data,0,0);context->PSSetConstantBuffers(0,1,postBuffer.GetAddressOf());context->VSSetShader(fullVS.Get(),nullptr,0);context->OMSetBlendState(nullptr,nullptr,0xffffffff);context->PSSetSamplers(0,1,sampler.GetAddressOf());unsigned halfSize=std::max(1u,dimension/2);
        fullPass(brightSurface.rt.Get(),extractPS.Get(),energySurface.srv.Get(),nullptr,halfSize);
        context->OMSetRenderTargets(0,nullptr,nullptr);context->GenerateMips(brightSurface.srv.Get());
        fullPass(blurSurface.rt.Get(),scatterHPS.Get(),brightSurface.srv.Get(),nullptr,halfSize);
        context->OMSetRenderTargets(0,nullptr,nullptr);context->GenerateMips(blurSurface.srv.Get());
        fullPass(bloomSurface.rt.Get(),scatterPS.Get(),blurSurface.srv.Get(),nullptr,halfSize);
        if(scene.waveSettings[3]>0){
            if(!resolvedSurface.texture)surface(resolvedSurface,dimension);
            fullPass(resolvedSurface.rt.Get(),compositePS.Get(),energySurface.srv.Get(),bloomSurface.srv.Get(),dimension);
            fullPass(rtv.Get(),fxaaPS.Get(),resolvedSurface.srv.Get(),nullptr,dimension);
        }else fullPass(rtv.Get(),compositePS.Get(),energySurface.srv.Get(),bloomSurface.srv.Get(),dimension);
        if(!snapshotPath.empty()){snapshot(snapshotPath);snapshotPath.clear();}check(swap->Present(1,0),"Present");
    }
};
