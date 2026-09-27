#include "display_plan.h"
#include <iostream>

int main() {
    try {
        const qoh_display::Size presets[]={{640,480},{960,720},{1280,960},{1440,1080},{1600,1200}};
        // Client budgets after subtracting taskbar + frame, including large DPI borders.
        const qoh_display::Size budgets[]={{1904,1001},{1888,962},{1350,689},{2544,1361}};
        for(auto requested:presets) for(auto available:budgets) {
            const auto actual=qoh_display::FitWindow(requested,available);
            if(actual.width>available.width || actual.height>available.height ||
               actual.width>requested.width || actual.height>requested.height ||
               actual.width*3!=actual.height*4)
                throw std::runtime_error("Window exceeds client budget or distorts 4:3.");
            if(requested.width<=available.width && requested.height<=available.height &&
               (actual.width!=requested.width || actual.height!=requested.height))
                throw std::runtime_error("A fitting window was unnecessarily resized.");
        }
        bool rejected=false;
        try { qoh_display::FitWindow({640,480},{639,479}); }
        catch(const std::runtime_error&) { rejected=true; }
        if(!rejected) throw std::runtime_error("Cannot downscale below the wrapper's native window minimum.");
        std::cout<<"PASS: all presets fit desktop budgets without stretching or enlargement\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
