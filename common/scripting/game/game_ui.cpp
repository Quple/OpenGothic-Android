#include "scripting/gameapi.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

#include <imgui.h>

using namespace Scripting;

/// ui: the menu of the mods (F10). A mod makes a tab with ui.tab(title, fn); fn is called every frame while the tab is
/// open, and the widgets that it calls are drawn at once (Dear ImGui): a button is a function that returns true in the
/// frame in which it is pressed, a field of text returns what is in it, so the mod keeps the state itself.
///
///   ui.tab("Co-op", () => {
///     ui.text("Hello");
///     if (ui.button("Host")) startHosting();
///     address = ui.inputText("Address", address);
///   });
///
/// Outside of a tab the widgets do nothing. Every pair of Begin and End of ImGui is a function with a callback
/// (child, withId, withColor), so a mistake of a mod cannot leave the menu in a broken state.
namespace {

bool drawing() {
  return ModHost::inst().isDrawing();
  }

double num(JSContext* ctx, int argc, JSValueConst* argv, int at, double def) {
  double v = def;
  if(at<argc && !JS_IsUndefined(argv[at]) && JS_ToFloat64(ctx,&v,argv[at])<0) {
    JS_FreeValue(ctx,JS_GetException(ctx));
    return def;
    }
  return v;
  }

/// A color: [r, g, b, a] of 0 to 1, or "#rrggbb" or "#rrggbbaa"
ImVec4 color(JSContext* ctx, JSValueConst v, ImVec4 def) {
  if(JS_IsString(v)) {
    const std::string s = js::str(ctx,v);
    if(s.size()>=7 && s[0]=='#') {
      auto hex = [&](size_t at) { return float(std::stoi(s.substr(at,2),nullptr,16))/255.f; };
      try {
        return ImVec4(hex(1),hex(3),hex(5),s.size()>=9 ? hex(7) : 1.f);
        } catch(...) {
        return def;
        }
      }
    return def;
    }
  if(JS_IsArray(v)) {
    float c[4] = {def.x,def.y,def.z,def.w};
    for(uint32_t i=0; i<4; ++i) {
      JSValue e = JS_GetPropertyUint32(ctx,v,i);
      double  d = 0;
      if(JS_IsNumber(e) && JS_ToFloat64(ctx,&d,e)==0)
        c[i] = float(d);
      JS_FreeValue(ctx,e);
      }
    return ImVec4(c[0],c[1],c[2],c[3]);
    }
  return def;
  }

// --- text

JSValue text(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(drawing())
    ImGui::TextUnformatted(js::join(ctx,argc,argv).c_str());
  return JS_UNDEFINED;
  }

JSValue textWrapped(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(drawing())
    ImGui::TextWrapped("%s",js::join(ctx,argc,argv).c_str());
  return JS_UNDEFINED;
  }

JSValue textDisabled(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(drawing())
    ImGui::TextDisabled("%s",js::join(ctx,argc,argv).c_str());
  return JS_UNDEFINED;
  }

/// textColored(color, text...)
JSValue textColored(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(drawing() && argc>1)
    ImGui::TextColored(color(ctx,argv[0],ImVec4(1,1,1,1)),"%s",js::join(ctx,argc-1,argv+1).c_str());
  return JS_UNDEFINED;
  }

JSValue bullet(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(drawing())
    ImGui::BulletText("%s",js::join(ctx,argc,argv).c_str());
  return JS_UNDEFINED;
  }

// --- widgets

/// button(label, width, height): true in the frame in which it is pressed. A negative width goes to the right edge.
JSValue button(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(!drawing() || argc<1)
    return JS_FALSE;
  const auto label = js::str(ctx,argv[0]);
  return JS_NewBool(ctx,ImGui::Button(label.c_str(),ImVec2(float(num(ctx,argc,argv,1,0)),float(num(ctx,argc,argv,2,0)))));
  }

/// checkbox(label, value): the value after the frame
JSValue checkbox(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<2)
    return JS_FALSE;
  bool v = JS_ToBool(ctx,argv[1])>0;
  if(drawing())
    ImGui::Checkbox(js::str(ctx,argv[0]).c_str(),&v);
  return JS_NewBool(ctx,v);
  }

/// inputText(label, value, flags): the text after the frame. flags: "decimal", "password", "enter" (the text is
/// the same until Enter), "readonly", the words are separated by spaces.
JSValue inputText(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<2)
    return JS_UNDEFINED;
  std::string value = js::str(ctx,argv[1]);
  if(!drawing())
    return JS_NewString(ctx,value.c_str());

  ImGuiInputTextFlags flags = 0;
  if(argc>2 && JS_IsString(argv[2])) {
    const auto f = js::str(ctx,argv[2]);
    if(f.find("decimal")!=std::string::npos)  flags |= ImGuiInputTextFlags_CharsDecimal;
    if(f.find("password")!=std::string::npos) flags |= ImGuiInputTextFlags_Password;
    if(f.find("enter")!=std::string::npos)    flags |= ImGuiInputTextFlags_EnterReturnsTrue;
    if(f.find("readonly")!=std::string::npos) flags |= ImGuiInputTextFlags_ReadOnly;
    }

  std::vector<char> buf(std::max<size_t>(512,value.size()+128),'\0');
  std::copy(value.begin(),value.end(),buf.begin());
  ImGui::InputText(js::str(ctx,argv[0]).c_str(),buf.data(),buf.size(),flags);
  return JS_NewString(ctx,buf.data());
  }

