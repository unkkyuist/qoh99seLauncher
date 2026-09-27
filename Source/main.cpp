#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <array>
#include <string>
#include <stdexcept>
#include <algorithm>
#include "native_keys.h"
#include "filter_plan.h"
#include "display_plan.h"
#include "portable_runtime.h"

namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;
constexpr wchar_t ClassName[] = L"QOH99LauncherWindow";
constexpr wchar_t GameHash[] = L"a79002592953e8d9ace3e363b3115ce4452c1a0789eb5b17c9bcb032af27f1f6";
constexpr const wchar_t* Actions[] = {L"위", L"아래", L"왼쪽", L"오른쪽", L"버튼 A", L"버튼 B", L"버튼 C", L"버튼 D"};
constexpr const wchar_t* FilterNames[] = {L"원본 도트 (Nearest)", L"부드럽게 (Bilinear)", L"윤곽 보정 (xBR)", L"윤곽 보정 (xBRZ)", L"브라운관 (CRT Lottes)"};
constexpr wchar_t YouTubeUrl[]=L"https://www.youtube.com/channel/UCm8aYp4XgUhKFPjOXb_jYng";
constexpr wchar_t ThreadsUrl[]=L"https://www.threads.com/@tikiland.t";
constexpr const wchar_t* FilterTips[] = {
    L"도트 경계를 선명하게 유지합니다. 원래 화면은 640 × 480입니다.",
    L"확대한 픽셀 경계를 부드럽게 연결합니다.",
    L"캐릭터 윤곽의 계단 모양을 완화합니다. 작은 글자도 함께 바뀝니다.",
    L"두 단계 셰이더로 윤곽을 보정합니다. 실제 화면에서 비교해 보세요.",
    L"화면 휘어짐 없이 주사선과 브라운관 질감을 표현합니다."
};
constexpr int Widths[] = {640, 960, 1280, 1440, 1600};
constexpr int Heights[] = {480, 720, 960, 1080, 1200};
constexpr const wchar_t* Sizes[] = {L"640 × 480", L"960 × 720", L"1280 × 960", L"1440 × 1080", L"1600 × 1200"};
enum { IdMode=100, IdSize, IdAspect, IdFilter, IdEsc, IdSave, IdPlay, IdPreview,
       IdReset, IdReload, IdOriginal, IdExtra, IdSafe, IdKeys=200, IdYouTube=400, IdThreads };

fs::path gameDir;
fs::path KeyPath() {
    auto local=gameDir/L"LocalConfig/QOHcnf.key";
    return fs::exists(local)?local:gameDir/L"System/QOHcnf.key";
}
HWND mainWindow{}, modeBox{}, sizeBox{}, aspectBox{}, filterBox{}, extraBox{}, escapeBox{}, statusLabel{}, tipLabel{};
HWND keyButtons[2][8]{};
HFONT bodyFont{}, titleFont{};
HBRUSH bgBrush{};
int dpi=96, capturePlayer=-1, captureAction=-1;
Bytes loadedConfig;
qoh_keys::KeyBindings keys{};
bool dirty=false, initialized=false;
HANDLE gameProcess{};
DWORD gamePid{};

int S(int n) { return MulDiv(n,dpi,96); }
std::wstring Widen(const std::string& text) {
    int len=MultiByteToWideChar(CP_UTF8,0,text.c_str(),-1,nullptr,0);
    std::wstring s(static_cast<size_t>(len),L'\0');
    MultiByteToWideChar(CP_UTF8,0,text.c_str(),-1,s.data(),len);
    if (!s.empty()) s.pop_back();
    return s;
}
std::runtime_error Error(const char* text) { return std::runtime_error(std::string(text)+" (Win32="+std::to_string(GetLastError())+")"); }
void Status(const std::wstring& s) { SetWindowTextW(statusLabel,s.c_str()); }
void Report(const std::exception& ex) { MessageBoxW(mainWindow,Widen(ex.what()).c_str(),L"QOH 런처",MB_OK|MB_ICONERROR); }

