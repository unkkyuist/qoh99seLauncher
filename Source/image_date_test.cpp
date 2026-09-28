#include "image_date.h"
#include <iostream>
#include <chrono>

namespace fs=std::filesystem;
void Require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
std::string Read(const fs::path& path) {
    std::ifstream stream(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),{}};
}
int main() {
    try {
        const auto root=fs::current_path()/("image-date-test-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
        fs::create_directories(root);
        const auto file=root/L"Tir_Mes1.Img",backup=root/L"backup";
        { std::ofstream stream(file,std::ios::binary); stream<<"unchanged image content"; }
        WIN32_FILE_ATTRIBUTE_DATA before{},after{};
        Require(GetFileAttributesExW(file.c_str(),GetFileExInfoStandard,&before)!=FALSE,"Initial timestamp failed");
        Require(qoh_compat::RestoreImageDate(file,backup),"Expected correction");
        Require(GetFileAttributesExW(file.c_str(),GetFileExInfoStandard,&after)!=FALSE,"Final timestamp failed");
        Require(after.ftLastWriteTime.dwLowDateTime==0xda3e1b00 && after.ftLastWriteTime.dwHighDateTime==0x01bfa6c3,"Wrong restored date");
        Require(Read(file)=="unchanged image content" && Read(backup/L"Tir_Mes1.Img")==Read(file),"Image content changed");
        const auto stamp=(std::uint64_t(before.ftLastWriteTime.dwHighDateTime)<<32)|before.ftLastWriteTime.dwLowDateTime;
        const auto record=Read(backup/L"Tir_Mes1.last-write-filetime.txt");
        Require(record==std::to_string(stamp)+"\r\n","Original timestamp not recorded");
        Require(!qoh_compat::RestoreImageDate(file,backup),"Second run changed the date");
        Require(Read(backup/L"Tir_Mes1.last-write-filetime.txt")==record,"Backup changed");
        const auto blocked=root/L"blocked-backup";
        { std::ofstream stream(blocked); stream<<"block directory creation"; }
        const auto untouched=root/L"untouched.Img";
        { std::ofstream stream(untouched); stream<<"untouched"; }
        Require(GetFileAttributesExW(untouched.c_str(),GetFileExInfoStandard,&before)!=FALSE,"Timestamp failed");
        bool rejected=false;
        try { qoh_compat::RestoreImageDate(untouched,blocked); } catch(const std::exception&) { rejected=true; }
        Require(rejected,"Backup failure was ignored");
        GetFileAttributesExW(untouched.c_str(),GetFileExInfoStandard,&after);
        Require(CompareFileTime(&before.ftLastWriteTime,&after.ftLastWriteTime)==0,"Changed date despite backup failure");
        std::cout<<"PASS: image bytes preserved, original date backed up, correction idempotent, backup failure leaves original date\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
