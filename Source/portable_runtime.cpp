#include <windows.h>
#include "portable_runtime.h"
#include "runtime_payload.h"
#include <fstream>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <utility>
#include <string>

namespace fs=std::filesystem;
namespace {
using Bytes=std::vector<unsigned char>;
Bytes Read(const fs::path& path) {
    std::ifstream stream(path,std::ios::binary);
    if(!stream) throw std::runtime_error("Cannot read runtime file: "+path.u8string());
    return Bytes(std::istreambuf_iterator<char>(stream),{});
}
void Write(const fs::path& path,const Bytes& bytes) {
    fs::create_directories(path.parent_path());
    auto temp=path; temp+=L".portable-tmp";
    try {
        {
            std::ofstream stream(temp,std::ios::binary|std::ios::trunc);
            if(!stream || !stream.write(reinterpret_cast<const char*>(bytes.data()),bytes.size()) || !stream.flush())
                throw std::runtime_error("Cannot prepare runtime file: "+path.u8string());
        }
        if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace runtime file. Close the game and try again: "+path.u8string());
    } catch(...) {
        std::error_code ec; fs::remove(temp,ec);
        throw;
    }
}
Bytes Resource(int id) {
    auto module=GetModuleHandleW(nullptr);
    auto resource=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);
    auto size=resource?SizeofResource(module,resource):0;
    auto loaded=resource?LoadResource(module,resource):nullptr;
    auto data=loaded?static_cast<const unsigned char*>(LockResource(loaded)):nullptr;
    if(!data || !size) throw std::runtime_error("Embedded runtime is missing. Download the complete launcher EXE again.");
    return Bytes(data,data+size);
}
struct Update { fs::path path, relative; Bytes bytes, original; bool existed; };
}

std::size_t qoh_runtime::Prepare(const fs::path& gameDir,bool gameRunning) {
    std::vector<Update> updates;
    for(const auto& file:kPayload) {
        const auto path=gameDir/file.path;
        const bool exists=fs::exists(path);
        if(exists && !fs::is_regular_file(path))
            throw std::runtime_error("A runtime file path is occupied by a directory: "+path.u8string());
        if(exists && file.preserve) continue;
        auto bytes=Resource(file.id);
        auto original=exists?Read(path):Bytes{};
        if(exists && original==bytes) continue;
        updates.push_back({path,file.path,std::move(bytes),std::move(original),exists});
    }
    if(updates.empty()) return 0;
    if(gameRunning) throw std::runtime_error("Close QOH99 and Config.exe before preparing the portable runtime.");

    fs::path backup;
    for(const auto& update:updates) if(update.existed) {
        if(backup.empty()) {
            const auto stamp=std::chrono::system_clock::now().time_since_epoch().count();
            backup=gameDir/L"LauncherBackup"/(L"portable-"+std::to_wstring(stamp));
            fs::create_directories(backup);
        }
        Write(backup/update.relative,update.original);
    }
    std::size_t installed=0;
    try {
        for(const auto& update:updates) { Write(update.path,update.bytes); ++installed; }
    } catch(const std::exception& error) {
        bool restored=true;
        while(installed) {
            const auto& update=updates[--installed];
            try {
                if(update.existed) Write(update.path,update.original);
                else fs::remove(update.path);
            } catch(...) { restored=false; }
        }
        throw std::runtime_error(std::string(error.what())+(restored?" Previous runtime files were restored.":
            " Restore the runtime files from LauncherBackup before retrying."));
    }
    return updates.size();
}