Bytes Read(const fs::path& file) {
    std::ifstream f(file,std::ios::binary);
    if (!f) throw std::runtime_error("Cannot read: "+file.u8string());
    return Bytes(std::istreambuf_iterator<char>(f),{});
}
void AtomicWrite(const fs::path& file,const Bytes& bytes) {
    fs::path tmp=file; tmp+=L".launcher-tmp";
    { std::ofstream f(tmp,std::ios::binary|std::ios::trunc);
      if (!f || !f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size()) || !f.flush())
          throw std::runtime_error("Cannot write: "+tmp.u8string()); }
    if (!MoveFileExW(tmp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw Error("Atomic file replacement failed");
}
std::wstring Sha256(const Bytes& bytes) {
    BCRYPT_ALG_HANDLE alg{};
    if (BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) throw Error("SHA256 initialization failed");
    UCHAR hash[32]{};
    auto result=BCryptHash(alg,nullptr,0,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),hash,32);
    BCryptCloseAlgorithmProvider(alg,0);
    if (result<0) throw Error("SHA256 failed");
    std::wostringstream s; s<<std::hex<<std::setfill(L'0');
    for (auto c:hash) s<<std::setw(2)<<static_cast<int>(c);
    return s.str();
}
bool GameRunning() {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if (snapshot==INVALID_HANDLE_VALUE) throw Error("Cannot check game process");
    PROCESSENTRY32W e{sizeof(e)}; bool running=false;
    if (Process32FirstW(snapshot,&e)) do {
        if (_wcsicmp(e.szExeFile,L"qoh99.exe")==0 || _wcsicmp(e.szExeFile,L"Config.exe")==0) {
            HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);
            wchar_t name[32768]{}; DWORD count=32768;
            if (!p) { running=true; break; }
            if (QueryFullProcessImageNameW(p,0,name,&count)) {
                if (_wcsicmp(fs::path(name).parent_path().c_str(),gameDir.c_str())==0) running=true;
            } else running=true;
            CloseHandle(p);
        }
    } while(!running && Process32NextW(snapshot,&e));
    CloseHandle(snapshot); return running;
}
std::wstring KeyName(const qoh_keys::KeyBinding& key) {
    if (key.kind) return L"패드 "+std::to_wstring(key.kind)+L" / "+std::to_wstring(key.code);
    if (!key.code) return L"미지정";
    LONG scan=static_cast<LONG>((key.code&0x7f)<<16);
    if (key.code&0x80) scan|=1<<24;
    wchar_t name[80]{};
    if (GetKeyNameTextW(scan,name,80)) return name;
    return L"Scan "+std::to_wstring(key.code);
}
void RefreshKeys() {
    for(int p=0;p<2;p++) for(int a=0;a<8;a++)
        SetWindowTextW(keyButtons[p][a],capturePlayer==p && captureAction==a?L"키를 누르세요…":KeyName(keys[p][a]).c_str());
}
void CancelCapture() { capturePlayer=captureAction=-1; RefreshKeys(); }
bool DuplicateKeys() {
    for(int n=0;n<16;n++) for(int m=n+1;m<16;m++) {
        auto a=keys[n/8][n%8], b=keys[m/8][m%8];
        if (!a.kind && !b.kind && a.code && a.code==b.code) return true;
    }
    return false;
}
void Changed() {
    dirty=true;
    Status(DuplicateKeys()?L"중복 키가 있습니다. 각 동작에 다른 키를 지정하세요.":L"변경된 설정이 있습니다. 저장하거나 게임 시작을 누르세요.");
}
int Selected(HWND box) { return static_cast<int>(SendMessageW(box,CB_GETCURSEL,0,0)); }
void RefreshDisplay() {
    bool windowed=Selected(modeBox)==1;
    EnableWindow(sizeBox,windowed);
    int base=std::clamp(Selected(filterBox),0,4);
    if(base==3) SendMessageW(extraBox,CB_SETCURSEL,0,0);
    EnableWindow(extraBox,base!=3);
    auto tip=std::wstring(FilterTips[base]);
    if(base==3) tip=L"xBRZ는 두 단계를 모두 사용하므로 추가 효과를 함께 쓸 수 없습니다.";
    SetWindowTextW(tipLabel,tip.c_str());
}
void LoadSettings() {
    initialized=false;
    loadedConfig=Read(KeyPath());
    keys=qoh_keys::DecodeKeys(loadedConfig);
    auto prefs=gameDir/L"QOH-Launcher.ini";
    auto pref=[&](const wchar_t* key,int def,int max) {
        return std::clamp(static_cast<int>(GetPrivateProfileIntW(L"launcher",key,def,prefs.c_str())),0,max);
    };
    SendMessageW(modeBox,CB_SETCURSEL,pref(L"mode",0,1),0);
    SendMessageW(sizeBox,CB_SETCURSEL,pref(L"size",1,4),0);
    SendMessageW(filterBox,CB_SETCURSEL,pref(L"filter",0,4),0);
    SendMessageW(extraBox,CB_SETCURSEL,pref(L"extra",0,2),0);
    SendMessageW(aspectBox,BM_SETCHECK,pref(L"aspect",1,1)?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(escapeBox,BM_SETCHECK,pref(L"blockEscape",1,1)?BST_CHECKED:BST_UNCHECKED,0);
    CancelCapture(); RefreshDisplay(); dirty=false; initialized=true;
    Status(L"설정을 불러왔습니다. 게임 시작 시 현재 설정이 적용됩니다.");
}
void BackupOnce(const fs::path& file) {
    fs::path backupDir=gameDir/L"LauncherBackup";
    fs::create_directories(backupDir);
    auto dest=backupDir/file.filename();
    if (fs::exists(file) && !fs::exists(dest)) fs::copy_file(file,dest);
}

qoh_display::Size WindowSize(int preset, RECT& work, RECT& frame) {
    // cnc-ddraw 7.1 centers its window on the primary display.
    if (!SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0)) throw Error("Cannot read desktop work area");
    frame={0,0,0,0};
    if (!AdjustWindowRectEx(&frame,WS_OVERLAPPEDWINDOW,FALSE,0)) throw Error("Cannot measure game window borders");
    return qoh_display::FitWindow({Widths[preset],Heights[preset]},
        {work.right-work.left-(frame.right-frame.left),work.bottom-work.top-(frame.bottom-frame.top)});
}
void SaveSettings() {
    if (!initialized) throw std::runtime_error("Game settings have not been loaded.");
    if (GameRunning()) throw std::runtime_error("QOH99 or Config.exe is running. Close it before saving settings.");
    if (DuplicateKeys()) throw std::runtime_error("Duplicate keyboard bindings. Assign unique keys to both players.");
    auto path=KeyPath();
    auto current=Read(path);
    if (current!=loadedConfig) throw std::runtime_error("Game configuration changed outside the launcher. Click Reload before saving.");
    const int mode=Selected(modeBox), size=Selected(sizeBox), filter=Selected(filterBox), extra=Selected(extraBox);
    bool aspect=SendMessageW(aspectBox,BM_GETCHECK,0,0)==BST_CHECKED;
    bool block=SendMessageW(escapeBox,BM_GETCHECK,0,0)==BST_CHECKED;
    RECT work{}, frame{};
    qoh_display::Size windowSize{};
    if(mode==1) windowSize=WindowSize(size,work,frame);
    auto newConfig=qoh_keys::EnableNativeFullscreen(qoh_keys::EncodeKeys(current,keys));
    auto plan=qoh_filters::MakePlan(filter,extra);
    if (plan.renderer==L"opengl") {
        if (!fs::is_regular_file(gameDir/plan.shader)) throw std::runtime_error("Missing filter file. Copy the complete LauncherShaders folder from the release ZIP.");
        if (plan.secondPass && !fs::is_regular_file(gameDir/(plan.shader+L".pass1"))) throw std::runtime_error("Missing second filter pass. Copy the complete LauncherShaders folder.");
    }
    if (!fs::is_regular_file(gameDir/L"ddraw.dll")) throw std::runtime_error("cnc-ddraw.dll patch is missing.");
    const auto ddraw=gameDir/L"ddraw.ini", prefs=gameDir/L"QOH-Launcher.ini";
    BackupOnce(path); BackupOnce(ddraw); BackupOnce(prefs);
    auto originalDdraw=Read(ddraw);
    bool prefsExisted=fs::exists(prefs); auto originalPrefs=prefsExisted?Read(prefs):Bytes{};
    // Edit a temporary INI through the Windows API, preserving unrelated settings.
    fs::path temp=gameDir/L"QOH-Launcher-ddraw.tmp";
    AtomicWrite(temp,originalDdraw);
    auto set=[&](const wchar_t* key,const std::wstring& val) {
        if (!WritePrivateProfileStringW(L"ddraw",key,val.c_str(),temp.c_str())) throw Error("Cannot write display settings");
        // Per-game entries take precedence over [ddraw]; update existing overrides too.
        wchar_t value[64]{};
        if (GetPrivateProfileStringW(L"qoh99",key,L"",value,64,temp.c_str())>0)
            if (!WritePrivateProfileStringW(L"qoh99",key,val.c_str(),temp.c_str())) throw Error("Cannot update game override");
    };
    set(L"windowed",L"true"); set(L"fullscreen",mode==0?L"true":L"false");
    set(L"border",mode==0?L"false":L"true"); set(L"maintas",aspect?L"true":L"false");
    // Preserve the game's reported source ratio; custom aspect_ratio overrides
    // that ratio within maintas rather than applying a second scaling pass.
    set(L"aspect_ratio",L""); set(L"boxing",L"false");
    set(L"width",mode==0?L"0":std::to_wstring(windowSize.width));
    set(L"height",mode==0?L"0":std::to_wstring(windowSize.height));
    // cnc-ddraw positions the client origin, then adds non-client borders.
    const int outerWidth=windowSize.width+frame.right-frame.left;
    const int outerHeight=windowSize.height+frame.bottom-frame.top;
    set(L"posX",mode==0?L"-32000":std::to_wstring(work.left+(work.right-work.left-outerWidth)/2-frame.left));
    set(L"posY",mode==0?L"-32000":std::to_wstring(work.top+(work.bottom-work.top-outerHeight)/2-frame.top));
    set(L"nonexclusive",L"true"); set(L"toggle_borderless",L"true"); set(L"savesettings",L"0");
    set(L"renderer",plan.renderer);
    set(L"shader",plan.shader);
    set(L"d3d9_filter",std::to_wstring(plan.d3d9Filter));
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,temp.c_str());
    auto newDdraw=Read(temp); fs::remove(temp);
    std::string text="[launcher]\r\nmode="+std::to_string(mode)+"\r\nsize="+std::to_string(size)+
        "\r\nfilter="+std::to_string(filter)+"\r\nextra="+std::to_string(extra)+"\r\naspect="+std::to_string(aspect)+"\r\nblockEscape="+std::to_string(block)+"\r\n";
    try {
        AtomicWrite(ddraw,newDdraw);
        AtomicWrite(prefs,Bytes(text.begin(),text.end()));
        AtomicWrite(path,newConfig);
    } catch (...) {
        // Best-effort rollback uses exact snapshots, not defaults.
        try { AtomicWrite(ddraw,originalDdraw); AtomicWrite(path,current);
              if (prefsExisted) AtomicWrite(prefs,originalPrefs); else fs::remove(prefs); } catch (...) {}
        throw;
    }
    loadedConfig=newConfig; dirty=false;
    Status(mode==1 && windowSize.height!=Heights[size]
        ? L"저장 완료 · 창이 화면 안에 들어오도록 크기를 조정했습니다."
        : L"저장 완료 · 원본 설정 백업: LauncherBackup");
}

