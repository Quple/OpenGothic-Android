#include "modmenu.h"

#include <algorithm>
#include <cmath>

#include <Tempest/Application>
#include <Tempest/Event>
#include <Tempest/Painter>

#include "scripting/modhost.h"
#include "utils/gthfont.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;
using namespace Scripting;

namespace {

const Color Gold    (0.82f,0.66f,0.34f,0.95f);
const Color Hairline(0.60f,0.64f,0.70f,0.28f);

void fill(Painter& p, const Rect& r, const Color& c) {
  p.setBrush(Brush(c));
  p.drawRect(r.x,r.y,r.w,r.h);
  }

void frame(Painter& p, const Rect& r, int th, const Color& border, const Color& body) {
  fill(p,r,border);
  fill(p,Rect(r.x+th,r.y+th,r.w-2*th,r.h-2*th),body);
  }

/// The text, vertically in the middle of the rect
void centered(Painter& p, const GthFont& f, const Rect& r, const std::string& txt) {
  const auto sz = f.textSize(txt);
  f.drawText(p,r.x+(r.w-sz.w)/2,r.y+(r.h-sz.h)/2+f.pixelSize(),txt);
  }

std::string utf8(uint32_t c) {
  std::string s;
  if(c<0x80) {
    s.push_back(char(c));
    } else
  if(c<0x800) {
    s.push_back(char(0xC0|(c>>6)));
    s.push_back(char(0x80|(c&0x3F)));
    } else
  if(c<0x10000) {
    s.push_back(char(0xE0|(c>>12)));
    s.push_back(char(0x80|((c>>6)&0x3F)));
    s.push_back(char(0x80|(c&0x3F)));
    }
  return s;
  }

}

ModMenu::ModMenu() {
  setVisible(false);
  setFocusPolicy(NoFocus);
  }

float ModMenu::unit() const {
  return std::min(float(w())/1000.f,float(h())/540.f)*Gothic::interfaceScale(this);
  }

void ModMenu::open() {
  if(active)
    return;
  active = true;
  setVisible(true);
  fit();
  ModHost::inst().scan();
  seen = 0;
  logScroll = 0;
  if(!paused) {
    Gothic::inst().pushPause();
    paused = true;
    }
  update();
  }

void ModMenu::close() {
  if(!active)
    return;
  active = false;
  drag   = 0;
  setVisible(false);
  if(paused) {
    Gothic::inst().popPause();
    paused = false;
    }
  update();
  }

void ModMenu::toggle() {
  if(active)
    close(); else
    open();
  }

/// The menu covers the window, a hidden widget is not sized by the layout
void ModMenu::fit() {
  if(auto* parent = owner()) {
    if(w()!=parent->w() || h()!=parent->h())
      resize(parent->w(),parent->h());
    setPosition(0,0);
    }
  }

void ModMenu::refresh() {
  if(!active)
    return;
  fit();
  const auto s = ModHost::inst().serial();
  if(s!=seen) {
    seen = s;
    update();
    }
  }

// --- layout

ModMenu::Layout ModMenu::layout() const {
  Layout l;
  const int W   = w();
  const int H   = h();
  const int m   = int(float(std::min(W,H))*0.03f);
  const int pad = int(14*unit());
  const int th  = int(46*unit());

  l.panel      = Rect(m,m,W-2*m,H-2*m);
  l.tabMods    = Rect(l.panel.x+pad,l.panel.y+pad,int(190*unit()),th);
  l.tabConsole = Rect(l.tabMods.x+l.tabMods.w+pad,l.tabMods.y,int(190*unit()),th);
  l.close      = Rect(l.panel.x+l.panel.w-pad-th,l.tabMods.y,th,th);

  const int top = l.tabMods.y+th+pad;
  l.body   = Rect(l.panel.x+pad,top,l.panel.w-2*pad,l.panel.y+l.panel.h-pad-top);
  l.rescan = Rect(l.body.x,l.body.y+l.body.h-th,int(190*unit()),th);
  l.input  = Rect(l.body.x,l.body.y+l.body.h-th,l.body.w,th);
  l.rowH   = int(78*unit());
  return l;
  }

int ModMenu::modsContent(const Layout& l) const {
  return int(ModHost::inst().mods().size())*l.rowH;
  }

