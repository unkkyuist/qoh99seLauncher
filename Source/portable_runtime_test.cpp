#include <windows.h>
#include "portable_runtime.h"
#include "runtime_payload.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <string>

namespace fs=std::filesystem;
void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void Put(const fs::path& path,const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path,std::ios::binary); file<<text;
    Require(bool(file),"Fixture write failed");
}
std::string Read(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);
    Require(bool(file),"Read failed");
    return {std::istreambuf_iterator<char>(file),{}};
}
template<class F> void Reject(F action) {
    bool threw=false;
    try { action(); } catch(const std::exception&) { threw=true; }
    Require(threw,"Expected rejection");
}
int main() {
    // Keep evidence under the test working directory; never touch a real game.
    const auto root=fs::current_path()/("portable-test-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    try {
        const auto fresh=root/L"fresh";
        Put(fresh/L"qoh99.exe","original-game-sentinel");
        Put(fresh/L"System/QOHcnf.key","personal-key-sentinel");
        const auto count=qoh_runtime::Prepare(fresh,false);
        Require(count==std::size(kPayload),"Missing extracted files");
        for(const auto& payload:kPayload) {
            const auto resource=FindResourceW(nullptr,MAKEINTRESOURCEW(payload.id),RT_RCDATA);
            const auto data=static_cast<const char*>(LockResource(LoadResource(nullptr,resource)));
            Require(Read(fresh/payload.path)==std::string(data,SizeofResource(nullptr,resource)),"Payload differs");
        }
        Require(Read(fresh/L"qoh99.exe")=="original-game-sentinel","Game overwritten");
        Require(Read(fresh/L"System/QOHcnf.key")=="personal-key-sentinel","Keys overwritten");
        Require(qoh_runtime::Prepare(fresh,true)==0,"Unchanged runtime should work while running");
        Put(fresh/L"ddraw.ini","custom-ini");
        Put(fresh/L"QOH-Launcher.ini","custom-launcher-ini");
        Put(fresh/L"ddraw.dll","old-wrapper");
        Reject([&]{qoh_runtime::Prepare(fresh,true);});
        Require(Read(fresh/L"ddraw.dll")=="old-wrapper","Running game was modified");
        Require(!fs::exists(fresh/L"LauncherBackup"),"Running game created backup");
        Require(qoh_runtime::Prepare(fresh,false)==1,"Expected one upgrade");
        Require(Read(fresh/L"ddraw.ini")=="custom-ini","INI overwritten");
        Require(Read(fresh/L"QOH-Launcher.ini")=="custom-launcher-ini","Launcher INI overwritten");
        bool backed=false;
        for(const auto& entry:fs::recursive_directory_iterator(fresh/L"LauncherBackup"))
            if(entry.path().filename()==L"ddraw.dll") backed=Read(entry.path())=="old-wrapper";
        Require(backed,"Missing original DLL backup");
        Require(qoh_runtime::Prepare(fresh,false)==0,"Upgrade not idempotent");

        // A locked shader forces rollback of the successfully replaced DLL.
        const auto locked=root/L"locked";
        Put(locked/L"ddraw.dll","old-wrapper");
        Put(locked/L"ddraw.ini","custom-ini");
        fs::create_directories(locked/L"LauncherShaders");
        const auto shader=locked/L"LauncherShaders/nearest-neighbor.glsl";
        Put(shader,"old-shader");
        HANDLE handle=CreateFileW(shader.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        Require(handle!=INVALID_HANDLE_VALUE,"Could not lock shader");
        Reject([&]{qoh_runtime::Prepare(locked,false);});
        CloseHandle(handle);
        Require(Read(locked/L"ddraw.dll")=="old-wrapper","DLL rollback failed");
        Require(Read(shader)=="old-shader","Locked shader changed");
        Require(!fs::exists(locked/L"ddraw.dll.portable-tmp"),"Temporary file leaked");
        const auto collision=root/L"collision";
        fs::create_directories(collision/L"ddraw.ini");
        Reject([&]{qoh_runtime::Prepare(collision,false);});
        Require(!fs::exists(collision/L"ddraw.dll"),"Preflight partially installed");
        std::cout<<"Portable runtime: "<<count<<" resources; fresh install, preservation, backup, running guard, rollback and collision passed. Evidence: "<<root.u8string()<<'\n';
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<"\nEvidence: "<<root.u8string()<<'\n'; return 1; }
}
