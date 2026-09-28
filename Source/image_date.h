#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <string>
#include <stdexcept>

namespace qoh_compat {
// Caller must verify both the XP executable hash and the original image hash.
inline bool RestoreImageDate(const std::filesystem::path& file,const std::filesystem::path& backup) {
    namespace fs=std::filesystem;
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if(!GetFileAttributesExW(file.c_str(),GetFileExInfoStandard,&info)) throw std::runtime_error("Cannot read image date");
    const FILETIME expected{0xda3e1b00,0x01bfa6c3};
    if(CompareFileTime(&info.ftLastWriteTime,&expected)==0) return false;
    fs::create_directories(backup);
    if(!fs::exists(backup/L"Tir_Mes1.Img")) fs::copy_file(file,backup/L"Tir_Mes1.Img");
    const auto record=backup/L"Tir_Mes1.last-write-filetime.txt";
    if(!fs::exists(record)) {
        const auto previous=(std::uint64_t(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime;
        auto temp=record; temp+=L".tmp";
        { std::ofstream stream(temp,std::ios::binary);
          stream<<previous<<"\r\n";
          if(!stream.flush()) throw std::runtime_error("Cannot back up original image date"); }
        if(!MoveFileExW(temp.c_str(),record.c_str(),MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot save original image date backup");
    }
    HANDLE handle=CreateFileW(file.c_str(),FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(handle==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot restore original image date");
    const BOOL restored=SetFileTime(handle,nullptr,nullptr,&expected);
    CloseHandle(handle);
    if(!restored) throw std::runtime_error("Cannot restore original image date");
    return true;
}
}