/// buttons of a row: 0 - start/stop, 1 - reset, 2 - starts with the game
Rect ModMenu::rowButton(const Layout& l, size_t row, int button, int scroll) const {
  const int bw  = int(118*unit());
  const int bh  = int(40*unit());
  const int gap = int(8*unit());
  const int x   = l.body.x+l.body.w-(3-button)*(bw+gap);
  const int y   = l.body.y+int(row)*l.rowH-scroll+(l.rowH-bh)/2;
  return Rect(x,y,bw,bh);
  }

ModMenu::Hit ModMenu::hitTest(int x, int y) const {
  const auto l = layout();
  if(l.close.contains(x,y))
    return {Action::Close,-1};
  if(l.tabMods.contains(x,y))
    return {Action::TabMods,-1};
  if(l.tabConsole.contains(x,y))
    return {Action::TabConsole,-1};

  if(tab==Tab::Mods) {
    if(l.rescan.contains(x,y))
      return {Action::Rescan,-1};

    const Rect list(l.body.x,l.body.y,l.body.w,l.body.h-l.rescan.h-int(10*unit()));
    if(!list.contains(x,y))
      return {};
    const int scroll = int(modsScroll);
    const auto& mods = ModHost::inst().mods();
    for(size_t i=0; i<mods.size(); ++i) {
      for(int b=0; b<3; ++b) {
        if(rowButton(l,i,b,scroll).contains(x,y))
          return {b==0 ? Action::Toggle : b==1 ? Action::Reset : Action::Auto,int(i)};
        }
      const Rect row(l.body.x,l.body.y+int(i)*l.rowH-scroll,l.body.w,l.rowH);
      if(row.contains(x,y))
        return {Action::Select,int(i)};
      }
    }
  return {};
  }

void ModMenu::perform(const Hit& hit) {
  auto& host = ModHost::inst();
  const auto& mods = host.mods();
  Mod* mod = hit.index>=0 && size_t(hit.index)<mods.size() ? mods[size_t(hit.index)].get() : nullptr;

  switch(hit.action) {
    case Action::None:
      break;
    case Action::Close:
      close();
      return;
    case Action::TabMods:
      tab = Tab::Mods;
      break;
    case Action::TabConsole:
      tab = Tab::Console;
      logScroll = 0;
      break;
    case Action::Rescan:
      host.scan();
      break;
    case Action::Select:
      selected = hit.index;
      break;
    case Action::Toggle:
      selected = hit.index;
      if(mod!=nullptr) {
        if(mod->isRunning())
          host.stop(*mod); else
          host.start(*mod);
        }
      break;
    case Action::Reset:
      selected = hit.index;
      if(mod!=nullptr)
        host.reset(*mod);
      break;
    case Action::Auto:
      selected = hit.index;
      if(mod!=nullptr)
        host.setEnabled(*mod,!mod->enabled);
      break;
    }
  update();
  }

// --- input

void ModMenu::run() {
  if(input.empty())
    return;
  ModHost::inst().execute(input);
  if(history.empty() || history.back()!=input)
    history.push_back(input);
  historyAt = history.size();
  input.clear();
  logScroll = 0;
  }

void ModMenu::type(const KeyEvent& e) {
  if(e.code<32 || e.code==127)
    return;
  if(e.code>=0x110000)
    return;   // keys of the touch controls
  if(input.size()<400)
    input += utf8(e.code);
  }

void ModMenu::keyDownEvent(KeyEvent& e) {
  if(!active) {
    e.ignore();
    return;
    }
  e.accept();
  auto& host = ModHost::inst();

  if(e.key==Event::K_ESCAPE || e.key==Event::K_F10) {
    close();
    return;
    }
  if(e.key==Event::K_Tab) {
    tab = tab==Tab::Mods ? Tab::Console : Tab::Mods;
    update();
    return;
    }

  if(tab==Tab::Mods) {
    const int count = int(host.mods().size());
    if(e.key==Event::K_Up && selected>0)
      --selected;
    if(e.key==Event::K_Down && selected+1<count)
      ++selected;
    if(selected<count && selected>=0) {
      Mod& mod = *host.mods()[size_t(selected)];
      if(e.key==Event::K_Return) {
        if(mod.isRunning())
          host.stop(mod); else
          host.start(mod);
        }
      if(e.key==Event::K_R)
        host.reset(mod);
      if(e.key==Event::K_E)
        host.setEnabled(mod,!mod.enabled);
      }
    update();
    return;
    }

  // the console
  if(e.key==Event::K_Return) {
    run();
    } else
  if(e.key==Event::K_Back) {
    // a character can have several bytes
    while(!input.empty() && (static_cast<unsigned char>(input.back())&0xC0)==0x80)
      input.pop_back();
    if(!input.empty())
      input.pop_back();
    } else
  if(e.key==Event::K_Up) {
    if(historyAt>0 && !history.empty())
      input = history[--historyAt];
    } else
  if(e.key==Event::K_Down) {
    if(historyAt+1<history.size())
      input = history[++historyAt]; else {
      historyAt = history.size();
      input.clear();
      }
    } else
  if(e.key==Event::K_PageUp) {
    logScroll += float(h())*0.4f;
    } else
  if(e.key==Event::K_PageDown) {
    logScroll = std::max(0.f,logScroll-float(h())*0.4f);
    } else {
    type(e);
    }
  update();
  }

