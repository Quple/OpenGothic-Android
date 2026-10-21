#include "errorscreen.h"

#include <algorithm>
#include <cctype>

#include "utils/gamelog.h"
#include "utils/gthfont.h"
#include "gothic.h"

using namespace Tempest;

namespace {

constexpr size_t MaxPending = 24;
constexpr size_t MaxBlock   = 140;

bool startsWith(const std::string& s, std::string_view p) {
  return s.compare(0,p.size(),p)==0;
  }

/// the lines of the exception of the VM of the scripts that follow its first line: the call stack, the stack, the global variables
bool isDetail(const std::string& line) {
  if(!startsWith(line,"[zenkit] "))
    return false;
  const std::string_view t = std::string_view(line).substr(9);
  if(t.compare(0,7,"-------")==0 || t.compare(0,3,"in ")==0)
    return true;
  if(!t.empty() && std::isdigit(static_cast<unsigned char>(t[0])) && t.find(':')!=std::string_view::npos)
    return true;
  if(t.compare(0,9,"Accessing")==0 || t.compare(0,7,"Parsing")==0)
    return false;
  return t.find(" = ")!=std::string_view::npos;
  }

}

ErrorScreen::ErrorScreen() {
  setFocusPolicy(NoFocus);
  }

ErrorScreen::Parsed ErrorScreen::parse(const std::vector<std::string>& block) {
  Parsed r;
  if(block.empty())
    return r;

  std::string zpy;
  bool        warn = false;
  for(auto& l:block) {
    if(l.find("in MEM_WARN at")!=std::string::npos)
      warn = true;
    if(startsWith(l,"[zpy]: "))
      zpy = l.substr(7);
    }
  if(warn)
    return r;     // the warnings of the scripts are not errors

  const std::string& first = block.front();
  if(startsWith(first,"[zenkit] Internal Exception: "))
    r.title = first.substr(std::string_view("[zenkit] Internal Exception: ").size()); else
    r.title = zpy.empty() ? std::string("Error of the scripts") : zpy;

  for(auto& l:block) {
    if(l=="[start of stacktrace]" || l=="[end of stacktrace]")
      continue;
    if(startsWith(l,"[zenkit] "))
      r.details.push_back(l.substr(9)); else
      r.details.push_back(l);
    }
  return r;
  }

void ErrorScreen::add(Entry e) {
  // an error that was shown is not shown again: the same line of the scripts can fail a hundred times
  std::string key = e.title;
  const auto at = e.text.find("\nin ");
  if(at!=std::string::npos)
    key += e.text.substr(at,e.text.find('\n',at+1)-at);
  if(!seen.insert(key).second)
    return;
  if(pending.size()>=MaxPending)
    return;
  pending.push_back(std::move(e));
  if(!active)
    next();
  }

void ErrorScreen::next() {
  if(pending.empty()) {
    active = false;
    return;
    }
  current = std::move(pending.front());
  pending.pop_front();
  active    = true;
  firstLine = 0;
  ++seenCount;
  update();
  }

void ErrorScreen::flush() {
  if(block.empty())
    return;
  auto p = parse(block);
  block.clear();
  if(p.title.empty())
    return;
  Entry e;
  e.title = p.title;
  for(auto& d:p.details)
    e.text += "\n"+d;
  add(std::move(e));
  }

void ErrorScreen::poll() {
  std::vector<GameLog::Line> lines;
  const uint64_t last = GameLog::since(serial,400,lines);
  if(lines.empty()) {
    serial = std::max(serial,last);
    // the block that nothing follows is complete (the log is quiet)
    if((inBlock || afterBlock) && ++idle>=3) {
      inBlock = afterBlock = false;
      flush();
      }
    return;
    }
  idle   = 0;
  serial = lines.back().serial;

  for(auto& l:lines) {
    const std::string& t = l.text;
    if(afterBlock) {
      // MEM_Error of LeGo: the message comes after the call stack
      afterBlock = false;
      if(startsWith(t,"[zpy]: ")) {
        block.push_back(t);
        flush();
        continue;
        }
      flush();
      }

    if(inBlock) {
      if(startsWith(t,"[zenkit] Internal Exception: ")) {
        flush();
        block.push_back(t);
        continue;
        }
      if(t=="[end of stacktrace]") {
        block.push_back(t);
        inBlock    = false;
        afterBlock = true;
        continue;
        }
      if(t=="[start of stacktrace]" || (isDetail(t) && block.size()<MaxBlock)) {
        block.push_back(t);
        continue;
        }
      inBlock = false;
      flush();
      }

    if(startsWith(t,"[zenkit] Internal Exception: ") || t=="[start of stacktrace]") {
      inBlock = true;
      block.assign(1,t);
      }
    }
  }

