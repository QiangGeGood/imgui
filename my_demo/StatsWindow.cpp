#include "StatsWindow.h"
#include "imgui.h"

void StatsWindow::Draw()
{
    ImGui::Begin("Stats", OpenFlag());
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS: %.1f", io.Framerate);
    ImGui::Text("Frame: %.3f ms", 1000.0f / (io.Framerate > 0.0f ? io.Framerate : 1.0f));
    ImGui::End();
}