void ModMenu::keyRepeatEvent(KeyEvent& e) {
  keyDownEvent(e);
  }

void ModMenu::mouseDownEvent(MouseEvent& e) {
#ifdef OPENGOTHIC_TOUCH_UI
  e.ignore();   // the touches are taken by the controls, see mobileTap
  return;
#endif
  if(!active) {
    e.ignore();
    return;
    }
  e.accept();
  if(e.button==MouseEvent::ButtonLeft)
    perform(hitTest(e.x,e.y));
  }

void ModMenu::mouseWheelEvent(MouseEvent& e) {
  if(!active) {
    e.ignore();
    return;
    }
  const float step = float(e.delta)/120.f*60.f*unit();
  if(tab==Tab::Mods)
    modsScroll = std::max(0.f,modsScroll-step); else
    logScroll  = std::max(0.f,logScroll+step);
  update();
  }

void ModMenu::mobileTap(float x, float y) {
  if(active)
    perform(hitTest(int(x),int(y)));
  }

void ModMenu::mobileTouch(float, float y, int phase) {
  if(!active)
    return;
  if(phase==0) {
    drag        = tab==Tab::Mods ? 1 : 2;
    startY      = y;
    startScroll = tab==Tab::Mods ? modsScroll : logScroll;
    return;
    }
  if(phase>=2 || drag==0) {
    drag = 0;
    return;
    }

  // a finger that only trembles is a tap
  const float dead = float(h())*0.012f;
  float dy = startY-y;
  dy = dy>dead ? dy-dead : dy<-dead ? dy+dead : 0.f;
  if(drag==1)
    modsScroll = std::max(0.f,startScroll+dy); else
    logScroll  = std::max(0.f,startScroll-dy);
  update();
  }

// --- painting

