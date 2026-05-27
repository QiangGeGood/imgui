#include "HelloWindow.h"
#include "imgui.h"

void HelloWindow::Draw()
{
    ImGui::Begin("Hello", OpenFlag());
    ImGui::Text("Hello, Dear ImGui!");
    if (ImGui::Button("Click")) counter_++;
    ImGui::SameLine();
    ImGui::Text("count = %d", counter_);
    ImGui::End();
}