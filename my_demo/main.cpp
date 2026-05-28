// Minimal Dear ImGui demo: each window is its own class.
// Demonstrates the weak_ptr lifetime model — the App only observes, the
// caller owns. Releasing the caller's shared_ptr destroys the window
// immediately and the App auto-drops the expired entry next frame.

#include "ImGuiDx11App.h"
#include "HelloWindow.h"
#include "StatsWindow.h"
#include "ProgressWindow.h"

#include "imgui.h"

int main(int, char**)
{
    ImGuiDx11App app(L"My ImGui Demo", 900, 600);

    // Long-lived windows: held in main() until app.Run() returns.
    auto hello = std::make_shared<HelloWindow>();
    auto stats = std::make_shared<StatsWindow>();
    app.AddWindow(hello);
    app.AddWindow(stats);

    // Dynamically created/destroyed window to exercise the weak_ptr model.
    std::shared_ptr<ProgressWindow> progress;
    int frame = 0;

    app.Run([&] {
        ++frame;

        ImGui::Begin("Lifetime Demo");
        ImGui::Text("frame: %d", frame);
        ImGui::Text("progress alive: %s", progress ? "YES" : "NO");
        ImGui::Separator();

        if (ImGui::Button("Create Progress"))
        {
            if (!progress)
            {
                progress = std::make_shared<ProgressWindow>();
                app.AddWindow(progress); // App stores weak_ptr only
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Destroy Progress"))
        {
            progress.reset(); // window dies now; App erases next frame
        }

        ImGui::TextWrapped(
            "Click Create then Destroy: the Progress window appears and "
            "disappears in sync with the owning shared_ptr.");
        ImGui::End();
    });

    return 0;
}
