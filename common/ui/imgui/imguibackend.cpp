#include "imguibackend.h"

#include <algorithm>
#include <cstdio>

#include <imgui.h>

using namespace Tempest;

namespace {

/// A font with the letters of the languages of the game. The default font of ImGui has no diacritics.
const char* const FontFiles[] = {
  "C:/Windows/Fonts/segoeui.ttf",
  "C:/Windows/Fonts/arial.ttf",
  "/system/fonts/Roboto-Regular.ttf",
  "/system/fonts/DroidSans.ttf",
  "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
  "/System/Library/Fonts/Supplemental/Arial.ttf",
  };

/// Latin with Latin Extended-A: Polish, German, Czech and the punctuation
const ImWchar FontRanges[] = {0x0020,0x00FF, 0x0100,0x017F, 0x2010,0x2027, 0};

ImGuiKey translate(Event::KeyType k) {
  if(Event::K_A<=k && k<=Event::K_Z)
    return ImGuiKey(int(ImGuiKey_A)+int(k-Event::K_A));
  if(Event::K_0<=k && k<=Event::K_9)
    return ImGuiKey(int(ImGuiKey_0)+int(k-Event::K_0));
  if(Event::K_F1<=k && k<=Event::K_F12)
    return ImGuiKey(int(ImGuiKey_F1)+int(k-Event::K_F1));

  switch(k) {
    case Event::K_ESCAPE:   return ImGuiKey_Escape;
    case Event::K_Tab:      return ImGuiKey_Tab;
    case Event::K_Left:     return ImGuiKey_LeftArrow;
    case Event::K_Right:    return ImGuiKey_RightArrow;
    case Event::K_Up:       return ImGuiKey_UpArrow;
    case Event::K_Down:     return ImGuiKey_DownArrow;
    case Event::K_PageUp:   return ImGuiKey_PageUp;
    case Event::K_PageDown: return ImGuiKey_PageDown;
    case Event::K_Home:     return ImGuiKey_Home;
    case Event::K_End:      return ImGuiKey_End;
    case Event::K_Insert:   return ImGuiKey_Insert;
    case Event::K_Delete:   return ImGuiKey_Delete;
    case Event::K_Back:     return ImGuiKey_Backspace;
    case Event::K_Space:    return ImGuiKey_Space;
    case Event::K_Return:   return ImGuiKey_Enter;
    case Event::K_LShift:   return ImGuiKey_LeftShift;
    case Event::K_RShift:   return ImGuiKey_RightShift;
    case Event::K_LControl: return ImGuiKey_LeftCtrl;
    case Event::K_RControl: return ImGuiKey_RightCtrl;
    case Event::K_LAlt:     return ImGuiKey_LeftAlt;
    case Event::K_RAlt:     return ImGuiKey_RightAlt;
    default:                break;
    }
  return ImGuiKey_None;
  }

/// The look of Gothic: a dark stone, gold for the active things
void theme(float scale) {
  ImGuiStyle& st = ImGui::GetStyle();
  st = ImGuiStyle();
  ImGui::StyleColorsDark(&st);

  st.WindowRounding    = 0;
  st.ChildRounding     = 0;
  st.FrameRounding     = 2;
  st.PopupRounding     = 0;
  st.ScrollbarRounding = 2;
  st.TabRounding       = 0;
  st.WindowBorderSize  = 2;
  st.FrameBorderSize   = 1;
  st.AntiAliasedLines  = false;
  st.AntiAliasedFill   = false;
  st.AntiAliasedLinesUseTex = false;
  st.WindowPadding     = ImVec2(12,10);
  st.FramePadding      = ImVec2(10,6);
  st.ItemSpacing       = ImVec2(10,8);
  st.ScrollbarSize     = 16;
  st.GrabMinSize       = 18;

  const ImVec4 gold     (0.82f,0.66f,0.34f,1.00f);
  const ImVec4 goldSoft (0.82f,0.66f,0.34f,0.55f);
  const ImVec4 dark     (0.02f,0.025f,0.035f,0.97f);
  const ImVec4 panel    (0.05f,0.055f,0.07f,1.00f);
  const ImVec4 hover    (0.14f,0.11f,0.06f,1.00f);
  const ImVec4 active   (0.24f,0.18f,0.08f,1.00f);

  auto* c = st.Colors;
  c[ImGuiCol_WindowBg]          = dark;
  c[ImGuiCol_ChildBg]           = ImVec4(0,0,0,0.30f);
  c[ImGuiCol_PopupBg]           = dark;
  c[ImGuiCol_Border]            = ImVec4(0.62f,0.50f,0.28f,0.95f);
  c[ImGuiCol_Text]              = ImVec4(0.90f,0.88f,0.82f,1.00f);
  c[ImGuiCol_TextDisabled]      = ImVec4(0.52f,0.52f,0.52f,1.00f);
  c[ImGuiCol_FrameBg]           = panel;
  c[ImGuiCol_FrameBgHovered]    = hover;
  c[ImGuiCol_FrameBgActive]     = active;
  c[ImGuiCol_TitleBg]           = dark;
  c[ImGuiCol_TitleBgActive]     = dark;
  c[ImGuiCol_Button]            = panel;
  c[ImGuiCol_ButtonHovered]     = hover;
  c[ImGuiCol_ButtonActive]      = active;
  c[ImGuiCol_Header]            = ImVec4(0.14f,0.11f,0.06f,0.80f);
  c[ImGuiCol_HeaderHovered]     = hover;
  c[ImGuiCol_HeaderActive]      = active;
  c[ImGuiCol_Separator]         = goldSoft;
  c[ImGuiCol_Tab]               = panel;
  c[ImGuiCol_TabHovered]        = hover;
  c[ImGuiCol_TabSelected]       = ImVec4(0.20f,0.15f,0.07f,1.00f);
  c[ImGuiCol_TabSelectedOverline] = gold;
  c[ImGuiCol_TabDimmed]         = panel;
  c[ImGuiCol_TabDimmedSelected] = ImVec4(0.20f,0.15f,0.07f,1.00f);
  c[ImGuiCol_CheckMark]         = gold;
  c[ImGuiCol_SliderGrab]        = gold;
  c[ImGuiCol_ScrollbarBg]       = ImVec4(0,0,0,0.30f);
  c[ImGuiCol_ScrollbarGrab]     = goldSoft;
  c[ImGuiCol_ScrollbarGrabHovered] = gold;
  c[ImGuiCol_ScrollbarGrabActive]  = gold;
  c[ImGuiCol_TableHeaderBg]     = panel;
  c[ImGuiCol_TableBorderStrong] = goldSoft;
  c[ImGuiCol_TableBorderLight]  = ImVec4(0.6f,0.64f,0.7f,0.20f);
  c[ImGuiCol_TextSelectedBg]    = ImVec4(0.82f,0.66f,0.34f,0.35f);

  st.ScaleAllSizes(scale);
  }

}