void ErrorScreen::report(std::string_view title, std::string_view details) {
  Entry e;
  e.title = std::string(title);
  e.text  = "\n"+std::string(details);
  add(std::move(e));
  }

void ErrorScreen::dismiss() {
  if(!active)
    return;
  next();
  update();
  }

void ErrorScreen::scroll(int lines) {
  firstLine = std::max(0,firstLine+lines);
  update();
  }

void ErrorScreen::keyDownEvent(KeyEvent& e) {
  if(!active) {
    e.ignore();
    return;
    }
  e.accept();
  if(e.key==Event::K_Up) {
    scroll(-3);
    return;
    }
  if(e.key==Event::K_Down) {
    scroll(3);
    return;
    }
  dismiss();
  }

void ErrorScreen::keyUpEvent(KeyEvent& e) {
  e.accept();
  }

void ErrorScreen::mouseDownEvent(MouseEvent& e) {
  if(!active) {
    e.ignore();
    return;
    }
  e.accept();
  dismiss();
  }

void ErrorScreen::mouseWheelEvent(MouseEvent& e) {
  if(!active) {
    e.ignore();
    return;
    }
  e.accept();
  scroll(e.delta>0 ? -3 : 3);
  }

void ErrorScreen::paintEvent(PaintEvent& e) {
  if(!active)
    return;

  Painter p(e);
  const float scale = std::max(1.0f,Gothic::interfaceScale(this)*0.9f);
  const auto& fnt   = Resources::font(scale);
  const int   m     = std::max(12,int(14.f*scale));
  const int   lh    = std::max(1,fnt.pixelSize());

  p.setBrush(Color(0.02f,0.0f,0.0f,0.96f));
  p.drawRect(0,0,w(),h());

  // the head
  p.setBrush(Color(0.55f,0.05f,0.05f,1.f));
  p.drawRect(0,0,w(),lh*2+m);
  p.setBrush(Color(1.f,1.f,1.f,1.f));
  fnt.drawText(p,m,m/2+lh,"ERROR OF THE SCRIPTS - tap or press a key to go on");

  int y = lh*2+m+m/2;
  p.setBrush(Color(1.f,0.75f,0.3f,1.f));
  const int titleH = std::max(lh,fnt.lineCount(w()-m*2,current.title)*lh);
  fnt.drawText(p,m,y+lh,w()-m*2,titleH,current.title,AlignLeft);
  y += titleH+m/2;

  const int footH  = lh+m;
  const int bodyH  = std::max(lh,h()-y-footH);
  const int total  = fnt.lineCount(w()-m*2,current.text);
  const int visible = std::max(1,bodyH/lh);
  const int maxFirst = std::max(0,total-visible);
  if(firstLine>maxFirst)
    firstLine = maxFirst;

  p.setBrush(Color(0.85f,0.85f,0.85f,1.f));
  fnt.drawText(p,m,y+lh,w()-m*2,bodyH,current.text,AlignLeft,firstLine);

  p.setBrush(Color(0.55f,0.55f,0.55f,1.f));
  std::string foot = "log.txt has the same. Error "+std::to_string(seenCount)+(pending.empty() ? std::string() : ", "+std::to_string(pending.size())+" more waiting");
  if(total>visible)
    foot += ". Drag or arrows to scroll ("+std::to_string(firstLine+1)+"-"+std::to_string(std::min(total,firstLine+visible))+" of "+std::to_string(total)+")";
  fnt.drawText(p,m,h()-m/2,foot);
  }
