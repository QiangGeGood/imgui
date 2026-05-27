#pragma once

#include "UiWindow.h"

class HelloWindow : public UiWindow
{
public:
    void Draw() override;

private:
    int counter_ = 0;
};