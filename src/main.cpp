#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <windowsx.h>
#include <shlobj.h>
#include <psapi.h>
#include <bcrypt.h>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <chrono>
#include <iomanip>
#include "audio.h"
#include "graphics.h"
#include "breath.h"

// Factory preset captured from the user's saved browser settings, 2026-09-17.
struct Config { int size=505,fps=60,density=3,mode=0,x=80,y=80; float sensitivity=1.5f,pointSize=1.2f,motion=1.3f,glow=2.3f,opacity=0.9f,innerDensity=0.05f,innerOpacity=0.3f,flowStrength=0.35f,waveStrength=1.0f,waveSpeed=0.5f,physicsFreedom=0.5f,rotationSpeed=0.5f,waveRoughness=1.5f,bloomStrength=1.0f,bloomSpread=1.75f,heatStrength=3.0f,outerWaveInfluence=0.25f,innerBounce=1.0f,aaStrength=0.75f,bloomPulse=2.0f,bloomDecay=0.35f;bool topmost=true,locked=false,visible=true,paused=false,demo=false;std::string color="#74BFFF"; };
Config config;std::mutex configMutex;Audio audio;std::atomic<bool>running{true};HWND windowHandle=nullptr;unsigned short port=0;std::string token;std::wstring dataDir,iniPath;std::atomic<float>measuredFps{0},cpuPercent{0};std::atomic<unsigned>memoryMb{0};bool softwareRenderer=false;NOTIFYICONDATAW tray{};UINT taskbarCreated=0;std::atomic<float> orientation[3]{};std::atomic<float> breathEnvelope{0};
constexpr UINT APPLY=WM_APP+2,TRAY=WM_APP+3,OPEN_SETTINGS=WM_APP+4;
std::wstring widen(const std::string&s){int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),w.data(),n);return w;}
std::string narrow(const std::wstring&s){int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);std::string a(n,0);WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),a.data(),n,nullptr,nullptr);return a;}
Config getConfig(){std::lock_guard<std::mutex>lock(configMutex);return config;}
void saveConfig(){auto c=getConfig();std::wostringstream s;s<<L"[visual]\nsize="<<c.size<<L"\nfps="<<c.fps<<L"\ndensity="<<c.density<<L"\nmode="<<c.mode<<L"\nx="<<c.x<<L"\ny="<<c.y<<L"\nsensitivity="<<c.sensitivity<<L"\npointSize="<<c.pointSize<<L"\nmotion="<<c.motion<<L"\nglow="<<c.glow<<L"\nopacity="<<c.opacity<<L"\ntopmost="<<c.topmost<<L"\nlocked="<<c.locked<<L"\ninnerDensity="<<c.innerDensity<<L"\ninnerOpacity="<<c.innerOpacity<<L"\nflowStrength="<<c.flowStrength<<L"\nwaveStrength="<<c.waveStrength<<L"\nrotationSpeed="<<c.rotationSpeed<<L"\nwaveRoughness="<<c.waveRoughness<<L"\nbloomStrength="<<c.bloomStrength<<L"\nbloomSpread="<<c.bloomSpread<<L"\nbloomPulse="<<c.bloomPulse<<L"\nbloomDecay="<<c.bloomDecay<<L"\naaStrength="<<c.aaStrength<<L"\ninnerBounce="<<c.innerBounce<<L"\nouterWaveInfluence="<<c.outerWaveInfluence<<L"\nheatStrength="<<c.heatStrength<<L"\nphysicsFreedom="<<c.physicsFreedom<<L"\nwaveSpeed="<<c.waveSpeed<<L"\ncolor="<<widen(c.color)<<L"\n";
    auto temp=iniPath+L".tmp";std::ofstream out(temp,std::ios::binary);auto content=narrow(s.str());out.write(content.data(),content.size());out.close();MoveFileExW(temp.c_str(),iniPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
bool validColor(const std::string&s){return s.size()==7&&s[0]=='#'&&std::all_of(s.begin()+1,s.end(),[](unsigned char c){return std::isxdigit(c)!=0;});}
bool updateConfig(const std::map<std::string,std::string>& values,std::string&error){std::lock_guard<std::mutex>lock(configMutex);auto next=config;
    for(auto&[key,value]:values){if(key=="color"){if(!validColor(value)){error="Invalid color";return false;}next.color=value;continue;}
        char*end=nullptr;float v=strtof(value.c_str(),&end);if(value.empty()||*end||!std::isfinite(v)||std::abs(v)>100000){error="Invalid number";return false;}
        auto range=[&](float lo,float hi){return v>=lo&&v<=hi;};bool ok=true;
        if(key=="size"){ok=range(160,2400)&&v==int(v);next.size=int(v);}else if(key=="fps"){ok=v==30||v==60||v==120;next.fps=int(v);}else if(key=="density"){ok=range(1,3)&&v==int(v);next.density=int(v);}else if(key=="mode"){ok=v==0||v==1;next.mode=int(v);}else if(key=="sensitivity"){ok=range(0.1f,5);next.sensitivity=v;}else if(key=="pointSize"){ok=range(0.5f,4);next.pointSize=v;}else if(key=="motion"){ok=range(0,3);next.motion=v;}else if(key=="glow"){ok=range(0,3);next.glow=v;}else if(key=="opacity"){ok=range(0.1f,1);next.opacity=v;}else if(key=="innerDensity"){ok=range(0,1);next.innerDensity=v;}else if(key=="innerOpacity"){ok=range(0,1);next.innerOpacity=v;}else if(key=="flowStrength"){ok=range(0,2);next.flowStrength=v;}else if(key=="waveStrength"){ok=range(0,3);next.waveStrength=v;}else if(key=="rotationSpeed"){ok=range(0.f,2.f);next.rotationSpeed=v;}else if(key=="waveRoughness"){ok=range(0.f,2.f);next.waveRoughness=v;}else if(key=="bloomStrength"){ok=range(0.f,3.f);next.bloomStrength=v;}else if(key=="bloomSpread"){ok=range(0.3f,2.f);next.bloomSpread=v;}else if(key=="bloomPulse"){ok=range(0.f,3.f);next.bloomPulse=v;}else if(key=="bloomDecay"){ok=range(.1f,1.5f);next.bloomDecay=v;}else if(key=="aaStrength"){ok=range(0.f,1.f);next.aaStrength=v;}else if(key=="innerBounce"){ok=range(0.f,2.f);next.innerBounce=v;}else if(key=="outerWaveInfluence"){ok=range(0.f,1.f);next.outerWaveInfluence=v;}else if(key=="heatStrength"){ok=range(0.f,3.f);next.heatStrength=v;}else if(key=="physicsFreedom"){ok=range(0,1);next.physicsFreedom=v;}else if(key=="waveSpeed"){ok=range(.3f,3);next.waveSpeed=v;}else if(key=="topmost"){ok=v==0||v==1;next.topmost=v!=0;}else if(key=="locked"){ok=v==0||v==1;next.locked=v!=0;}else if(key=="demo"){ok=v==0||v==1;next.demo=v!=0;}else {error="Unknown setting";return false;}if(!ok){error="Out of range";return false;}
    }config=next;return true;
}
void loadConfig(){std::map<std::string,std::string>v;for(auto key:{"size","fps","density","mode","sensitivity","pointSize","motion","glow","opacity","locked","topmost","color","innerDensity","innerOpacity","flowStrength","waveStrength","waveSpeed","physicsFreedom","rotationSpeed","waveRoughness","bloomStrength","bloomSpread","heatStrength","outerWaveInfluence","innerBounce","aaStrength","bloomPulse","bloomDecay"}){wchar_t value[128];GetPrivateProfileStringW(L"visual",widen(key).c_str(),L"",value,128,iniPath.c_str());if(*value){std::string error;updateConfig({{key,narrow(value)}},error);}}
    config.x=int(GetPrivateProfileIntW(L"visual",L"x",80,iniPath.c_str()));config.y=int(GetPrivateProfileIntW(L"visual",L"y",80,iniPath.c_str()));
}
std::string stateJson(){auto c=getConfig();auto a=audio.get();std::ostringstream s;s<<std::fixed<<std::setprecision(3);s<<"{\"size\":"<<c.size<<",\"fps\":"<<c.fps<<",\"density\":"<<c.density<<",\"mode\":"<<c.mode<<",\"sensitivity\":"<<c.sensitivity<<",\"pointSize\":"<<c.pointSize<<",\"motion\":"<<c.motion<<",\"glow\":"<<c.glow<<",\"opacity\":"<<c.opacity<<",\"innerDensity\":"<<c.innerDensity<<",\"innerOpacity\":"<<c.innerOpacity<<",\"flowStrength\":"<<c.flowStrength<<",\"waveStrength\":"<<c.waveStrength<<",\"rotationSpeed\":"<<c.rotationSpeed<<",\"waveRoughness\":"<<c.waveRoughness<<",\"bloomStrength\":"<<c.bloomStrength<<",\"bloomSpread\":"<<c.bloomSpread<<",\"bloomPulse\":"<<c.bloomPulse<<",\"bloomDecay\":"<<c.bloomDecay<<",\"bloomEnvelope\":"<<breathEnvelope.load()<<",\"effectiveBloom\":"<<c.bloomStrength*(1+c.bloomPulse*breathEnvelope.load())<<",\"aaStrength\":"<<c.aaStrength<<",\"innerBounce\":"<<c.innerBounce<<",\"outerWaveInfluence\":"<<c.outerWaveInfluence<<",\"heatStrength\":"<<c.heatStrength<<",\"physicsFreedom\":"<<c.physicsFreedom<<",\"waveSpeed\":"<<c.waveSpeed<<",\"topmost\":"<<c.topmost<<",\"locked\":"<<c.locked<<",\"demo\":"<<c.demo<<",\"visible\":"<<c.visible<<",\"paused\":"<<c.paused<<",\"color\":\""<<c.color<<"\",\"x\":"<<c.x<<",\"y\":"<<c.y<<",\"audioConnected\":"<<a.connected<<",\"rms\":"<<a.rms<<",\"sampleRate\":"<<a.rate<<",\"actualFps\":"<<measuredFps.load()<<",\"cpu\":"<<cpuPercent.load()<<",\"memoryMb\":"<<memoryMb.load()<<",\"softwareRenderer\":"<<softwareRenderer<<",\"bands\":[";for(int i=0;i<64;++i){if(i)s<<',';s<<a.bands[i];}s<<"],\"spring\":"<<a.rhythm.displacement<<",\"transient\":"<<a.rhythm.transient<<",\"bass\":"<<a.rhythm.bass<<",\"dominant\":"<<a.rhythm.dominant<<",\"waves\":[";for(int k=0;k<4;++k){if(k)s<<",";s<<"["<<a.rhythm.waveAge[k]<<","<<a.rhythm.wavePower[k]<<"]";}s<<"],\"kicks\":[";for(int k=0;k<4;++k){if(k)s<<",";s<<"["<<a.rhythm.kickAge[k]<<","<<a.rhythm.kickPower[k]<<"]";}s<<"],\"orientation\":["<<orientation[0].load()<<","<<orientation[1].load()<<","<<orientation[2].load()<<"]}";return s.str();}
void openSettings(){std::wstring url=L"http://127.0.0.1:"+std::to_wstring(port)+L"/#"+widen(token);ShellExecuteW(nullptr,L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
std::string urlDecode(const std::string&s){std::string o;for(size_t i=0;i<s.size();++i){if(s[i]=='%'&&i+2<s.size()){auto hex=s.substr(i+1,2);char*end;long v=strtol(hex.c_str(),&end,16);if(*end)throw std::runtime_error("Bad encoding");o+=char(v);i+=2;}else o+=s[i]=='+'?' ':s[i];}return o;}
std::map<std::string,std::string> form(const std::string&s){std::map<std::string,std::string>out;size_t pos=0;while(pos<s.size()){auto end=s.find('&',pos);if(end==std::string::npos)end=s.size();auto eq=s.find('=',pos);if(eq==std::string::npos||eq>=end)throw std::runtime_error("Bad form");out[urlDecode(s.substr(pos,eq-pos))]=urlDecode(s.substr(eq+1,end-eq-1));pos=end+1;}return out;}
class Server {
    SOCKET listener=INVALID_SOCKET;std::thread worker;std::string html;
    void reply(SOCKET client,int code,const std::string&body,const char*type="application/json; charset=utf-8"){
        std::string header="HTTP/1.1 "+std::to_string(code)+(code==200?" OK\r\n":" Error\r\n")+"Content-Type: "+type+"\r\nContent-Length: "+std::to_string(body.size())+"\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'\r\nConnection: close\r\n\r\n";
        auto sendAll=[&](const std::string&s){size_t p=0;while(p<s.size()){int n=send(client,s.data()+p,int(s.size()-p),0);if(n<=0)return;p+=n;}};sendAll(header);sendAll(body);
    }
    void handle(SOCKET client){DWORD timeout=1200;setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char*>(&timeout),sizeof(timeout));setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<char*>(&timeout),sizeof(timeout));std::string request;char buffer[4096];size_t split=std::string::npos;
        while(request.size()<16384){int n=recv(client,buffer,sizeof(buffer),0);if(n<=0)return;request.append(buffer,n);split=request.find("\r\n\r\n");if(split!=std::string::npos)break;}if(split==std::string::npos){reply(client,413,"{}");return;}
        std::istringstream input(request.substr(0,split));std::string method,path,version,line;input>>method>>path>>version;std::getline(input,line);std::map<std::string,std::string>headers;
        while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();auto colon=line.find(':');if(colon==std::string::npos)continue;auto key=line.substr(0,colon);std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return char(std::tolower(c));});auto val=line.substr(colon+1);while(!val.empty()&&val.front()==' ')val.erase(val.begin());headers[key]=val;}
        auto host="127.0.0.1:"+std::to_string(port);if(headers["host"]!=host){reply(client,403,"{}");return;}
        if(headers.count("origin")&&headers["origin"]!="http://"+host){reply(client,403,"{}");return;}
        if(path=="/"&&method=="GET"){reply(client,200,html,"text/html; charset=utf-8");return;}
        if(path=="/favicon.ico"&&method=="GET"){reply(client,204,"");return;}
        if(headers["x-luma-token"]!=token){reply(client,403,"{\"error\":\"Open settings from the tray icon\"}");return;}
        if(path=="/api/state"&&method=="GET"){reply(client,200,stateJson());return;}
        if(method!="POST"){reply(client,405,"{}");return;}
        if(headers["content-type"].find("application/x-www-form-urlencoded")!=0||headers.count("transfer-encoding")){reply(client,400,"{}");return;}
        std::string length=headers["content-length"];if(length.empty()||length.size()>5||!std::all_of(length.begin(),length.end(),[](unsigned char c){return std::isdigit(c)!=0;})){reply(client,400,"{}");return;}size_t size=std::stoul(length);if(size>4096){reply(client,413,"{}");return;}
        auto body=request.substr(split+4);while(body.size()<size){int n=recv(client,buffer,int(std::min(size-body.size(),sizeof(buffer))),0);if(n<=0)return;body.append(buffer,n);}body.resize(size);auto values=form(body);
        if(path=="/api/settings"){std::string error;if(!updateConfig(values,error)){reply(client,400,"{\"error\":\""+error+"\"}");return;}PostMessageW(windowHandle,APPLY,0,0);reply(client,200,stateJson());return;}
        if(path=="/api/action"){auto action=values["action"];int command=action=="visibility"?2:action=="pause"?3:action=="center"?5:action=="reset"?6:action=="quit"?9:0;if(!command){reply(client,400,"{}");return;}PostMessageW(windowHandle,WM_COMMAND,command,0);reply(client,200,"{\"ok\":true}");return;}
        reply(client,404,"{}");
    }
