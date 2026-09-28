#include "game_target.h"
#include "native_keys.h"
#include <fstream>
#include <iostream>
#include <chrono>

namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
void Require(bool value,const char* text) { if(!value) throw std::runtime_error(text); }
template<class F> void Reject(F action) {
    bool rejected=false;
    try { action(); } catch(const std::exception&) { rejected=true; }
    Require(rejected,"Invalid target was accepted");
}
Bytes Read(const fs::path& path) {
    std::ifstream stream(path,std::ios::binary);
    Require(bool(stream),"Cannot read test input");
    return {std::istreambuf_iterator<char>(stream),{}};
}
int wmain(int argc,wchar_t** argv) {
    try {
        Bytes exe(512);
        exe[0]='M'; exe[1]='Z'; exe[0x3c]=0x80;
        exe[0x80]='P'; exe[0x81]='E'; exe[0x84]=0x4c; exe[0x85]=1;
        exe[0x98]=0xb; exe[0x99]=1;
        const std::string markers="DDRAW.dll / QOHcnf.key";
        std::copy(markers.begin(),markers.end(),exe.begin()+256);
        qoh_target::ValidateImage(exe);
        auto otherVersion=exe; otherVersion[400]=42;
        qoh_target::ValidateImage(otherVersion);
        Reject([]{qoh_target::ValidateImage({});});
        auto bad=exe; bad[0x3f]=0xff;
        Reject([&]{qoh_target::ValidateImage(bad);});
        bad=exe; bad[0x85]=0x86;
        Reject([&]{qoh_target::ValidateImage(bad);});
        bad=exe; bad[0x97]=0x20;
        Reject([&]{qoh_target::ValidateImage(bad);});
        bad=exe; std::fill(bad.begin()+256,bad.end(),std::uint8_t{0});
        Reject([&]{qoh_target::ValidateImage(bad);});
        const auto root=fs::current_path()/("target-test-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
        fs::create_directories(root/L"System");
        std::ofstream(root/L"QOHcnf.key")<<"unrelated root copy";
        std::ofstream(root/L"System/QOHcnf.key")<<"system";
        Require(qoh_target::KeyPath(root)==root/L"System/QOHcnf.key","Root copy overrode the game setting");
        fs::create_directories(root/L"LocalConfig");
        std::ofstream(root/L"LocalConfig/QOHcnf.key")<<"local";
        Require(qoh_target::KeyPath(root)==root/L"LocalConfig/QOHcnf.key","LocalConfig precedence changed");
        for(int i=1;i<argc;i++) {
            const fs::path path=argv[i];
            qoh_target::ValidateImage(Read(path));
            auto config=Read(qoh_target::KeyPath(path.parent_path()));
            qoh_keys::DecodeKeys(config);
            qoh_keys::EnableNativeFullscreen(config);
            std::cout<<"Accepted actual game and settings without modifying them: "<<path.u8string()<<'\n';
        }
        std::cout<<"PASS: different game images, invalid EXE rejection and configuration paths\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