ImGuiBackend::ImGuiBackend(TextureAtlas& atlas):atlas(atlas) {
  IMGUI_CHECKVERSION();
  ctx = ImGui::CreateContext();
  ImGui::SetCurrentContext(ctx);

  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
  }

ImGuiBackend::~ImGuiBackend() {
  ImGui::SetCurrentContext(ctx);
  ImGui::DestroyContext(ctx);
  }

void ImGuiBackend::setScale(float s) {
  s = std::clamp(s,0.5f,4.f);
  if(std::abs(s-scale)<0.01f)
    return;
  scale = s;
  ImGui::SetCurrentContext(ctx);
  theme(scale);
  buildFont();
  }

void ImGuiBackend::buildFont() {
  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->Clear();

  const float size = std::max(8.f,std::round(16.f*scale));
  ImFont* f = nullptr;
  for(const char* file:FontFiles) {
    if(std::FILE* fp = std::fopen(file,"rb")) {
      std::fclose(fp);
      ImFontConfig cfg;
      cfg.OversampleH = 2;
      cfg.OversampleV = 1;
      f = io.Fonts->AddFontFromFileTTF(file,size,&cfg,FontRanges);
      if(f!=nullptr)
        break;
      }
    }
  if(f==nullptr) {
    ImFontConfig cfg;
    cfg.SizePixels = size;
    io.Fonts->AddFontDefault(&cfg);
    }

  unsigned char* pixels = nullptr;
  int            w = 0, h = 0;
  io.Fonts->GetTexDataAsAlpha8(&pixels,&w,&h);
  font = atlas.load(pixels,uint32_t(w),uint32_t(h),TextureFormat::R8);
  io.Fonts->SetTexID(ImTextureID(1));
  }