void BlockEscape(HANDLE process) {
    // Original fixed-base executable: Escape's jump-table entry alone targets DestroyWindow.
    const BYTE before[]={0x0f,0x0e,0x47,0x00}, after[]={0x0f,0x0b,0x47,0x00};
    const BYTE epilogue[]={0x5f,0x5e,0x5d,0x33,0xc0,0x5b,0x83,0xc4,0x40,0xc2,0x10,0x00};
    const BYTE destroy[]={0x55,0xff,0x15,0xc4,0x91,0x49,0x00,0xe9,0xe3,0xfc,0xff,0xff};
    auto verify=[&](uintptr_t addr,const BYTE* expected,size_t len) {
        BYTE actual[16]{}; SIZE_T got{};
        if (!ReadProcessMemory(process,reinterpret_cast<void*>(addr),actual,len,&got) || got!=len || !std::equal(actual,actual+len,expected))
            throw std::runtime_error("Escape patch signature mismatch. Game was not started.");
    };
    verify(0x470e60,before,sizeof(before)); verify(0x470b0f,epilogue,sizeof(epilogue)); verify(0x470e0f,destroy,sizeof(destroy));
    DWORD old{}, ignored{}; SIZE_T done{};
    void* target=reinterpret_cast<void*>(0x470e60);
    if (!VirtualProtectEx(process,target,4,PAGE_EXECUTE_READWRITE,&old)) throw Error("Escape patch protection failed");
    BOOL written=WriteProcessMemory(process,target,after,4,&done);
    BOOL restored=VirtualProtectEx(process,target,4,old,&ignored);
    if (!written || done!=4 || !restored || !FlushInstructionCache(process,target,4)) throw Error("Escape patch failed");
    verify(0x470e60,after,sizeof(after));
}

