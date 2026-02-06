#pragma once

#include "app.hpp"
#include "../wm/wm.hpp"

namespace sertos::apps {

using wm::Rect;

class AboutApp : public App {
public:
    AboutApp();
    ~AboutApp() override = default;
    
    void render() override;
    void handleKeyPress(input::KeyCode code, u8 ascii, bool ctrl, bool alt, bool shift) override;
    void handleMouseClick(i32 x, i32 y, bool doubleClick) override;

private:
    void drawLogo(i32 x, i32 y, const Rect& clip);
    void drawSystemInfo(i32 x, i32 y, const Rect& clip);
};

}
