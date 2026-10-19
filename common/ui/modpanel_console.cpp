#include "modpanel.h"

#include <algorithm>
#include <cstring>

#include <imgui.h>

#include "scripting/modhost.h"

using namespace Scripting;

namespace {

ImVec4 levelColor(ModHost::LogLine::Level l) {
  switch(l) {
    case ModHost::LogLine::Error: return ImVec4(0.95f,0.35f,0.30f,1.f);
    case ModHost::LogLine::Warn:  return ImVec4(0.95f,0.78f,0.30f,1.f);
    case ModHost::LogLine::Debug: return ImVec4(0.55f,0.55f,0.55f,1.f);
    default:                      break;
    }
  return ImVec4(0.90f,0.88f,0.82f,1.f);
  }

}

/// Up and Down walk through the history, Tab completes the name of a command
int ModPanel::inputCallback(ImGuiInputTextCallbackData* data) {
  auto* self = static_cast<ModPanel*>(data->UserData);

  if(data->EventFlag==ImGuiInputTextFlags_CallbackHistory) {
    if(self->history.empty())
      return 0;
    if(data->EventKey==ImGuiKey_UpArrow && self->historyAt>0)
      --self->historyAt; else
    if(data->EventKey==ImGuiKey_DownArrow && self->historyAt<int(self->history.size()))
      ++self->historyAt;

    const char* text = self->historyAt<int(self->history.size()) ? self->history[size_t(self->historyAt)].c_str() : "";
    data->DeleteChars(0,data->BufTextLen);
    data->InsertChars(0,text);
    return 0;
    }

  if(data->EventFlag==ImGuiInputTextFlags_CallbackCompletion) {
    // the word that is typed
    const std::string typed(data->Buf,size_t(data->CursorPos));
    if(typed.find(' ')!=std::string::npos)
      return 0;
    std::vector<std::string> found;
    for(auto& n:ModHost::inst().commandNames()) {
      if(n.compare(0,typed.size(),typed)==0)
        found.push_back(n);
      }
    if(found.size()==1) {
      data->DeleteChars(0,data->CursorPos);
      data->InsertChars(0,(found[0]+" ").c_str());
      } else
    if(found.size()>1) {
      std::string all = "commands:";
      for(auto& n:found)
        all += " "+n;
      ModHost::inst().log(ModHost::LogLine::Info,"console",all);
      }
    }
  return 0;
  }

void ModPanel::run() {
  if(input[0]=='\0')
    return;
  const std::string line = input;
  ModHost::inst().execute(line);
  if(history.empty() || history.back()!=line)
    history.push_back(line);
  historyAt = int(history.size());
  input[0]  = '\0';
  focusInput = true;
  }

void ModPanel::drawConsole() {
  auto& host = ModHost::inst();

  if(ImGui::Button("Clear"))
    host.clearLog();
  ImGui::SameLine();
  ImGui::TextDisabled("help lists the commands, Tab completes, Up and Down walk through the history");

  const float footer = ImGui::GetFrameHeightWithSpacing()+ImGui::GetStyle().ItemSpacing.y;
  ImGui::BeginChild("log",ImVec2(0,-footer),ImGuiChildFlags_Borders);
  for(auto& l:host.lines()) {
    ImGui::PushStyleColor(ImGuiCol_Text,levelColor(l.level));
    ImGui::TextWrapped("[%s] %s",l.source.c_str(),l.text.c_str());
    ImGui::PopStyleColor();
    }

  // new lines are followed, unless the player has scrolled up
  if(host.serial()!=seenLog) {
    if(ImGui::GetScrollY()>=ImGui::GetScrollMaxY()-4.f || seenLog==0)
      ImGui::SetScrollHereY(1.f);
    seenLog = host.serial();
    }
  ImGui::EndChild();

  const float send = ImGui::CalcTextSize("Send").x+ImGui::GetStyle().FramePadding.x*2.f+ImGui::GetStyle().ItemSpacing.x;
  ImGui::SetNextItemWidth(-send);
  if(focusInput) {
    ImGui::SetKeyboardFocusHere();
    focusInput = false;
    }
  const auto flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory |
                     ImGuiInputTextFlags_CallbackCompletion;
  if(ImGui::InputText("##input",input,sizeof(input),flags,&ModPanel::inputCallback,this))
    run();
  ImGui::SameLine();
  if(ImGui::Button("Send"))
    run();
  }