void LaunchGame() {
    SaveSettings();
    auto exe=gameDir/L"qoh99.exe";
    bool block=SendMessageW(escapeBox,BM_GETCHECK,0,0)==BST_CHECKED;
    if (block && Sha256(Read(exe))!=GameHash)
        throw std::runtime_error("This QOH99.exe version is not supported by the Escape guard. No binary changes were made.");
    STARTUPINFOW si{sizeof(si)}; PROCESS_INFORMATION pi{};
    std::wstring cmd=L"\""+exe.wstring()+L"\"";
    if (!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED,nullptr,gameDir.c_str(),&si,&pi)) throw Error("Cannot start QOH99");
    try {
        if (block) BlockEscape(pi.hProcess);
        if (ResumeThread(pi.hThread)==static_cast<DWORD>(-1)) throw Error("Cannot resume QOH99");
    } catch (...) {
        TerminateProcess(pi.hProcess,1); // Only our newly-created, still-suspended child.
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess); throw;
    }
    CloseHandle(pi.hThread); gameProcess=pi.hProcess; gamePid=pi.dwProcessId;
    SetTimer(mainWindow,1,500,nullptr);
    Status(block?L"게임 실행 중 · Esc 즉시 종료 차단 · 종료는 Alt+F4":L"게임 실행 중 · Esc 차단 꺼짐 · 종료는 Alt+F4");
    for(int id=IdMode;id<IdKeys+16;id++) if(HWND c=GetDlgItem(mainWindow,id)) EnableWindow(c,FALSE);
}

