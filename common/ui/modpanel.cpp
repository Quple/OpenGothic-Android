#include "modpanel.h"

#include <imgui.h>

#include "scripting/modhost.h"

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
      // the tabs that mods have made (ui.tab in the scripts), the mods are in charge of what is in them
      auto& host = Scripting::ModHost::inst();
      for(auto& m:host.mods()) {
        if(!m->isRunning())
          continue;
        for(size_t i=0; i<m->tabs.size(); ++i) {
          const std::string title = m->tabs[i].title;
          if(ImGui::BeginTabItem(title.c_str())) {
            if(m->tabs[i].failed)
              ImGui::TextColored(ImVec4(0.95f,0.35f,0.30f,1.f),"This tab has failed, see the console. Reset the mod in the tab Mods.");
            else
              host.drawTab(*m,i);
            ImGui::EndTabItem();
            }
          if(!m->isRunning() || i>=m->tabs.size())
            break;
          }
        }
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
