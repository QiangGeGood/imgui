// UiWindow: abstract base for self-contained ImGui windows.
// Subclasses encapsulate ImGui::Begin/End plus their own state.

#pragma once

class UiWindow
{
public:
    virtual ~UiWindow() = default;

    // Called once per frame between ImGui::NewFrame() and ImGui::Render().
    virtual void Draw() = 0;

    // Pointer to the open flag, suitable for passing to ImGui::Begin(name, p_open).
    bool* OpenFlag() { return &open_; }
    bool  IsOpen() const { return open_; }

protected:
    bool open_ = true;
};