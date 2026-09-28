#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace qoh_exe {
using Bytes = std::vector<std::uint8_t>;
// Only used after the caller verifies the complete canonical EXE SHA-256.
inline constexpr std::size_t Hook = 0x719a2, Cave = 0x98900, TextSize = 0x1e8, Escape = 0x70e60;
inline const Bytes OriginalHook{0xa1,0xe0,0x3d,0x61,0x00,0x89,0x58,0x50};
inline Bytes Jump(std::size_t from, std::size_t to) {
    const auto displacement=static_cast<std::uint32_t>(to-from-5);
    return {0xe9,static_cast<std::uint8_t>(displacement),static_cast<std::uint8_t>(displacement>>8),
        static_cast<std::uint8_t>(displacement>>16),static_cast<std::uint8_t>(displacement>>24)};
}
inline Bytes HookBytes() { auto b=Jump(Hook,Cave); b.resize(8,0x90); return b; }
inline Bytes CaveBytes() {
    auto b=OriginalHook;
    // Keep the original +0x50 initialization, then normalize the loaded profile.
    const Bytes fullscreen{0xc7,0x40,0x6c,0x01,0x00,0x00,0x00};
    b.insert(b.end(),fullscreen.begin(),fullscreen.end());
    auto back=Jump(Cave+b.size(),Hook+OriginalHook.size());
    b.insert(b.end(),back.begin(),back.end()); return b;
}
inline bool Match(const Bytes& b,std::size_t at,const Bytes& value) {
    return at<=b.size() && value.size()<=b.size()-at && std::equal(value.begin(),value.end(),b.begin()+at);
}
inline void Put(Bytes& b,std::size_t at,const Bytes& value) { std::copy(value.begin(),value.end(),b.begin()+at); }
inline Bytes SizeBytes(bool xp) { return {static_cast<std::uint8_t>(xp?0x95:0x75),0x78,0x09,0}; }
inline Bytes Canonical(const Bytes& image,bool xp) {
    auto b=image;
    const Bytes originalEscape{0x0f,0x0e,0x47,0},patchedEscape{0x0f,0x0b,0x47,0};
    const Bytes expandedSize{0x14,0x79,0x09,0};
    if(Match(b,Hook,HookBytes()) && Match(b,Cave,CaveBytes()) && Match(b,TextSize,expandedSize)) {
        Put(b,Hook,OriginalHook); Put(b,Cave,Bytes(20,0)); Put(b,TextSize,SizeBytes(xp));
    }
    if(Match(b,Escape,patchedEscape)) Put(b,Escape,originalEscape);
    return b;
}
inline Bytes Apply(const Bytes& canonical,bool xp,bool blockEscape) {
    if(!Match(canonical,Hook,OriginalHook) || !Match(canonical,Cave,Bytes(20,0)) ||
       !Match(canonical,TextSize,SizeBytes(xp)) || !Match(canonical,Escape,{0x0f,0x0e,0x47,0}) ||
       !Match(canonical,0x70b0f,{0x5f,0x5e,0x5d,0x33,0xc0,0x5b,0x83,0xc4,0x40,0xc2,0x10,0}) ||
       !Match(canonical,0x70e0f,{0x55,0xff,0x15,0xc4,0x91,0x49,0,0xe9,0xe3,0xfc,0xff,0xff}))
        throw std::runtime_error("Game compatibility signature mismatch. EXE was not changed.");
    auto b=canonical;
    Put(b,Hook,HookBytes()); Put(b,Cave,CaveBytes()); Put(b,TextSize,{0x14,0x79,0x09,0});
    if(blockEscape) Put(b,Escape,{0x0f,0x0b,0x47,0});
    return b;
}
}