/// sliderInt(label, value, min, max), sliderFloat(...): the value after the frame
JSValue slider(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int isFloat) {
  if(argc<4)
    return JS_UNDEFINED;
  const double v = num(ctx,argc,argv,1,0), lo = num(ctx,argc,argv,2,0), hi = num(ctx,argc,argv,3,1);
  if(!drawing())
    return JS_NewFloat64(ctx,v);
  const auto label = js::str(ctx,argv[0]);
  if(isFloat) {
    float f = float(v);
    ImGui::SliderFloat(label.c_str(),&f,float(lo),float(hi));
    return JS_NewFloat64(ctx,double(f));
    }
  int i = int(v);
  ImGui::SliderInt(label.c_str(),&i,int(lo),int(hi));
  return JS_NewInt32(ctx,i);
  }

/// combo(label, index, items): the index after the frame
JSValue combo(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<3 || !JS_IsArray(argv[2]))
    return JS_UNDEFINED;
  int index = int(num(ctx,argc,argv,1,0));
  if(!drawing())
    return JS_NewInt32(ctx,index);

  std::vector<std::string> items;
  JSValue  len = JS_GetPropertyStr(ctx,argv[2],"length");
  uint32_t n   = 0;
  JS_ToUint32(ctx,&n,len);
  JS_FreeValue(ctx,len);
  for(uint32_t i=0; i<n && i<256; ++i) {
    JSValue e = JS_GetPropertyUint32(ctx,argv[2],i);
    items.push_back(js::str(ctx,e));
    JS_FreeValue(ctx,e);
    }

  const char* shown = index>=0 && size_t(index)<items.size() ? items[size_t(index)].c_str() : "";
  if(ImGui::BeginCombo(js::str(ctx,argv[0]).c_str(),shown)) {
    for(size_t i=0; i<items.size(); ++i) {
      if(ImGui::Selectable(items[i].c_str(),int(i)==index))
        index = int(i);
      }
    ImGui::EndCombo();
    }
  return JS_NewInt32(ctx,index);
  }

/// selectable(label, selected): true in the frame in which it is pressed
JSValue selectable(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(!drawing() || argc<1)
    return JS_FALSE;
  const bool selected = argc>1 && JS_ToBool(ctx,argv[1])>0;
  return JS_NewBool(ctx,ImGui::Selectable(js::str(ctx,argv[0]).c_str(),selected));
  }

// --- layout

JSValue sameLine(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(drawing())
    ImGui::SameLine(float(num(ctx,argc,argv,0,0)));
  return JS_UNDEFINED;
  }

JSValue separator(JSContext*, JSValueConst, int, JSValueConst*) {
  if(drawing())
    ImGui::Separator();
  return JS_UNDEFINED;
  }

JSValue spacing(JSContext*, JSValueConst, int, JSValueConst*) {
  if(drawing())
    ImGui::Spacing();
  return JS_UNDEFINED;
  }

JSValue indent(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(drawing())
    ImGui::Indent(float(num(ctx,argc,argv,0,0)));
  return JS_UNDEFINED;
  }

JSValue unindent(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(drawing())
    ImGui::Unindent(float(num(ctx,argc,argv,0,0)));
  return JS_UNDEFINED;
  }

/// setNextWidth(width): of the next widget, a negative one is from the right edge
JSValue setNextWidth(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(drawing())
    ImGui::SetNextItemWidth(float(num(ctx,argc,argv,0,0)));
  return JS_UNDEFINED;
  }

JSValue fontSize(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  return JS_NewFloat64(ctx,drawing() ? double(ImGui::GetFontSize()) : 16.0);
  }

/// width(): the room to the right edge
JSValue width(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  return JS_NewFloat64(ctx,drawing() ? double(ImGui::GetContentRegionAvail().x) : 0.0);
  }

JSValue textWidth(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(!drawing() || argc<1)
    return JS_NewFloat64(ctx,0);
  return JS_NewFloat64(ctx,double(ImGui::CalcTextSize(js::str(ctx,argv[0]).c_str()).x));
  }

/// anyActive(): a field has the focus, the keyboard of the screen is shown for it
JSValue anyActive(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  return JS_NewBool(ctx,drawing() && ImGui::IsAnyItemActive());
  }

// --- groups, with a function between the beginning and the end

JSValue call(JSContext* ctx, JSValueConst fn) {
  return JS_Call(ctx,fn,JS_UNDEFINED,0,nullptr);
  }

/// child(id, width, height, fn): a region with a border that scrolls
JSValue child(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(!drawing() || argc<4 || !JS_IsFunction(ctx,argv[3]))
    return JS_UNDEFINED;
  JSValue ret = JS_UNDEFINED;
  if(ImGui::BeginChild(js::str(ctx,argv[0]).c_str(),
                       ImVec2(float(num(ctx,argc,argv,1,0)),float(num(ctx,argc,argv,2,0))),ImGuiChildFlags_Borders))
    ret = call(ctx,argv[3]);
  ImGui::EndChild();
  return ret;
  }

