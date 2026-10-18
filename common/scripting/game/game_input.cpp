#include "scripting/gameapi.h"

#include <set>
#include <Tempest/Event>

using namespace Scripting;

namespace {

std::set<std::string>& pressed() {
  static std::set<std::string> keys;
  return keys;
  }

/// input.isDown("F1") - the keys are written like in the events: A..Z, 0..9, F1..F12, Space, Return, Escape, Up...
JSValue isDown(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<1)
    return JS_FALSE;
  return JS_NewBool(ctx,game::isKeyDown(js::str(ctx,argv[0])));
  }

}

std::string game::keyName(int k) {
  using K = Tempest::Event;
  if(k>=K::K_A && k<=K::K_Z)
    return std::string(1,char('A'+(k-K::K_A)));
  if(k>=K::K_0 && k<=K::K_9)
    return std::string(1,char('0'+(k-K::K_0)));
  if(k>=K::K_F1 && k<=K::K_F24)
    return "F"+std::to_string(1+k-K::K_F1);

  switch(k) {
    case K::K_ESCAPE:   return "Escape";
    case K::K_Return:   return "Return";
    case K::K_Space:    return "Space";
    case K::K_Tab:      return "Tab";
    case K::K_Back:     return "Backspace";
    case K::K_Delete:   return "Delete";
    case K::K_Insert:   return "Insert";
    case K::K_Home:     return "Home";
    case K::K_End:      return "End";
    case K::K_PageUp:   return "PageUp";
    case K::K_PageDown: return "PageDown";
    case K::K_Left:     return "Left";
    case K::K_Right:    return "Right";
    case K::K_Up:       return "Up";
    case K::K_Down:     return "Down";
    case K::K_LShift:
    case K::K_RShift:   return "Shift";
    case K::K_LControl:
    case K::K_RControl: return "Control";
    case K::K_LAlt:
    case K::K_RAlt:     return "Alt";
    case K::K_Pause:    return "Pause";
    default:            break;
    }
  return std::string();
  }

bool game::isKeyDown(std::string_view name) {
  return pressed().count(std::string(name))>0;
  }

void game::keyEvent(const Tempest::KeyEvent& e, bool down) {
  const auto name = keyName(int(e.key));
  if(name.empty())
    return;

  if(down)
    pressed().insert(name); else
    pressed().erase(name);

  auto& host = ModHost::inst();
  const char* event = down ? "key:down" : "key:up";
  if(!host.hasHandlers(event))
    return;
  host.emit(event,[&](JSContext* ctx) {
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx,o,"key",JS_NewString(ctx,name.c_str()));
    return o;
    });
  }

/// input: the keyboard. The events are key:down and key:up.
void game::installInput(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue input  = JS_NewObject(ctx);
  js::fn(ctx,input,"isDown",isDown,1);
  JS_SetPropertyStr(ctx,global,"input",input);
  JS_FreeValue(ctx,global);
  }