HWND Control(const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id=0) {
    int offset=y>=274?64:24;
    HWND hwnd=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,S(x),S(y+offset),S(w),S(h),mainWindow,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(bodyFont),TRUE); return hwnd;
}
HWND Label(const wchar_t* text,int x,int y,int w,int h) { return Control(L"STATIC",text,SS_LEFT,x,y,w,h); }
HWND Button(const wchar_t* text,int x,int y,int w,int h,int id) { return Control(L"BUTTON",text,BS_PUSHBUTTON|WS_TABSTOP,x,y,w,h,id); }
void InitUI() {
    Label(L"Thread-@tikiland.t",28,-12,740,24);
    auto title=Label(L"QOH99  ·  게임 런처",26,18,740,36);
    SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
    Label(L"화면과 조작을 설정하고 원본 게임을 시작하세요.",28,59,720,24);
    Control(L"BUTTON",L"화면",BS_GROUPBOX,24,96,752,204);
    Label(L"화면 모드",42,125,90,24);
    modeBox=Control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,140,122,265,160,IdMode);
    for(auto text:{L"보더리스 전체 화면",L"창 모드"}) SendMessageW(modeBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));
    Label(L"창 크기",430,125,80,24);
    sizeBox=Control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,516,122,237,200,IdSize);
    for(auto text:Sizes) SendMessageW(sizeBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));
    Label(L"화면 필터",42,170,90,24);
    filterBox=Control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,140,167,265,210,IdFilter);
    for(auto text:FilterNames) SendMessageW(filterBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));
    aspectBox=Control(L"BUTTON",L"원래 4:3 비율 유지",BS_AUTOCHECKBOX|WS_TABSTOP,430,168,295,28,IdAspect);
    Label(L"추가 효과",42,210,90,24);
    extraBox=Control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,140,207,265,150,IdExtra);
    for(auto text:{L"없음",L"주사선 (Scanlines)",L"선명도 보정 (RCAS)"}) SendMessageW(extraBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));
    Label(L"기본 필터 → 추가 효과 순서로 적용",430,210,314,24);
    tipLabel=Label(L"",42,253,710,32);
    Control(L"BUTTON",L"키 설정 · 버튼을 누른 뒤 원하는 키를 입력하세요",BS_GROUPBOX,24,274,752,294);
    Label(L"1P",44,300,300,24); Label(L"2P",420,300,300,24);
    for(int p=0;p<2;p++) for(int a=0;a<8;a++) {
        int col=a/4,row=a%4,x=42+p*376+col*172,y=336+row*47;
        Label(Actions[a],x,y+6,77,25);
        keyButtons[p][a]=Button(L"",x+78,y,86,34,IdKeys+p*8+a);
    }
    Button(L"추천 키로",42,533,128,27,IdReset);
    Label(L"방향키 + Z/X/C/V  ·  I/J/K/L + A/S/D/F",186,536,530,24);
    escapeBox=Control(L"BUTTON",L"Esc를 눌러도 게임이 바로 종료되지 않게 하기",BS_AUTOCHECKBOX|WS_TABSTOP,30,586,725,28,IdEsc);
    Label(L"Alt+Enter: 화면 모드 전환   ·   Alt+F4: 게임 종료",32,619,718,24);
    statusLabel=Label(L"",30,660,740,46);
    Button(L"다시 불러오기",26,717,128,36,IdReload);
    Button(L"원본 설정 도구",166,717,137,36,IdOriginal);
    Button(L"저장",315,717,80,36,IdSave);
    Button(L"게임으로 미리보기",407,717,166,36,IdPreview);
    Button(L"게임 시작",589,713,187,44,IdPlay);
    Button(L"문제 해결: 필터 없이 실행",26,773,278,32,IdSafe);
    Label(L"선택한 창 크기와 키 설정은 유지합니다.",323,779,445,24);
    LoadSettings();
}