void ModMenu::paintEvent(PaintEvent& e) {
  if(!active)
    return;

  Painter p(e);
  auto&       host = ModHost::inst();
  const float u    = unit();
  const auto  l    = layout();
  const int   pad  = int(14*u);
  const auto& fn   = Resources::font(Resources::FontType::Normal,u*0.8f);
  const auto& fh   = Resources::font(Resources::FontType::Hi,u*0.8f);
  const auto& fd   = Resources::font(Resources::FontType::Disabled,u*0.75f);
  const auto& fr   = Resources::font(Resources::FontType::Red,u*0.8f);

  fill(p,Rect(0,0,w(),h()),Color(0.f,0.f,0.f,0.55f));
  frame(p,l.panel,std::max(2,int(2*u)),Color(0.62f,0.50f,0.28f,0.95f),Color(0.02f,0.025f,0.035f,0.96f));

  // tabs
  auto drawTab = [&](const Rect& r, const char* name, bool on) {
    frame(p,r,1,on ? Gold : Hairline,Color(0.04f,0.045f,0.06f,0.9f));
    centered(p,on ? fh : fn,r,name);
    };
  drawTab(l.tabMods,"Mods",tab==Tab::Mods);
  drawTab(l.tabConsole,"Console",tab==Tab::Console);

  frame(p,l.close,1,Hairline,Color(0.04f,0.045f,0.06f,0.9f));
  centered(p,fn,l.close,"X");

  const std::string title = std::string("GothicJS ")+HostVersion+"   "+std::to_string(host.runningCount())+"/"+
                            std::to_string(host.mods().size())+" running";
  fd.drawText(p,l.tabConsole.x+l.tabConsole.w+2*pad,l.tabMods.y+(l.tabMods.h+fd.pixelSize())/2,title);

  if(tab==Tab::Mods) {
    const Rect list(l.body.x,l.body.y,l.body.w,l.body.h-l.rescan.h-int(10*u));
    const auto& mods = host.mods();

    const float maxScroll = float(std::max(0,modsContent(l)-list.h));
    modsScroll = std::clamp(modsScroll,0.f,maxScroll);
    const int scroll = int(modsScroll);

    p.setScissor(list.x,list.y,list.w,list.h);
    for(size_t i=0; i<mods.size(); ++i) {
      const Mod& m = *mods[i];
      const int  y = l.body.y+int(i)*l.rowH-scroll;
      if(y+l.rowH<list.y || y>list.y+list.h)
        continue;

      if(int(i)==selected)
        fill(p,Rect(l.body.x,y,l.body.w,l.rowH-int(4*u)),Color(0.12f,0.09f,0.04f,0.55f));

      const char* st = m.state==Mod::State::Running ? "running" : m.state==Mod::State::Failed ? "failed" : "stopped";
      const auto& nameFont = int(i)==selected ? fh : fn;
      const std::string head = m.manifest.name+"  v"+m.manifest.version+(m.manifest.author.empty() ? "" : "  by "+m.manifest.author);
      nameFont.drawText(p,l.body.x+pad,y+int(26*u),head);

      const std::string line2 = std::string(st)+(m.enabled ? "   starts with the game" : "")+
                                (m.errors>0 ? "   errors: "+std::to_string(m.errors) : "");
      (m.state==Mod::State::Failed ? fr : fd).drawText(p,l.body.x+pad,y+int(54*u),line2);
      if(!m.error.empty() && m.state==Mod::State::Failed) {
        const auto firstLine = m.error.substr(0,m.error.find('\n'));
        fr.drawText(p,l.body.x+pad,y+int(70*u),firstLine.substr(0,90));
        }

      const char* labels[3] = {m.isRunning() ? "Stop" : "Start","Reset",m.enabled ? "Auto: on" : "Auto: off"};
      for(int b=0; b<3; ++b) {
        const auto r = rowButton(l,i,b,scroll);
        frame(p,r,1,b==2 && m.enabled ? Gold : Hairline,Color(0.04f,0.045f,0.06f,0.9f));
        centered(p,fn,r,labels[b]);
        }
      fill(p,Rect(l.body.x,y+l.rowH-int(3*u),l.body.w,1),Hairline);
      }
    p.setScissor(0,0,w(),h());

    if(mods.empty()) {
      std::string where = host.roots().empty() ? "mods/" : host.roots().front().string();
      fd.drawText(p,l.body.x+pad,l.body.y+int(40*u),"There are no mods. Put folders or zip files into");
      fn.drawText(p,l.body.x+pad,l.body.y+int(70*u),where);
      }

    frame(p,l.rescan,1,Hairline,Color(0.04f,0.045f,0.06f,0.9f));
    centered(p,fn,l.rescan,"Rescan");
    return;
    }

  // console
  const Rect area(l.body.x,l.body.y,l.body.w,l.body.h-l.input.h-int(8*u));
  fill(p,area,Color(0.f,0.f,0.f,0.35f));

  const int   tw   = area.w-2*pad;
  const auto& lines = host.lines();
  int total = 0;
  std::vector<int> heights;
  heights.reserve(lines.size());
  for(auto& ln:lines) {
    const std::string txt = "["+ln.source+"] "+ln.text;
    const int hh = fn.textSize(tw,txt).h+int(4*u);
    heights.push_back(hh);
    total += hh;
    }
  logScroll = std::clamp(logScroll,0.f,float(std::max(0,total-area.h)));

  p.setScissor(area.x,area.y,area.w,area.h);
  int y = area.y+area.h+int(logScroll);
  for(size_t i=lines.size(); i>0; ) {
    --i;
    y -= heights[i];
    if(y>area.y+area.h)
      continue;
    if(y+heights[i]<area.y)
      break;
    const auto& ln = lines[i];
    const auto& f  = ln.level==ModHost::LogLine::Error ? fr : ln.level==ModHost::LogLine::Warn ? fh :
                     ln.level==ModHost::LogLine::Debug ? fd : fn;
    f.drawText(p,area.x+pad,y+f.pixelSize()+int(2*u),tw,0,"["+ln.source+"] "+ln.text,AlignLeft);
    }
  p.setScissor(0,0,w(),h());

  frame(p,l.input,1,Gold,Color(0.03f,0.035f,0.05f,0.95f));
  const std::string shown = "> "+input+((Application::tickCount()/500)%2==0 ? "_" : "");
  fh.drawText(p,l.input.x+pad,l.input.y+(l.input.h+fh.pixelSize())/2,shown);
  }
