#include "modpanel.h"

#include <imgui.h>

#include "scripting/modhost.h"

using namespace Scripting;

/// The host of the mods: versions, the game and where the files are
void ModPanel::drawAbout() {
  auto& host = ModHost::inst();

  ImGui::Text("Mods of OpenGothic, JavaScript (QuickJS)");
  ImGui::Separator();
  ImGui::Text("host %s, api %d",HostVersion,ApiVersion);
  ImGui::Text("game %s, platform %s",host.gameName().c_str(),host.platformName().c_str());
  ImGui::Text("%d of %d mods running",int(host.runningCount()),int(host.mods().size()));

  ImGui::Spacing();
  ImGui::TextDisabled("Folders of the mods:");
  for(auto& r:host.roots())
    ImGui::TextWrapped("%s",r.string().c_str());
  ImGui::TextDisabled("Data of the mods:");
  ImGui::TextWrapped("%s",host.dataDir().string().c_str());

  ImGui::Spacing();
  ImGui::TextWrapped("Every mod runs in a world of its own and cannot see the others. "
                     "The description of the API is in docs/MODS.md.");
  }