LRESULT CALLBACK WndProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_CREATE:
        mainWindow=w;
        try { InitUI(); } catch(const std::exception& e) { Report(e); Status(L"설정 읽기 실패 · 원본 게임 폴더를 확인하세요."); }
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode(reinterpret_cast<HDC>(wp),TRANSPARENT); SetTextColor(reinterpret_cast<HDC>(wp),RGB(32,43,63)); return reinterpret_cast<LRESULT>(bgBrush);
    case WM_CTLCOLORBTN: return reinterpret_cast<LRESULT>(bgBrush);
    case WM_COMMAND:
        try {
            int id=LOWORD(wp), notify=HIWORD(wp);
            if(id>=IdKeys && id<IdKeys+16 && !gameProcess && initialized) {
                capturePlayer=(id-IdKeys)/8; captureAction=(id-IdKeys)%8; RefreshKeys();
                Status(L"지정할 키를 누르세요. Esc는 입력 취소입니다."); return 0;
            }
            if((id==IdMode || id==IdFilter || id==IdSize || id==IdExtra) && notify==CBN_SELCHANGE) { CancelCapture(); RefreshDisplay(); Changed(); }
            if(id==IdAspect || id==IdEsc) { CancelCapture(); Changed(); }
            if(id==IdReload) LoadSettings();
            if(id==IdReset && initialized) {
                const uint32_t defaults[2][8]={{0xc8,0xd0,0xcb,0xcd,0x2c,0x2d,0x2e,0x2f},{0x17,0x25,0x24,0x26,0x1e,0x1f,0x20,0x21}};
                for(int p=0;p<2;p++) for(int a=0;a<8;a++) keys[p][a]={0,defaults[p][a]};
                CancelCapture(); Changed();
            }
            if(id==IdSave) { CancelCapture(); SaveSettings(); }
            if(id==IdPlay || id==IdPreview) { CancelCapture(); LaunchGame(); }
            if(id==IdSafe) {
                CancelCapture();
                SendMessageW(filterBox,CB_SETCURSEL,0,0);
                SendMessageW(extraBox,CB_SETCURSEL,0,0);
                RefreshDisplay(); Changed(); LaunchGame();
            }
            if(id==IdOriginal) {
                if(GameRunning()) throw std::runtime_error("Close QOH99 before opening the original configuration tool.");
                if(dirty && initialized) {
                    int answer=MessageBoxW(w,L"원본 도구를 열기 전에 런처 설정을 저장할까요?",L"키 설정",MB_YESNOCANCEL|MB_ICONQUESTION);
                    if(answer==IDCANCEL) return 0;
                    if(answer==IDYES) SaveSettings();
                }
                auto exe=gameDir/L"Config.exe";
                if(reinterpret_cast<INT_PTR>(ShellExecuteW(w,L"open",exe.c_str(),nullptr,gameDir.c_str(),SW_SHOWNORMAL))<=32) throw Error("Cannot open Config.exe");
                Status(L"원본 설정 도구를 닫은 후 '다시 불러오기'를 누르세요.");
            }
            if(id==IdYouTube || id==IdThreads) {
                auto url=id==IdYouTube?YouTubeUrl:ThreadsUrl;
                if(reinterpret_cast<INT_PTR>(ShellExecuteW(w,L"open",url,nullptr,nullptr,SW_SHOWNORMAL))<=32) throw Error("Cannot open browser link");
            }
        } catch(const std::exception& e) { Report(e); }
        return 0;
    case WM_TIMER:
        if(gameProcess && WaitForSingleObject(gameProcess,0)==WAIT_OBJECT_0) {
            CloseHandle(gameProcess); gameProcess=nullptr; gamePid=0; KillTimer(w,1);
            for(int id=IdMode;id<IdKeys+16;id++) if(HWND c=GetDlgItem(w,id)) EnableWindow(c,TRUE);
            try { LoadSettings(); Status(L"게임이 종료되었습니다. 설정을 바꾸거나 다시 시작할 수 있습니다."); } catch(const std::exception& e) { Report(e); }
        }
        return 0;
    case WM_CLOSE:
        if(dirty) {
            int answer=MessageBoxW(w,L"변경한 설정을 저장하고 닫을까요?",L"QOH 런처",MB_YESNOCANCEL|MB_ICONQUESTION);
            if(answer==IDCANCEL) return 0;
            if(answer==IDYES) { try { SaveSettings(); } catch(const std::exception& e) { Report(e); return 0; } }
        }
        DestroyWindow(w); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    wchar_t path[32768]{}; GetModuleFileNameW(nullptr,path,32768);
    gameDir=fs::path(path).parent_path();
    if(!fs::is_regular_file(gameDir/L"qoh99.exe")) {
        MessageBoxW(nullptr,L"QOH-Launcher.exe를 qoh99.exe와 같은 폴더에 놓으세요.",L"QOH 런처",MB_OK|MB_ICONERROR); return 1;
    }
    HANDLE single=CreateMutexW(nullptr,FALSE,L"Local\\QOH99NativeLauncher");
    if(GetLastError()==ERROR_ALREADY_EXISTS) {
        if(HWND existing=FindWindowW(ClassName,nullptr)) { ShowWindow(existing,SW_RESTORE); SetForegroundWindow(existing); }
        if(single) CloseHandle(single); return 0;
    }
    try { qoh_runtime::Prepare(gameDir,GameRunning()); }
    catch(const std::exception& error) {
        MessageBoxW(nullptr,Widen(error.what()).c_str(),L"QOH 런처 - 실행 파일 준비 실패",MB_OK|MB_ICONERROR);
        if(single) CloseHandle(single);
        return 1;
    }
    HDC screen=GetDC(nullptr); dpi=GetDeviceCaps(screen,LOGPIXELSX); ReleaseDC(nullptr,screen);
    bodyFont=CreateFontW(-S(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"맑은 고딕");
    titleFont=CreateFontW(-S(25),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"맑은 고딕");
    bgBrush=CreateSolidBrush(RGB(247,249,252));
    INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_STANDARD_CLASSES}; InitCommonControlsEx(&ic);
    WNDCLASSW wc{}; wc.lpfnWndProc=WndProc; wc.hInstance=instance; wc.lpszClassName=ClassName;
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));
    if(!wc.hIcon) wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    wc.hbrBackground=bgBrush; RegisterClassW(&wc);
    HMENU menu=CreateMenu(), about=CreatePopupMenu();
    AppendMenuW(about,MF_STRING,IdYouTube,YouTubeUrl);
    AppendMenuW(about,MF_STRING,IdThreads,ThreadsUrl);
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(about),L"About");
    RECT rect{0,0,S(800),S(885)}; DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    AdjustWindowRect(&rect,style,TRUE);
    HWND hwnd=CreateWindowExW(WS_EX_CONTROLPARENT,ClassName,L"QOH99 Launcher 0.2.4",style,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,menu,instance,nullptr);
    if(!hwnd) return 1;
    ShowWindow(hwnd,show); UpdateWindow(hwnd);
    MSG m{};
    while(GetMessageW(&m,nullptr,0,0)>0) {
        if(capturePlayer>=0 && (m.message==WM_KEYDOWN || m.message==WM_SYSKEYDOWN)) {
            if(m.wParam==VK_ESCAPE) { CancelCapture(); Status(L"키 지정을 취소했습니다."); }
            else if(m.wParam==VK_LWIN || m.wParam==VK_RWIN || m.wParam==VK_MENU || m.wParam==VK_F4 || m.wParam==VK_PAUSE) {
                Status(L"Windows / Alt / F4 / Pause는 시스템 조작용입니다. 다른 키를 선택하세요.");
            } else {
                uint32_t code=(static_cast<uint32_t>(m.lParam)>>16)&0xff;
                if(m.lParam&(1<<24)) code|=0x80;
                if(code) { keys[capturePlayer][captureAction]={0,code}; CancelCapture(); Changed(); }
            }
            continue;
        }
        if(!IsDialogMessageW(hwnd,&m)) { TranslateMessage(&m); DispatchMessageW(&m); }
    }
    if(gameProcess) CloseHandle(gameProcess);
    if(single) CloseHandle(single);
    DeleteObject(bodyFont); DeleteObject(titleFont); DeleteObject(bgBrush);
    return 0;
}
