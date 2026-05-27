#include "ProgressWindow.h"
#include "imgui.h"

void ProgressWindow::Draw()
{
    if (running_)
    {
        progress_ += speed_ * ImGui::GetIO().DeltaTime;
        if (progress_ > 1.0f) progress_ -= 1.0f; // wrap
    }

    ImGui::Begin("Progress", OpenFlag());
    ImGui::ProgressBar(progress_, ImVec2(-1.0f, 0.0f));
    ImGui::SliderFloat("Speed", &speed_, 0.0f, 2.0f, "%.2f /s");
    if (ImGui::Button(running_ ? "Pause" : "Resume")) running_ = !running_;
    ImGui::SameLine();
    if (ImGui::Button("Reset")) progress_ = 0.0f;
    ImGui::End();
}