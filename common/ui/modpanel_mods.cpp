#include "modpanel.h"

#include <imgui.h>

#include "scripting/modhost.h"

using namespace Scripting;

/// A card for every mod: what it is, whether it works, and the buttons to start, stop and reset it
void ModPanel::drawMods() {
  auto& host = ModHost::inst();

  if(ImGui::Button("Rescan"))
    host.scan();
  ImGui::SameLine();
  ImGui::TextDisabled("%d of %d running",int(host.runningCount()),int(host.mods().size()));

  const float footer = ImGui::GetTextLineHeightWithSpacing()*2.f;
  ImGui::BeginChild("list",ImVec2(0,-footer),ImGuiChildFlags_None);

  const auto& mods = host.mods();
  if(mods.empty()) {
    ImGui::TextWrapped("There are no mods. Put folders or zip files with a manifest.json into:");
    for(auto& r:host.roots())
      ImGui::TextColored(ImVec4(0.82f,0.66f,0.34f,1.f),"%s",r.string().c_str());
    }

  for(size_t i=0; i<mods.size(); ++i) {
    Mod& m = *mods[i];
    ImGui::PushID(int(i));
    ImGui::BeginChild("card",ImVec2(0,0),ImGuiChildFlags_Borders|ImGuiChildFlags_AutoResizeY);

    ImGui::TextColored(ImVec4(0.82f,0.66f,0.34f,1.f),"%s",m.manifest.name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("v%s%s%s",m.manifest.version.c_str(),
                        m.manifest.author.empty() ? "" : "  by ",m.manifest.author.c_str());

    ImVec4 color(0.55f,0.55f,0.55f,1.f);
    const char* state = "stopped";
    if(m.state==Mod::State::Running) {
      color = ImVec4(0.45f,0.85f,0.45f,1.f);
      state = "running";
      } else
    if(m.state==Mod::State::Failed) {
      color = ImVec4(0.95f,0.35f,0.30f,1.f);
      state = "failed";
      }
    ImGui::TextColored(color,"%s",state);
    if(m.errors>0) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.95f,0.60f,0.30f,1.f),"%d error(s), see the console",int(m.errors));
      }

    if(!m.manifest.description.empty())
      ImGui::TextWrapped("%s",m.manifest.description.c_str());
    if(m.state==Mod::State::Failed && !m.error.empty())
      ImGui::TextColored(ImVec4(0.95f,0.35f,0.30f,1.f),"%s",m.error.substr(0,m.error.find('\n')).c_str());

    if(ImGui::Button(m.isRunning() ? "Stop" : "Start")) {
      if(m.isRunning())
        host.stop(m); else
        host.start(m);
      }
    ImGui::SameLine();
    if(ImGui::Button("Reset"))
      host.reset(m);
    ImGui::SameLine();
    bool autoStart = m.enabled;
    if(ImGui::Checkbox("Starts with the game",&autoStart))
      host.setEnabled(m,autoStart);

    ImGui::EndChild();
    ImGui::PopID();
    }
  ImGui::EndChild();

  ImGui::Separator();
  ImGui::TextDisabled("Reset reads the files of a mod again. Errors of the scripts are in the console.");
  }