/// withId(id, fn): the widgets of fn have the id, for the widgets that have the same label
JSValue withId(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(!drawing() || argc<2 || !JS_IsFunction(ctx,argv[1]))
    return JS_UNDEFINED;
  ImGui::PushID(js::str(ctx,argv[0]).c_str());
  JSValue ret = call(ctx,argv[1]);
  ImGui::PopID();
  return ret;
  }

/// withColor(color, fn): the text of fn has the color
JSValue withColor(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(!drawing() || argc<2 || !JS_IsFunction(ctx,argv[1]))
    return JS_UNDEFINED;
  ImGui::PushStyleColor(ImGuiCol_Text,color(ctx,argv[0],ImVec4(1,1,1,1)));
  JSValue ret = call(ctx,argv[1]);
  ImGui::PopStyleColor();
  return ret;
  }

/// withScale(factor, fn): the text of fn is larger or smaller
JSValue withScale(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(!drawing() || argc<2 || !JS_IsFunction(ctx,argv[1]))
    return JS_UNDEFINED;
  ImGui::SetWindowFontScale(std::clamp(float(num(ctx,argc,argv,0,1)),0.5f,3.f));
  JSValue ret = call(ctx,argv[1]);
  ImGui::SetWindowFontScale(1.f);
  return ret;
  }

// --- tabs

/// tab(title, fn): a tab in the menu of the mods, fn draws it. The same title replaces the tab.
JSValue tab(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<2 || !JS_IsFunction(ctx,argv[1]))
    return JS_ThrowTypeError(ctx,"ui.tab(title, fn)");
  auto&       m     = js::mod(ctx);
  const auto  title = js::str(ctx,argv[0]);
  for(auto& t:m.tabs) {
    if(t.title!=title)
      continue;
    JS_FreeValue(ctx,t.fn);
    t.fn     = JS_DupValue(ctx,argv[1]);
    t.failed = false;
    return JS_UNDEFINED;
    }
  Mod::Tab t;
  t.title = title;
  t.fn    = JS_DupValue(ctx,argv[1]);
  m.tabs.push_back(std::move(t));
  return JS_UNDEFINED;
  }

JSValue removeTab(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<1)
    return JS_UNDEFINED;
  auto&      m     = js::mod(ctx);
  const auto title = js::str(ctx,argv[0]);
  for(size_t i=0; i<m.tabs.size(); ++i) {
    if(m.tabs[i].title!=title)
      continue;
    JS_FreeValue(ctx,m.tabs[i].fn);
    m.tabs.erase(m.tabs.begin()+std::ptrdiff_t(i));
    break;
    }
  return JS_UNDEFINED;
  }

}

void game::installUi(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue ui     = JS_NewObject(ctx);

  js::fn(ctx,ui,"tab",        tab,2);
  js::fn(ctx,ui,"removeTab",  removeTab,1);
  js::fn(ctx,ui,"text",       text,1);
  js::fn(ctx,ui,"textWrapped",textWrapped,1);
  js::fn(ctx,ui,"textDisabled",textDisabled,1);
  js::fn(ctx,ui,"textColored",textColored,2);
  js::fn(ctx,ui,"bullet",     bullet,1);
  js::fn(ctx,ui,"button",     button,3);
  js::fn(ctx,ui,"checkbox",   checkbox,2);
  js::fn(ctx,ui,"inputText",  inputText,3);
  js::fnMagic(ctx,ui,"sliderInt",  [](JSContext* c, JSValueConst t, int n, JSValueConst* a, int) { return slider(c,t,n,a,0); },4,0);
  js::fnMagic(ctx,ui,"sliderFloat",[](JSContext* c, JSValueConst t, int n, JSValueConst* a, int) { return slider(c,t,n,a,1); },4,1);
  js::fn(ctx,ui,"combo",      combo,3);
  js::fn(ctx,ui,"selectable", selectable,2);
  js::fn(ctx,ui,"sameLine",   sameLine,1);
  js::fn(ctx,ui,"separator",  separator,0);
  js::fn(ctx,ui,"spacing",    spacing,0);
  js::fn(ctx,ui,"indent",     indent,1);
  js::fn(ctx,ui,"unindent",   unindent,1);
  js::fn(ctx,ui,"setNextWidth",setNextWidth,1);
  js::fn(ctx,ui,"fontSize",   fontSize,0);
  js::fn(ctx,ui,"width",      width,0);
  js::fn(ctx,ui,"textWidth",  textWidth,1);
  js::fn(ctx,ui,"anyActive",  anyActive,0);
  js::fn(ctx,ui,"child",      child,4);
  js::fn(ctx,ui,"withId",     withId,2);
  js::fn(ctx,ui,"withColor",  withColor,2);
  js::fn(ctx,ui,"withScale",  withScale,2);

  JS_SetPropertyStr(ctx,global,"ui",ui);
  JS_FreeValue(ctx,global);
  }
