#pragma once
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <array>
#include <complex>
#include <atomic>
#include <mutex>
#include <thread>
#include <cmath>
#include <algorithm>
using Microsoft::WRL::ComPtr;
constexpr float PI = 3.14159265358979323846f;
struct Rhythm {
    float displacement=0,velocity=0,transient=0,bass=0,dominant=0,cooldown=0;
    std::array<float,64> previous{};
    std::array<float,4> waveAge{10,10,10,10},wavePower{};
    std::array<float,4> kickAge{10,10,10,10},kickPower{};
    float waveCooldown=0;unsigned waveHead=0;
    void update(const std::array<float,64>&bands,float rms,float dt,float waveSpeed=1){
        float flux[3]{},level[3]{};int count[3]{};
        for(int i=0;i<60;++i){int region=i<18?0:i<44?1:2;flux[region]+=std::max(0.f,bands[i]-previous[i]);level[region]+=bands[i];++count[region];}
        float subFlux=0,subLevel=0;for(int i=0;i<13;++i){subFlux+=std::max(0.f,bands[i]-previous[i]);subLevel+=bands[i];}
        subFlux=subFlux/13/(.08f+subLevel/13*.65f);
        for(auto&age:waveAge)age=std::min(10.f,age+dt);for(auto&age:kickAge)age=std::min(10.f,age+dt);waveCooldown=std::max(0.f,waveCooldown-dt);
        waveHead=unsigned(std::max_element(waveAge.begin(),waveAge.end())-waveAge.begin());
        if(rms>.002f&&subFlux>.10f&&waveCooldown<=0&&waveAge[waveHead]>=2.2f/waveSpeed){waveAge[waveHead]=0;wavePower[waveHead]=std::clamp((subFlux-.10f)*2.8f,.08f,1.f);waveCooldown=.16f;}
        previous=bands;float strongest=0;int region=0;
        for(int j=0;j<3;++j){level[j]/=count[j];flux[j]=flux[j]/count[j]/(0.08f+level[j]*0.65f)*(j==0?1.3f:j==1?1.f:0.7f);if(flux[j]>strongest){strongest=flux[j];region=j;}}
        bass=level[0];cooldown=std::max(0.f,cooldown-dt);transient*=std::exp(-dt/0.09f);
        if(rms>0.002f&&strongest>0.10f&&cooldown<=0){float hit=std::clamp((strongest-0.10f)*2.4f,0.f,1.f);velocity=std::min(18.f,velocity+hit*18);transient=std::max(transient,hit);dominant=float(region);cooldown=0.085f;auto slot=std::max_element(kickAge.begin(),kickAge.end())-kickAge.begin();kickAge[slot]=0;kickPower[slot]=hit;}
        int steps=std::max(1,int(std::ceil(dt/0.004f)));float step=dt/steps;
        for(int i=0;i<steps;++i){velocity+=(-190*displacement-17*velocity)*step;displacement+=velocity*step;}
    }
    static bool waveSelfTest(){Rhythm r;std::array<float,64>b{};for(int i=0;i<13;++i)b[i]=.6f;r.update(b,.1f,.02f,.3f);float power=r.wavePower[0];if(power<.5f)return false;b.fill(0);for(int i=0;i<20;++i)r.update(b,0,.02f,.3f);if(r.wavePower[0]!=power||r.waveAge[0]<.39f)return false;for(int i=44;i<60;++i)b[i]=.8f;r.update(b,.1f,.02f,.3f);return r.wavePower[1]==0&&r.wavePower[0]==power;}
    static bool kickSelfTest(){Rhythm r;std::array<float,64>b{};for(int beat=0;beat<7;++beat){b.fill(0);for(int i=0;i<6;++i)r.update(b,0,.02f,.3f);for(int i=0;i<18;++i)b[i]=.6f;r.update(b,.1f,.02f,.3f);auto slot=std::min_element(r.kickAge.begin(),r.kickAge.end())-r.kickAge.begin();if(r.kickAge[slot]!=0||r.kickPower[slot]<.5f)return false;}auto p=r.kickPower;b.fill(0);r.update(b,0,.2f,.3f);Rhythm mid;for(int i=18;i<44;++i)b[i]=.6f;mid.update(b,.1f,.02f);return r.kickPower==p&&*std::min_element(r.kickAge.begin(),r.kickAge.end())>=.2f&&mid.kickPower[0]>.5f&&mid.wavePower[0]==0;}
    static bool selfTest(){Rhythm r;std::array<float,64>b{};b.fill(.45f);r.update(b,.1f,.02f);float first=r.transient,peak=r.displacement;for(int i=0;i<8;++i){r.update(b,.1f,.02f);peak=std::max(peak,r.displacement);}for(int i=0;i<150;++i)r.update(b,.1f,.02f);if(first<.5f||peak<.3f||std::abs(r.displacement)>.001f||r.transient>.001f)return false;b.fill(0);r.update(b,0,.02f);if(r.transient>.001f)return false;for(int i=0;i<18;++i)b[i]=.5f;r.update(b,.1f,.02f);return r.transient>.5f&&r.dominant==0;}
};
struct AudioFrame { std::array<float,64> bands{}; float rms=0, peak=0; bool connected=false; unsigned rate=0; Rhythm rhythm; };
class Audio {
    std::atomic<float> propagationSpeed{1};
    std::atomic<bool> stop{false}; std::thread worker; std::mutex mutex; AudioFrame state;
    std::array<float,2048> ring{}; unsigned cursor=0, since=0; Rhythm rhythm;
    void analyze(unsigned rate) {
        std::array<std::complex<float>,2048> a; float sum=0, peak=0;
        for(unsigned i=0;i<2048;++i) { float s=ring[(cursor+i)%2048]; sum+=s*s; peak=std::max(peak,std::abs(s)); a[i]=s*(0.5f-0.5f*std::cos(2*PI*i/2047)); }
        for(unsigned i=1,j=0;i<2048;++i) { unsigned bit=1024; for(;j&bit;bit>>=1)j^=bit; j^=bit; if(i<j)std::swap(a[i],a[j]); }
        for(unsigned len=2;len<=2048;len*=2) { auto wlen=std::polar(1.f,-2*PI/len); for(unsigned i=0;i<2048;i+=len) { std::complex<float>w=1; for(unsigned j=0;j<len/2;++j) { auto u=a[i+j],v=a[i+j+len/2]*w; a[i+j]=u+v;a[i+j+len/2]=u-v;w*=wlen; } } }
        AudioFrame f; f.connected=true;f.rate=rate;f.rms=std::sqrt(sum/2048);f.peak=peak;
        for(int i=0;i<64;++i) { float lo=35*std::pow(18000.f/35,i/64.f),hi=35*std::pow(18000.f/35,(i+1)/64.f); int l=std::clamp(int(lo*2048/rate),1,1023),h=std::clamp(int(hi*2048/rate),l+1,1024); float p=0;for(int k=l;k<h;++k)p=std::max(p,std::abs(a[k])/512); f.bands[i]=std::clamp(std::log1p(p*35)/3.f,0.f,1.f); }
        rhythm.update(f.bands,f.rms,1024.f/rate,propagationSpeed.load());f.rhythm=rhythm;
        std::lock_guard<std::mutex> lock(mutex);state=f;
    }
    void session(IMMDeviceEnumerator* enumerator) {
        ComPtr<IMMDevice> device; if(FAILED(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&device)))return;
        LPWSTR id=nullptr;device->GetId(&id);std::wstring original=id?id:L"";CoTaskMemFree(id);
        ComPtr<IAudioClient> client;if(FAILED(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,&client)))return;
        WAVEFORMATEX* fmt=nullptr;if(FAILED(client->GetMixFormat(&fmt)))return;
        WAVEFORMATEXTENSIBLE ext{};memcpy(&ext,fmt,std::min(sizeof(ext),sizeof(WAVEFORMATEX)+size_t(fmt->cbSize)));
        HRESULT hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,1000000,0,fmt,nullptr);CoTaskMemFree(fmt);if(FAILED(hr))return;
        bool floating=ext.Format.wFormatTag==WAVE_FORMAT_IEEE_FLOAT || (ext.Format.wFormatTag==WAVE_FORMAT_EXTENSIBLE && IsEqualGUID(ext.SubFormat,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT));
        unsigned bits=ext.Format.wBitsPerSample,channels=ext.Format.nChannels,rate=ext.Format.nSamplesPerSec;
        if(!channels || !(bits==16||bits==24||bits==32) || (floating&&bits!=32))return;
        ComPtr<IAudioCaptureClient> capture;if(FAILED(client->GetService(IID_PPV_ARGS(&capture))))return;
        if(FAILED(client->Start()))return;
        { std::lock_guard<std::mutex> lock(mutex);state.connected=true;state.rate=rate; }
        ULONGLONG check=GetTickCount64(),last=check; ring.fill(0);cursor=since=0;rhythm={};
        while(!stop) {
            UINT32 count=0;hr=capture->GetNextPacketSize(&count);if(FAILED(hr))break;
            while(count && !stop) {
                BYTE* data=nullptr;UINT32 frames=0;DWORD flags=0;hr=capture->GetBuffer(&data,&frames,&flags,nullptr,nullptr);if(FAILED(hr))break;
                for(unsigned i=0;i<frames;++i) { float mono=0; if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT))for(unsigned c=0;c<channels;++c) { const BYTE*p=data+i*ext.Format.nBlockAlign+c*(bits/8);float v=0;
                    if(floating)memcpy(&v,p,4);else if(bits==16){short n;memcpy(&n,p,2);v=n/32768.f;}else if(bits==32){int n;memcpy(&n,p,4);v=n/2147483648.f;}else{int n=int((unsigned(p[2])<<24)|(unsigned(p[1])<<16)|(unsigned(p[0])<<8));v=n/2147483648.f;} if(std::isfinite(v))mono+=v;
                }ring[cursor]=mono/channels;cursor=(cursor+1)%2048;if(++since>=1024){since=0;analyze(rate);} }
                capture->ReleaseBuffer(frames);last=GetTickCount64();hr=capture->GetNextPacketSize(&count);if(FAILED(hr))break;
            }
            if(FAILED(hr))break;
            auto now=GetTickCount64();if(now-last>150){std::lock_guard<std::mutex>lock(mutex);state.rms=state.peak=0;state.bands.fill(0);rhythm.update(state.bands,0,.01f);state.rhythm=rhythm;}
            if(now-check>1500){check=now;ComPtr<IMMDevice> current;if(FAILED(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&current)))break;LPWSTR cid=nullptr;current->GetId(&cid);bool changed=!cid||original!=cid;CoTaskMemFree(cid);if(changed)break;}
            Sleep(10);
        }client->Stop();
    }
public:
    void setWaveSpeed(float speed){propagationSpeed=std::clamp(speed,.3f,3.f);}
    void start(){worker=std::thread([this]{CoInitializeEx(nullptr,COINIT_MULTITHREADED);ComPtr<IMMDeviceEnumerator>e;CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&e));while(!stop){if(e)session(e.Get());{std::lock_guard<std::mutex>lock(mutex);state={};}for(int i=0;i<20&&!stop;++i)Sleep(50);}CoUninitialize();});}
    AudioFrame get(){std::lock_guard<std::mutex>lock(mutex);return state;}
    ~Audio(){stop=true;if(worker.joinable())worker.join();}
    static bool selfTest(){Audio a;for(unsigned i=0;i<2048;++i)a.ring[i]=0.5f*std::sin(2*PI*1000*i/48000);a.analyze(48000);auto f=a.get();auto peak=int(std::max_element(f.bands.begin(),f.bands.end())-f.bands.begin());return f.rms>0.34f&&f.rms<0.37f&&peak>=32&&peak<=36;}
};