public:
    void start(){WSADATA data;if(WSAStartup(MAKEWORD(2,2),&data)!=0)throw std::runtime_error("Winsock startup failed");html=resource(101);listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(listener==INVALID_SOCKET)throw std::runtime_error("Cannot create settings socket");BOOL exclusive=TRUE;setsockopt(listener,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<char*>(&exclusive),sizeof(exclusive));sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=htons(17863);if(bind(listener,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==SOCKET_ERROR){addr.sin_port=0;if(bind(listener,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==SOCKET_ERROR)throw std::runtime_error("Cannot bind local settings server");}int size=sizeof(addr);getsockname(listener,reinterpret_cast<sockaddr*>(&addr),&size);port=ntohs(addr.sin_port);if(listen(listener,8)==SOCKET_ERROR)throw std::runtime_error("Cannot listen");
        worker=std::thread([this]{while(running){fd_set sockets;FD_ZERO(&sockets);FD_SET(listener,&sockets);timeval wait{0,200000};if(select(0,&sockets,nullptr,nullptr,&wait)<=0)continue;auto client=accept(listener,nullptr,nullptr);if(client==INVALID_SOCKET)continue;try{handle(client);}catch(...){reply(client,400,"{}");}shutdown(client,SD_SEND);DWORD drainTimeout=100;setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char*>(&drainTimeout),sizeof(drainTimeout));char discard[2048];auto deadline=GetTickCount64()+100;while(GetTickCount64()<deadline&&recv(client,discard,sizeof(discard),0)>0){}closesocket(client);}});
    }
    ~Server(){running=false;if(worker.joinable())worker.join();if(listener!=INVALID_SOCKET)closesocket(listener);WSACleanup();}
};
int glowPadding(float spread){return int(std::ceil(40*spread))+16;}
int glowPadding(const Config&c){return glowPadding(c.bloomSpread);}
void clampPlacement(Config&c){int pad=glowPadding(c);RECT proposed{c.x,c.y,c.x+c.size,c.y+c.size};MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromRect(&proposed,MONITOR_DEFAULTTONEAREST),&mi);c.size=std::min(c.size,std::max(160,int(std::min(mi.rcWork.right-mi.rcWork.left,mi.rcWork.bottom-mi.rcWork.top))-2*pad));c.x=std::clamp(c.x,int(mi.rcWork.left)+pad,std::max(int(mi.rcWork.left)+pad,int(mi.rcWork.right)-c.size-pad));c.y=std::clamp(c.y,int(mi.rcWork.top)+pad,std::max(int(mi.rcWork.top)+pad,int(mi.rcWork.bottom)-c.size-pad));}
void applyWindow(){Config c;{std::lock_guard<std::mutex>lock(configMutex);clampPlacement(config);c=config;}
    LONG_PTR ex=WS_EX_TOOLWINDOW|WS_EX_NOREDIRECTIONBITMAP|WS_EX_LAYERED;if(c.topmost)ex|=WS_EX_TOPMOST;if(c.locked)ex|=WS_EX_TRANSPARENT|WS_EX_NOACTIVATE;SetWindowLongPtrW(windowHandle,GWL_EXSTYLE,ex);SetLayeredWindowAttributes(windowHandle,0,255,LWA_ALPHA);int pad=glowPadding(c);SetWindowPos(windowHandle,c.topmost?HWND_TOPMOST:HWND_NOTOPMOST,c.x-pad,c.y-pad,c.size+2*pad,c.size+2*pad,SWP_NOACTIVATE|SWP_FRAMECHANGED);ShowWindow(windowHandle,c.visible?SW_SHOWNOACTIVATE:SW_HIDE);saveConfig();
}
void addTray(){tray.cbSize=sizeof(tray);tray.hWnd=windowHandle;tray.uID=1;tray.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;tray.uCallbackMessage=TRAY;tray.hIcon=LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1));wcscpy_s(tray.szTip,L"Luma 流光 · 右键设置 / 双击打开");Shell_NotifyIconW(NIM_ADD,&tray);}
void contextMenu(){auto c=getConfig();HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"打开设置…");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,2,c.visible?L"隐藏悬浮球":L"显示悬浮球");AppendMenuW(menu,MF_STRING,3,c.paused?L"继续动画":L"暂停动画");AppendMenuW(menu,MF_STRING|(c.topmost?MF_CHECKED:0),7,L"置顶");AppendMenuW(menu,MF_STRING|(c.locked?MF_CHECKED:0),4,L"锁定位置 / 鼠标穿透");AppendMenuW(menu,MF_STRING,5,L"移到屏幕中央");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,9,L"退出 Luma");POINT p;GetCursorPos(&p);SetForegroundWindow(windowHandle);TrackPopupMenu(menu,TPM_RIGHTBUTTON,p.x,p.y,0,windowHandle,nullptr);DestroyMenu(menu);PostMessageW(windowHandle,WM_NULL,0,0);}
LRESULT CALLBACK windowProc(HWND h,UINT msg,WPARAM w,LPARAM l){if(msg==taskbarCreated&&taskbarCreated){addTray();return 0;}switch(msg){
    case OPEN_SETTINGS:openSettings();return 0;
    case TRAY:if(l==WM_RBUTTONUP||l==WM_CONTEXTMENU)contextMenu();else if(l==WM_LBUTTONDBLCLK)openSettings();return 0;
    case WM_CONTEXTMENU:contextMenu();return 0;
    case WM_NCHITTEST:{auto c=getConfig();if(c.locked)return HTTRANSPARENT;POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);float dx=float(p.x)-glowPadding(c)-c.size/2.f,dy=float(p.y)-glowPadding(c)-c.size/2.f;if(dx*dx+dy*dy>c.size*c.size*0.235f)return HTTRANSPARENT;return HTCLIENT;}
    case WM_LBUTTONDOWN:if(!getConfig().locked){ReleaseCapture();SendMessageW(h,WM_NCLBUTTONDOWN,HTCAPTION,0);}return 0;
    case WM_EXITSIZEMOVE:{RECT r;GetWindowRect(h,&r);{std::lock_guard<std::mutex>lock(configMutex);config.x=r.left+glowPadding(config);config.y=r.top+glowPadding(config);}saveConfig();return 0;}
    case WM_MOUSEWHEEL:{std::lock_guard<std::mutex>lock(configMutex);config.size=std::clamp(config.size+GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*32,160,2400);PostMessageW(h,APPLY,0,0);return 0;}
    case APPLY:applyWindow();return 0;
    case WM_DISPLAYCHANGE:case WM_DPICHANGED:PostMessageW(h,APPLY,0,0);return 0;
    case WM_COMMAND:{int id=LOWORD(w);if(id==1){openSettings();return 0;}if(id==9){DestroyWindow(h);return 0;}{std::lock_guard<std::mutex>lock(configMutex);if(id==2)config.visible=!config.visible;if(id==3)config.paused=!config.paused;if(id==4)config.locked=!config.locked;if(id==7)config.topmost=!config.topmost;if(id==5){MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(h,MONITOR_DEFAULTTONEAREST),&mi);config.x=(mi.rcWork.left+mi.rcWork.right-config.size)/2;config.y=(mi.rcWork.top+mi.rcWork.bottom-config.size)/2;}if(id==6)config=Config{};}applyWindow();return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT p;BeginPaint(h,&p);EndPaint(h,&p);return 0;}
    case WM_CLOSE:DestroyWindow(h);return 0;
    case WM_DESTROY:Shell_NotifyIconW(NIM_DELETE,&tray);running=false;PostQuitMessage(0);return 0;
    }return DefWindowProcW(h,msg,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR args,int){if(wcsstr(args,L"--self-test"))return Audio::selfTest()&&Rhythm::selfTest()&&Rhythm::waveSelfTest()&&Rhythm::kickSelfTest()&&bloomBreathSelfTest()?0:1;
    HANDLE singleton=CreateMutexW(nullptr,FALSE,L"Local\\Luma.Flow.Visualizer.v1");if(GetLastError()==ERROR_ALREADY_EXISTS){auto old=FindWindowW(L"LumaOverlay",nullptr);if(old)PostMessageW(old,OPEN_SETTINGS,0,0);CloseHandle(singleton);return 0;}
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    try {PWSTR path=nullptr;check(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&path),"Settings directory");dataDir=std::wstring(path)+L"\\Luma";CoTaskMemFree(path);CreateDirectoryW(dataDir.c_str(),nullptr);iniPath=dataDir+L"\\settings.ini";loadConfig();
        unsigned char random[24];if(BCryptGenRandom(nullptr,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)throw std::runtime_error("Token generation failed");for(auto c:random){char hex[3];sprintf_s(hex,"%02x",c);token+=hex;}
        taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=windowProc;wc.hInstance=instance;wc.lpszClassName=L"LumaOverlay";wc.hCursor=LoadCursorW(nullptr,IDC_SIZEALL);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));RegisterClassExW(&wc);
        windowHandle=CreateWindowExW(WS_EX_TOOLWINDOW|(config.topmost?WS_EX_TOPMOST:0)|WS_EX_NOREDIRECTIONBITMAP|WS_EX_LAYERED,L"LumaOverlay",L"Luma · 流光",WS_POPUP,config.x,config.y,config.size,config.size,nullptr,nullptr,instance,nullptr);if(!windowHandle)throw std::runtime_error("Cannot create overlay window");
        Graphics graphics;graphics.init(windowHandle);if(wcsstr(args,L"--snapshot"))graphics.snapshotPath=dataDir+L"\\snapshot.png";softwareRenderer=graphics.software;Server server;server.start();audio.start();addTray();applyWindow();
        {std::ofstream session(dataDir+L"\\session.json");session<<"{\"port\":"<<port<<",\"token\":\""<<token<<"\",\"pid\":"<<GetCurrentProcessId()<<"}";}
        if(!wcsstr(args,L"--quiet"))openSettings();
        HANDLE frameTimer=CreateWaitableTimerExW(nullptr,nullptr,0x00000002,TIMER_ALL_ACCESS);if(!frameTimer)frameTimer=CreateWaitableTimerW(nullptr,FALSE,nullptr);
        auto start=std::chrono::steady_clock::now(),last=start,stats=start,next=start;float elapsed=0,energy=0,rotationTime=0;bool evidenceDone=false;Rhythm displayedRhythm;std::array<float,64>smooth{};unsigned frames=0;ULONGLONG previousCpu=0;SYSTEM_INFO sys;GetSystemInfo(&sys);
        while(running){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){running=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}if(!running)break;
            auto now=std::chrono::steady_clock::now();auto c=getConfig();audio.setWaveSpeed(c.waveSpeed);if(now<next){auto us=std::chrono::duration_cast<std::chrono::microseconds>(next-now).count();if(frameTimer){LARGE_INTEGER due;due.QuadPart=-std::max<long long>(1,us)*10;SetWaitableTimer(frameTimer,&due,0,nullptr,nullptr,FALSE);MsgWaitForMultipleObjectsEx(1,&frameTimer,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);}else MsgWaitForMultipleObjectsEx(0,nullptr,DWORD(std::max<long long>(1,us/1000)),QS_ALLINPUT,MWMO_INPUTAVAILABLE);continue;}
            float dt=std::chrono::duration<float>(now-last).count();last=now;auto a=audio.get();int targetFps=c.fps;if(softwareRenderer)targetFps=std::min(targetFps,30);if(!c.visible||c.paused)targetFps=5;else if(a.rms<0.0005f&&!c.demo)targetFps=std::min(targetFps,30);
            next=now+std::chrono::microseconds(1000000/targetFps);if(!c.paused)elapsed+=std::min(dt,0.1f);
            if(!c.paused)rotationTime+=std::min(dt,.1f)*c.rotationSpeed;
            auto noise=[](float x){float i=std::floor(x),f=x-i;f=f*f*(3-2*f);auto h=[](float n){float a=std::sin(n*127.1f+311.7f)*43758.5453f;return (a-std::floor(a))*2-1;};return h(i)+(h(i+1)-h(i))*f;};
            float seed=random[0]*.31f;orientation[0]=rotationTime*.43f+noise(rotationTime*.065f+seed)*1.1f;orientation[1]=rotationTime*.31f+noise(rotationTime*.071f+seed+17)*.85f;orientation[2]=rotationTime*.18f+noise(rotationTime*.045f+seed+39)*.7f;
            float target=c.demo?(0.15f+0.12f*std::sin(elapsed*3)+0.04f*std::sin(elapsed*7)):a.rms;float f=1-std::exp(-dt*(target>energy?20.f:6.f));energy+=(target-energy)*f;
            for(int i=0;i<64;++i){float b=c.demo?(0.13f+0.12f*std::sin(elapsed*2+i*0.3f)):a.bands[i];smooth[i]+=(b-smooth[i])*(1-std::exp(-dt*12));}
            if(!c.paused){displayedRhythm=a.rhythm;if(c.demo){float phase=std::fmod(elapsed,.6f);displayedRhythm.displacement=1.7f*std::exp(-8.5f*phase)*std::sin(10.9f*phase);displayedRhythm.transient=std::exp(-phase/.09f);displayedRhythm.bass=.22f;displayedRhythm.dominant=0;for(int k=0;k<4;++k){displayedRhythm.waveAge[k]=std::fmod(elapsed,.6f)+k*.6f;displayedRhythm.wavePower[k]=.8f;displayedRhythm.kickAge[k]=phase+k*.6f;displayedRhythm.kickPower[k]=.8f;}}}
            float breathKicks[16]{};for(int k=0;k<4;++k){breathKicks[k*4]=displayedRhythm.kickAge[k];breathKicks[k*4+1]=displayedRhythm.kickPower[k];}breathEnvelope=bloomBreath(breathKicks,c.sensitivity,c.bloomDecay);
            if(c.visible&&(!c.paused||frames==0)){Scene s{};s.framing[0]=float(c.size+2*glowPadding(c));s.view[0]=s.view[1]=float(c.size);s.view[2]=elapsed;s.view[3]=energy*3;unsigned rgb=std::stoul(c.color.substr(1),nullptr,16);s.tint[0]=((rgb>>16)&255)/255.f;s.tint[1]=((rgb>>8)&255)/255.f;s.tint[2]=(rgb&255)/255.f;s.tint[3]=c.opacity;s.options[0]=c.pointSize;s.options[1]=c.motion;s.options[2]=c.glow;s.options[3]=float(c.mode);s.grid[0]=c.density==1?128.f:c.density==2?192.f:256.f;s.grid[1]=c.density==1?48.f:c.density==2?72.f:96.f;s.grid[2]=c.sensitivity;s.grid[3]=c.outerWaveInfluence;s.rhythm[0]=displayedRhythm.displacement;s.rhythm[1]=displayedRhythm.transient;s.rhythm[2]=displayedRhythm.bass;s.rhythm[3]=displayedRhythm.dominant;s.interior[0]=c.innerDensity;s.interior[1]=c.innerOpacity;s.interior[2]=c.flowStrength;s.interior[3]=c.waveStrength;s.waveSettings[0]=c.waveSpeed;s.waveSettings[1]=c.physicsFreedom;s.waveSettings[2]=c.innerBounce;s.waveSettings[3]=c.aaStrength;for(int j=0;j<3;++j)s.rotation[j]=orientation[j].load();s.atmosphere[0]=c.waveRoughness;s.atmosphere[1]=c.bloomStrength;s.atmosphere[2]=c.bloomSpread;s.atmosphere[3]=c.heatStrength;for(int k=0;k<4;++k){s.waves[k*4]=displayedRhythm.waveAge[k];s.waves[k*4+1]=displayedRhythm.wavePower[k];s.kicks[k*4]=displayedRhythm.kickAge[k];s.kicks[k*4+1]=displayedRhythm.kickPower[k];}s.atmosphere[1]=c.bloomStrength*(1+c.bloomPulse*breathEnvelope.load());std::copy(smooth.begin(),smooth.end(),s.bands);if(wcsstr(args,L"--snapshot-attack")&&!graphics.snapshotPath.empty()){s.rhythm[0]=.7f;s.rhythm[1]=1;s.rhythm[2]=.4f;s.waves[0]=.62f;s.waves[1]=1.f;}if(wcsstr(args,L"--render-evidence")&&!evidenceDone){
                auto actual=s;Scene proof{};proof.view[2]=2;proof.view[0]=proof.view[1]=560;proof.options[0]=1.5f;proof.options[1]=.5f;proof.options[2]=1;proof.grid[0]=192;proof.grid[1]=72;proof.grid[2]=1.5f;proof.grid[3]=.2f;proof.interior[0]=.6f;proof.interior[1]=.45f;proof.interior[2]=proof.interior[3]=1;proof.waveSettings[0]=1;proof.waveSettings[1]=.5f;proof.waveSettings[2]=1;proof.waveSettings[3]=.75f;proof.atmosphere[0]=proof.atmosphere[2]=proof.atmosphere[3]=1;proof.tint[0]=.12f;proof.tint[1]=.4f;proof.tint[2]=1;proof.tint[3]=.9f;for(int k=0;k<4;++k)proof.waves[k*4]=10;
                for(int k=0;k<4;++k){proof.rotation[0]=k*.51f;proof.rotation[1]=k*.67f;proof.rotation[2]=k*.39f;graphics.snapshotPath=dataDir+L"\\sphere-"+std::to_wstring(k)+L".png";graphics.render(proof);}
                proof.options[0]=3;proof.grid[0]=256;proof.grid[1]=96;proof.rotation[0]=.4f;proof.rotation[1]=.7f;proof.rotation[2]=.2f;
                for(int k=0;k<3;++k){proof.atmosphere[1]=k==0?0:1.1f;proof.atmosphere[2]=1;proof.atmosphere[3]=k==2?2.f:0;graphics.snapshotPath=dataDir+L"\\bloom-"+std::to_wstring(k)+L".png";graphics.render(proof);}
                proof.options[0]=1.5f;proof.grid[0]=192;proof.grid[1]=72;proof.tint[0]=proof.tint[1]=proof.tint[2]=1;proof.atmosphere[1]=1.1f;proof.atmosphere[3]=1;
                for(int k=0;k<3;++k){proof.rhythm[0]=k==1?.65f:k==2?-.08f:0;proof.rhythm[1]=k==1?1.f:0;proof.rhythm[2]=k==1?.4f:0;proof.waves[0]=k==1?.57f:1.05f;proof.waves[1]=k==0?0.f:1.f;graphics.snapshotPath=dataDir+L"\\elastic-"+std::to_wstring(k)+L".png";graphics.render(proof);}
                proof.rhythm[0]=.65f;proof.waves[0]=.57f;
                for(int k=0;k<4;++k){proof.rotation[0]=k*.51f;proof.rotation[1]=k*.67f;proof.rotation[2]=k*.39f;graphics.snapshotPath=dataDir+L"\\elastic-rotation-"+std::to_wstring(k)+L".png";graphics.render(proof);}
                proof.rotation[0]=.4f;proof.rotation[1]=.7f;proof.rotation[2]=.2f;proof.interior[0]=0;proof.atmosphere[1]=0;proof.rhythm[0]=0;
                for(int k=0;k<3;++k){proof.grid[3]=k==0?0.f:k==1?.2f:1.f;graphics.snapshotPath=dataDir+L"\\coupling-"+std::to_wstring(k)+L".png";graphics.render(proof);}
                proof.grid[3]=0;proof.waves[1]=0;proof.rhythm[0]=.65f;graphics.snapshotPath=dataDir+L"\\scale-audio.png";graphics.render(proof);
                proof.rhythm[0]=0;proof.view[2]=20;graphics.snapshotPath=dataDir+L"\\shape-autonomous.png";graphics.render(proof);
                proof.view[2]=2;proof.interior[0]=.6f;proof.grid[3]=.2f;proof.atmosphere[1]=0;
                proof.kicks[0]=.14f;proof.kicks[1]=1;proof.waveSettings[2]=0;graphics.snapshotPath=dataDir+L"\\bounce-off.png";graphics.render(proof);proof.waveSettings[2]=1;
                for(int k=0;k<8;++k){proof.kicks[0]=.06f+k*.18f;graphics.snapshotPath=dataDir+L"\\bounce-"+std::to_wstring(k)+L".png";graphics.render(proof);}
                proof.kicks[1]=0;proof.rotation[0]=1.5707963f;proof.rotation[1]=proof.rotation[2]=0;proof.atmosphere[1]=1.1f;
                proof.waves[1]=0;graphics.snapshotPath=dataDir+L"\\origin-base.png";graphics.render(proof);proof.waves[1]=1;
                for(int k=0;k<3;++k){proof.waves[0]=.085f+k*.16f;graphics.snapshotPath=dataDir+L"\\origin-"+std::to_wstring(k)+L".png";graphics.render(proof);}
                proof.waves[1]=0;proof.view[2]=2;proof.options[0]=1.5f;proof.rotation[0]=.4f;proof.rotation[1]=.7f;proof.rotation[2]=.2f;proof.atmosphere[1]=0;
                for(int size:{320,560})for(int mode=0;mode<3;++mode)for(int phase=0;phase<4;++phase){
                    proof.view[0]=proof.view[1]=float(size);proof.waveSettings[3]=mode==0?0.f:mode==1?.75f:1.f;proof.rotation[3]=phase*.25f;
                    graphics.snapshotPath=dataDir+L"\\aa-"+std::to_wstring(size)+L"-"+std::to_wstring(mode)+L"-"+std::to_wstring(phase)+L".png";graphics.render(proof);
                }
                proof.waveSettings[3]=.75f;proof.rotation[3]=0;graphics.snapshotPath=dataDir+L"\\aa-repeat.png";graphics.render(proof);
                float lightKick[16]{};
                for(int phase=0;phase<5;++phase){
                    lightKick[0]=phase==2?.25f:phase==3?2.f:.05f;lightKick[1]=phase==0?0.f:1.f;
                    float pulse=phase==4?0.f:.8f;proof.atmosphere[1]=1.1f*(1+pulse*bloomBreath(lightKick,1,.35f));
                    graphics.snapshotPath=dataDir+L"\\breath-"+std::to_wstring(phase)+L".png";graphics.render(proof);
                }
                graphics.diagnosticNoDither=true;graphics.snapshotPath=dataDir+L"\\halo-current-before.png";graphics.render(proof);
                graphics.diagnosticNoDither=false;graphics.snapshotPath=dataDir+L"\\halo-current-after.png";graphics.render(proof);
                proof.interior[2]=.35f;proof.atmosphere[0]=1.5f;
                for(int tint=0;tint<2;++tint)for(int level=0;level<3;++level){
                    proof.tint[0]=tint==0?1.f:.12f;proof.tint[1]=tint==0?203.f/255:.4f;proof.tint[2]=tint==0?129.f/255:1.f;
                    proof.atmosphere[1]=1.1f;proof.atmosphere[3]=level==2?3.f:float(level);
                    graphics.snapshotPath=dataDir+L"\\heat-"+std::to_wstring(tint)+L"-"+std::to_wstring(level)+L".png";graphics.render(proof);
                }
                // Stress the actual padded presentation: maximum Bloom/breath,
                // particle size and impulse, including the smallest artwork.
                proof.atmosphere[2]=2;proof.atmosphere[1]=12;proof.options[0]=4;
                proof.grid[0]=256;proof.grid[1]=96;proof.grid[2]=5;
                proof.rhythm[0]=1.25f;proof.interior[3]=3;proof.kicks[0]=.1f;proof.kicks[1]=1;
                proof.waves[0]=.5f;proof.waves[1]=1;
                for(int size:{160,320,560})for(int angle=0;angle<3;++angle){
                    proof.view[0]=proof.view[1]=float(size);proof.framing[0]=float(size+2*glowPadding(2));
                    proof.rotation[0]=angle*.83f;proof.rotation[1]=angle*1.13f;proof.rotation[2]=angle*.37f;
                    graphics.snapshotPath=dataDir+L"\\padding-"+std::to_wstring(size)+L"-"+std::to_wstring(angle)+L".png";graphics.render(proof);
                }
                proof.view[0]=proof.view[1]=320;proof.atmosphere[1]=0;
                for(int padded=0;padded<2;++padded){proof.framing[0]=padded?512.f:0.f;
                    graphics.snapshotPath=dataDir+L"\\padding-scale-"+std::to_wstring(padded)+L".png";graphics.render(proof);}
                s=actual;evidenceDone=true;graphics.snapshotPath=dataDir+L"\\snapshot.png";
            }graphics.render(s);++frames;}
            float seconds=std::chrono::duration<float>(now-stats).count();if(seconds>=1){measuredFps=frames/seconds;frames=0;stats=now;FILETIME create,exit,kernel,user;GetProcessTimes(GetCurrentProcess(),&create,&exit,&kernel,&user);ULONGLONG used=(ULONGLONG(kernel.dwHighDateTime)<<32|kernel.dwLowDateTime)+(ULONGLONG(user.dwHighDateTime)<<32|user.dwLowDateTime);if(previousCpu)cpuPercent=float(used-previousCpu)/10000000.f/seconds*100/sys.dwNumberOfProcessors;previousCpu=used;PROCESS_MEMORY_COUNTERS memory{};if(GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory)))memoryMb=unsigned(memory.WorkingSetSize/1024/1024);}
        }
        if(frameTimer)CloseHandle(frameTimer);DeleteFileW((dataDir+L"\\session.json").c_str());
    }catch(const std::exception&e){running=false;Shell_NotifyIconW(NIM_DELETE,&tray);if(windowHandle)DestroyWindow(windowHandle);auto message=L"Luma 无法启动或渲染。\n\n"+widen(e.what())+L"\n\n请检查显卡驱动后重试。";MessageBoxW(nullptr,message.c_str(),L"Luma",MB_OK|MB_ICONERROR);CoUninitialize();CloseHandle(singleton);return 1;}
    CoUninitialize();CloseHandle(singleton);return 0;
}
