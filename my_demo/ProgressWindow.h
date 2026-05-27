#pragma once

#include "UiWindow.h"

// Animated progress bar window. Useful for visually demonstrating how the
// window appears/disappears when its owning shared_ptr is created/released.
class ProgressWindow : public UiWindow
{
public:
    void Draw() override;

private:
    float progress_ = 0.0f;  // 0..1
    float speed_    = 0.3f;  // units per second
    bool  running_  = true;
};