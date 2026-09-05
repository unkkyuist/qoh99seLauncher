#include "filter_plan.h"
#include <iostream>
#include <set>
int main() {
    try {
        auto check=[](bool value) { if(!value) throw std::runtime_error("Filter plan mismatch"); };
        std::set<std::wstring> recipes;
        for(int base:{0,1,2,4}) for(int extra:{1,2}) {
            auto p=qoh_filters::MakePlan(base,extra);
            check(p.renderer==L"opengl" && p.secondPass);
            check((p.shader.find(L"bilinear.glsl")!=std::wstring::npos)==(base==1 || base==4));
            check(p.shader.find(L"-pass1scale")==std::wstring::npos);
            recipes.insert(p.shader);
        }
        check(recipes.size()==8);
        auto xbrz=qoh_filters::MakePlan(3,0); check(xbrz.secondPass);
        check(xbrz.shader==L"LauncherShaders\\xbrz\\xbrz-freescale-multipass.glsl");
        check(qoh_filters::MakePlan(0,0).shader==L"Nearest neighbor");
        check(qoh_filters::MakePlan(1,0).d3d9Filter==1);
        for(auto pair:{std::pair{3,1},std::pair{3,2},std::pair{-1,0},std::pair{5,0},std::pair{0,3}}) {
            bool rejected=false;
            try { qoh_filters::MakePlan(pair.first,pair.second); } catch(const std::runtime_error&) { rejected=true; }
            check(rejected);
        }
        std::cout<<"PASS: eight filter combinations, bilinear routing, xBRZ two-pass guard\n";
        return 0;
    } catch(const std::exception& ex) { std::cerr<<ex.what()<<'\n'; return 1; }
}