void ImGuiBackend::newFrame(int width, int height) {
  ImGui::SetCurrentContext(ctx);
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(float(width),float(height));
  io.DeltaTime   = 1.f/60.f;
  ImGui::NewFrame();
  }

void ImGuiBackend::render(Painter& p) {
  ImGui::SetCurrentContext(ctx);
  ImGui::Render();
  ImDrawData* dd = ImGui::GetDrawData();
  if(dd==nullptr || font.isEmpty())
    return;

  const float tw = float(font.w());
  const float th = float(font.h());

  uint32_t lastColor = 0;
  bool     first     = true;
  for(int n=0; n<dd->CmdListsCount; ++n) {
    const ImDrawList* list = dd->CmdLists[n];
    const ImDrawVert* vtx  = list->VtxBuffer.Data;
    const ImDrawIdx*  idx  = list->IdxBuffer.Data;

    for(const ImDrawCmd& cmd:list->CmdBuffer) {
      if(cmd.UserCallback!=nullptr)
        continue;

      const int x0 = std::max(0,int(cmd.ClipRect.x));
      const int y0 = std::max(0,int(cmd.ClipRect.y));
      const int x1 = int(cmd.ClipRect.z);
      const int y1 = int(cmd.ClipRect.w);
      if(x1<=x0 || y1<=y0)
        continue;
      p.setScissor(x0,y0,x1-x0,y1-y0);

      for(unsigned i=0; i+2<cmd.ElemCount; i+=3) {
        const ImDrawVert& a = vtx[idx[cmd.IdxOffset+i+0]+cmd.VtxOffset];
        const ImDrawVert& b = vtx[idx[cmd.IdxOffset+i+1]+cmd.VtxOffset];
        const ImDrawVert& c = vtx[idx[cmd.IdxOffset+i+2]+cmd.VtxOffset];

        // a triangle has one color, which is what the widgets of ImGui use without anti-aliasing
        if(first || a.col!=lastColor) {
          const float r  = float((a.col>>IM_COL32_R_SHIFT)&0xFF)/255.f;
          const float g  = float((a.col>>IM_COL32_G_SHIFT)&0xFF)/255.f;
          const float bl = float((a.col>>IM_COL32_B_SHIFT)&0xFF)/255.f;
          const float al = float((a.col>>IM_COL32_A_SHIFT)&0xFF)/255.f;
          p.setBrush(Brush(font,Color(r,g,bl,al),PaintDevice::Alpha));
          lastColor = a.col;
          first     = false;
          }
        p.drawTriangle(a.pos.x,a.pos.y,a.uv.x*tw,a.uv.y*th,
                       b.pos.x,b.pos.y,b.uv.x*tw,b.uv.y*th,
                       c.pos.x,c.pos.y,c.uv.x*tw,c.uv.y*th);
        }
      }
    }
  }

// --- input

void ImGuiBackend::mouseMove(float x, float y) {
  ImGui::SetCurrentContext(ctx);
  ImGui::GetIO().AddMousePosEvent(x,y);
  }

void ImGuiBackend::mouseButton(int button, bool down) {
  ImGui::SetCurrentContext(ctx);
  ImGui::GetIO().AddMouseButtonEvent(button,down);
  }

void ImGuiBackend::mouseWheel(float x, float y) {
  ImGui::SetCurrentContext(ctx);
  ImGui::GetIO().AddMouseWheelEvent(x,y);
  }

void ImGuiBackend::scrollByPixels(float dy) {
  ImGui::SetCurrentContext(ctx);
  // a step of the wheel is five lines
  const float step = 5.f*ImGui::GetFontSize();
  ImGui::GetIO().AddMouseWheelEvent(0.f,dy/std::max(1.f,step));
  }

void ImGuiBackend::key(Event::KeyType k, bool down) {
  const auto ik = translate(k);
  if(ik==ImGuiKey_None)
    return;
  ImGui::SetCurrentContext(ctx);
  ImGui::GetIO().AddKeyEvent(ik,down);
  }

void ImGuiBackend::text(uint32_t unicode) {
  ImGui::SetCurrentContext(ctx);
  ImGui::GetIO().AddInputCharacter(unicode);
  }

bool ImGuiBackend::wantsKeyboard() const {
  ImGui::SetCurrentContext(ctx);
  return ImGui::GetIO().WantCaptureKeyboard;
  }

bool ImGuiBackend::wantsText() const {
  ImGui::SetCurrentContext(ctx);
  return ImGui::GetIO().WantTextInput;
  }
