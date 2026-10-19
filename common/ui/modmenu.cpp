#include "modmenu.h"

#include <algorithm>

#include <Tempest/Event>
#include <Tempest/Painter>

#include "gothic.h"

using namespace Tempest;

ModMenu::ModMenu(TextureAtlas& atlas):backend(atlas) {
  setVisible(false);
  setFocusPolicy(NoFocus);
  }

float ModMenu::unit() const {
  return std::min(float(w())/1000.f,float(h())/540.f)*Gothic::interfaceScale(this);
  }

/// The menu covers the window, a hidden widget is not sized by the layout
void ModMenu::fit() {
  if(auto* parent = owner()) {
    if(w()!=parent->w() || h()!=parent->h())
      resize(parent->w(),parent->h());
    setPosition(0,0);
    }
  }

void ModMenu::open() {
  if(active)
    return;
  active = true;
  setVisible(true);
  fit();
  panel.reset();
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

void ModMenu::refresh() {
  if(!active)
    return;
  fit();
  update();
  }

void ModMenu::paintEvent(PaintEvent& e) {
  if(!active || w()<=0 || h()<=0)
    return;

  backend.setScale(unit());
  backend.newFrame(w(),h());
  const bool keep = panel.draw(float(w()),float(h()));

  Painter p(e);
  p.setBrush(Brush(Color(0.f,0.f,0.f,0.55f)));
  p.drawRect(0,0,w(),h());
  backend.render(p);

  if(!keep)
    close();
  }

// --- keyboard

void ModMenu::feedKey(const KeyEvent& e, bool down) {
  backend.key(e.key,down);
  }

void ModMenu::keyDownEvent(KeyEvent& e) {
  if(!active) {
    e.ignore();
    return;
    }
  e.accept();

  // Escape closes the menu, but first it leaves a field of the text
  if(e.key==Event::K_ESCAPE && !backend.wantsText()) {
    close();
    return;
    }

  feedKey(e,true);
  // the characters, the keys of the touch controls are not characters
  if(e.code>=32 && e.code!=127 && e.code<0x110000 && !(e.modifier&Event::M_Ctrl)) {
    uint32_t c = e.code;
    if((e.modifier&Event::M_Shift) && c>='a' && c<='z')
      c -= 'a'-'A';
    backend.text(c);
    }
  update();
  }

void ModMenu::keyRepeatEvent(KeyEvent& e) {
  keyDownEvent(e);
  }

void ModMenu::keyUpEvent(KeyEvent& e) {
  feedKey(e,false);
  e.accept();
  }

// --- the mouse of the desktop

namespace {
int imguiButton(const MouseEvent& e) {
  return e.button==MouseEvent::ButtonRight ? 1 : e.button==MouseEvent::ButtonMid ? 2 : 0;
  }
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
  backend.mouseMove(float(e.x),float(e.y));
  backend.mouseButton(imguiButton(e),true);
  update();
  }

void ModMenu::mouseUpEvent(MouseEvent& e) {
  backend.mouseMove(float(e.x),float(e.y));
  backend.mouseButton(imguiButton(e),false);
  update();
  }

void ModMenu::mouseMoveEvent(MouseEvent& e) {
  if(!active)
    return;
  backend.mouseMove(float(e.x),float(e.y));
  }

void ModMenu::mouseDragEvent(MouseEvent& e) {
  if(!active)
    return;
  backend.mouseMove(float(e.x),float(e.y));
  }

void ModMenu::mouseWheelEvent(MouseEvent& e) {
  if(!active) {
    e.ignore();
    return;
    }
  backend.mouseMove(float(e.x),float(e.y));
  backend.mouseWheel(0.f,float(e.delta)/120.f);
  update();
  }

// --- the touch screen

void ModMenu::mobileTap(float x, float y) {
  if(!active)
    return;
  backend.mouseMove(x,y);
  backend.mouseButton(0,true);
  backend.mouseButton(0,false);
  update();
  }

void ModMenu::mobileTouch(float x, float y, int phase) {
  if(!active)
    return;
  if(phase==0) {
    backend.mouseMove(x,y);
    touchY = y;
    return;
    }
  if(phase!=1)
    return;

  // a finger that only trembles is a tap
  const float dy = y-touchY;
  if(std::abs(dy)<float(h())*0.01f)
    return;
  backend.mouseMove(x,y);
  backend.scrollByPixels(dy);
  touchY = y;
  update();
  }
