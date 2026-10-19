#include "modpanel.h"

#include <imgui.h>

ModPanel::ModPanel() {
  }

void ModPanel::reset() {
  focusInput = true;
  }

bool ModPanel::draw(float width, float height) {
  const float margin = std::min(width,height)*0.03f;
  ImGui::SetNextWindowPos(ImVec2(margin,margin));
  ImGui::SetNextWindowSize(ImVec2(width-2*margin,height-2*margin));

  bool open  = true;
  const auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
  if(ImGui::Begin("OpenGothic mods",&open,flags)) {
    if(ImGui::BeginTabBar("tabs")) {
      if(ImGui::BeginTabItem("Mods")) {
        drawMods();
        ImGui::EndTabItem();
        }
      if(ImGui::BeginTabItem("Console")) {
        drawConsole();
        ImGui::EndTabItem();
        }
      if(ImGui::BeginTabItem("About")) {
        drawAbout();
        ImGui::EndTabItem();
        }
      ImGui::EndTabBar();
      }
    }
  ImGui::End();
  return open;
  }
