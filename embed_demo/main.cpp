// Minimal embedding example.
//
// Scenario: an existing program has its OWN main loop (here simulated by
// a console loop that mutates shared state). We embed ImGuiDx11App to
// visualize that state live, touching the original loop with just a few
// lines: declare the app, register a window, call Start().
//
// Build:    target embed_demo
// Run:      build\embed_demo\Release\embed_demo.exe
//           Close the UI window OR press Ctrl+C to exit.

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <thread>

#include "imgui.h"
#include "ImGuiDx11App.h"
#include "UiWindow.h"

// ---- Shared state produced by the "existing" program. -------------------
// In real code this is whatever data you already have. Make it safe to
// read from the UI thread (atomics, mutex, snapshot — your choice).
struct AppState
{
    std::atomic<int>   tick{ 0 };
    std::atomic<float> sine{ 0.0f };
    std::atomic<bool>  paused{ false };
};

// ---- A tiny UiWindow that visualizes the shared state. ------------------
class StateWindow : public UiWindow
{
public:
    explicit StateWindow(AppState* s) : state_(s) {}

    void Draw() override
    {
        ImGui::Begin("Live State", &open_);
        ImGui::Text("tick  = %d", state_->tick.load());
        ImGui::Text("sine  = %.3f", state_->sine.load());
        bool paused = state_->paused.load();
        if (ImGui::Checkbox("paused", &paused))
            state_->paused.store(paused);
        ImGui::Separator();
        ImGui::TextDisabled("(close this window to terminate the program)");
        ImGui::End();
    }

private:
    AppState* state_;
};

// ---- "Existing" program -------------------------------------------------
int main()
{
    AppState state;

    // === Embedding: just 3 lines added to the existing program. ===
    ImGuiDx11App ui(L"Embedded Debug UI", 480, 260);
    auto win = std::make_shared<StateWindow>(&state);
    ui.AddWindow(win);
    ui.Start();
    // ==============================================================

    std::printf("Existing loop running. Close the UI window to exit.\n");

    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();

    // The original main loop — completely untouched in shape.
    while (ui.IsRunning())
    {
        if (!state.paused.load())
        {
            state.tick.fetch_add(1);
            float t = std::chrono::duration<float>(clock::now() - t0).count();
            state.sine.store(std::sin(t * 2.0f));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // ui's destructor would also Stop()+join, but call explicitly for clarity.
    ui.Stop();
    std::printf("Exited cleanly. Final tick = %d\n", state.tick.load());
    return 0;
}