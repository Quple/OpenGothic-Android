#pragma once

#include <Tempest/Color>
#include <Tempest/Painter>
#include <Tempest/Rect>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "utils/gthfont.h"
#include "resources.h"

/// What the screens of a phone are drawn with: the colors and the shapes of the look that the menus share
namespace touchdraw {

using namespace Tempest;

/// Painter::drawTriangle does not follow the place of the widget as drawRect does: a widget that is not in the corner adds it here
inline Point& origin() {
  static Point o;
  return o;
  }

struct Origin {
  explicit Origin(Point p) { origin() = p; }
  ~Origin() { origin() = Point(); }
  };

inline const Color Dim(0.01f,0.03f,0.06f,0.66f);
inline const Color PanelFill(0.03f,0.07f,0.10f,0.86f);
inline const Color PanelLine(0.58f,0.68f,0.78f,0.26f);
inline const Color Hairline(0.58f,0.68f,0.78f,0.16f);
inline const Color Gold(0.84f,0.70f,0.40f,1.00f);
inline const Color GoldDim(0.84f,0.70f,0.40f,0.38f);
inline const Color Cream(0.98f,0.91f,0.73f,1.00f);
inline const Color White(0.94f,0.96f,0.98f,1.00f);
inline const Color Grey(0.62f,0.68f,0.74f,1.00f);
inline const Color Dark(0.09f,0.07f,0.04f,1.00f);
inline const Color RowSel(0.20f,0.19f,0.12f,0.88f);
inline const Color Alert(0.95f,0.55f,0.45f,1.00f);

inline void bar(Painter& p, int x, int y, int w, int h, const Color& c) {
  if(w<=0 || h<=0)
    return;
  p.setBrush(Brush(c));
  p.drawRect(x,y,w,h);
  }

inline void frame(Painter& p, const Rect& r, int th, const Color& border, const Color& body) {
  bar(p,r.x,r.y,r.w,r.h,border);
  bar(p,r.x+th,r.y+th,r.w-2*th,r.h-2*th,body);
  }

inline void stroke(Painter& p, float x0, float y0, float x1, float y1, float thickness, const Color& c) {
  const float dx  = x1-x0;
  const float dy  = y1-y0;
  const float len = std::max(0.001f,std::sqrt(dx*dx+dy*dy));
  const float nx  = -dy/len*thickness/2;
  const float ny  =  dx/len*thickness/2;
  p.setBrush(Brush(c));
  const float ox = float(origin().x);
  const float oy = float(origin().y);
  p.drawTriangle(ox+x0+nx,oy+y0+ny,0.f,0.f, ox+x0-nx,oy+y0-ny,0.f,0.f, ox+x1+nx,oy+y1+ny,0.f,0.f);
  p.drawTriangle(ox+x1+nx,oy+y1+ny,0.f,0.f, ox+x0-nx,oy+y0-ny,0.f,0.f, ox+x1-nx,oy+y1-ny,0.f,0.f);
  }

/// "<" or ">" of the given height
inline void chevron(Painter& p, float cx, float cy, float h, float th, bool right, const Color& c) {
  const float a = right ? -h*0.28f : h*0.28f;
  const float b = right ?  h*0.28f : -h*0.28f;
  stroke(p,cx+a,cy-h/2,cx+b,cy,th,c);
  stroke(p,cx+b,cy,cx+a,cy+h/2,th,c);
  }

/// a filled circle
inline void disc(Painter& p, float cx, float cy, float r, const Color& c) {
  p.setBrush(Brush(c));
  const int n = 14;
  for(int i=0; i<n; ++i) {
    const float a0 = 6.2831853f*float(i)/float(n);
    const float a1 = 6.2831853f*float(i+1)/float(n);
    const float ox = float(origin().x);
    const float oy = float(origin().y);
    p.drawTriangle(ox+cx,oy+cy,0.f,0.f, ox+cx+std::cos(a0)*r,oy+cy+std::sin(a0)*r,0.f,0.f, ox+cx+std::cos(a1)*r,oy+cy+std::sin(a1)*r,0.f,0.f);
    }
  }

inline void arrow(Painter& p, float x, float cy, float len, float th, const Color& c) {
  stroke(p,x,cy,x+len,cy,th,c);
  stroke(p,x+len-len*0.38f,cy-len*0.34f,x+len,cy,th,c);
  stroke(p,x+len-len*0.38f,cy+len*0.34f,x+len,cy,th,c);
  }

inline GthFont fontOf(bool big, int px, const Color& c) {
  const char* name = big ? "font_old_20_white.tga" : "font_old_10_white.tga";
  const auto& base = Resources::font(name,Resources::FontType::Normal,1.f);
  const float s    = float(std::max(px,6))/float(std::max(1,base.pixelSize()));
  return Resources::font(name,Resources::FontType::Normal,s).tinted(c);
  }

/// the text is cut (...) to fit the width
inline std::string fit(const GthFont& f, std::string s, int width) {
  if(f.textSize(s).w<=width)
    return s;
  while(!s.empty() && f.textSize(s+"...").w>width)
    s.pop_back();
  return s+"...";
  }

inline std::string upper(std::string s) {
  for(auto& c:s)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  return s;
  }

/// text in parts that are joined by a dot (0x1f is the place of it), the font of the game may not have the dot
inline int joinedWidth(const GthFont& f, const std::string& s, int gap) {
  int    w = 0;
  size_t at = 0;
  while(true) {
    const auto end = s.find('\x1f',at);
    w += f.textSize(s.substr(at,end==std::string::npos ? std::string::npos : end-at)).w;
    if(end==std::string::npos)
      break;
    w += gap*2+std::max(2,gap/2);
    at = end+1;
    }
  return w;
  }

inline void drawJoined(Painter& p, const GthFont& f, int x, int baseline, const std::string& s, int gap, const Color& dot) {
  size_t at = 0;
  while(true) {
    const auto end  = s.find('\x1f',at);
    const auto part = s.substr(at,end==std::string::npos ? std::string::npos : end-at);
    f.drawText(p,x,baseline,part);
    x += f.textSize(part).w;
    if(end==std::string::npos)
      break;
    const int d = std::max(2,gap/2);
    bar(p,x+gap,baseline-f.pixelSize()/2-d/2,d,d,dot);
    x += gap*2+d;
    at = end+1;
    }
  }

/// letters with a space between them: the small captions
inline void drawSpaced(Painter& p, const GthFont& f, int x, int baseline, const std::string& s, int spacing) {
  for(char c:s) {
    const char one[2] = {c,'\0'};
    f.drawText(p,x,baseline,one);
    x += f.textSize(one).w+spacing;
    }
  }

}
