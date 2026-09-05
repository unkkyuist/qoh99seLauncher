#pragma once
#include <string>
#include <stdexcept>

namespace qoh_filters {
struct Plan { std::wstring renderer, shader; int d3d9Filter; bool secondPass; };
inline Plan MakePlan(int base,int extra) {
    if(base<0 || base>4 || extra<0 || extra>2) throw std::runtime_error("Invalid filter selection.");
    if(base==3 && extra) throw std::runtime_error("xBRZ already uses both shader passes. Select another base filter for an extra effect.");
    if(extra) {
        auto name=L"LauncherShaders\\recipes\\base"+std::to_wstring(base)+L"-extra"+std::to_wstring(extra);
        if(base==1 || base==4) name+=L"-bilinear";
        return {L"opengl",name+L".glsl",0,true};
    }
    switch(base) {
    case 0: return {L"auto",L"Nearest neighbor",0,false};
    case 1: return {L"auto",L"Bilinear",1,false};
    case 2: return {L"opengl",L"LauncherShaders\\xbr\\xbr-lv2-noblend.glsl",0,false};
    case 3: return {L"opengl",L"LauncherShaders\\xbrz\\xbrz-freescale-multipass.glsl",0,true};
    default: return {L"opengl",L"LauncherShaders\\crt\\crt-lottes-fast-no-warp-bilinear.glsl",0,false};
    }
}
}
