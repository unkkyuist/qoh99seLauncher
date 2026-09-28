#pragma once
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>

namespace qoh_target {
// This is an executable-family check, not a version whitelist. In particular,
// do not accept the XP self-extracting installer as the actual game.
inline void ValidateImage(const std::vector<std::uint8_t>& bytes) {
    auto u16=[&](std::size_t at) { return unsigned(bytes.at(at)) | (unsigned(bytes.at(at+1))<<8); };
    auto u32=[&](std::size_t at) { return std::uint32_t(u16(at)) | (std::uint32_t(u16(at+2))<<16); };
    if(bytes.size()<64 || u16(0)!=0x5a4d) throw std::runtime_error("Select the extracted QOH game EXE, not an archive or installer.");
    const std::size_t pe=u32(0x3c);
    if(pe>bytes.size()-26 || u32(pe)!=0x4550 || u16(pe+4)!=0x14c ||
       (u16(pe+22)&0x2000) || u16(pe+24)!=0x10b)
        throw std::runtime_error("Select a 32-bit QOH game executable.");
    std::string text(bytes.begin(),bytes.end());
    for(auto& ch:text) if(ch>='A' && ch<='Z') ch=char(ch-'A'+'a');
    if(text.find("ddraw.dll")==std::string::npos || text.find("qohcnf.key")==std::string::npos)
        throw std::runtime_error("This is not a recognized DirectDraw QOH executable. Select qoh99.exe from the extracted game folder.");
}
inline std::filesystem::path KeyPath(const std::filesystem::path& directory) {
    auto local=directory/L"LocalConfig/QOHcnf.key";
    return std::filesystem::exists(local)?local:directory/L"System/QOHcnf.key";
}
}
