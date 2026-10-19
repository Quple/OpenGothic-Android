#pragma once

#include <memory>

#include <Tempest/TextureAtlas>
#include <Tempest/Widget>

#include "ui/imgui/imguibackend.h"
#include "ui/modpanel.h"

/// F10: the mods of the game and the console of them. The window is drawn by ImGui, see modpanel*.cpp.
class ModMenu : public Tempest::Widget {
  public:
    explicit ModMenu(Tempest::TextureAtlas& atlas);

    bool isActive() const { return active; }
    void open();
    void close();
    void toggle();

    /// Repaints every frame while the menu is open: ImGui animates and tracks the mouse. Called every frame.
    void refresh();

    void keyDownEvent  (Tempest::KeyEvent&   e) override;
    void keyRepeatEvent(Tempest::KeyEvent&   e) override;
    void keyUpEvent    (Tempest::KeyEvent&   e) override;
    void mouseDownEvent (Tempest::MouseEvent& e) override;
    void mouseUpEvent   (Tempest::MouseEvent& e) override;
    void mouseMoveEvent (Tempest::MouseEvent& e) override;
    void mouseDragEvent (Tempest::MouseEvent& e) override;
    void mouseWheelEvent(Tempest::MouseEvent& e) override;

    // the touch screen: a tap is a click, a finger that moves scrolls
    void mobileTap(float x, float y);
    void mobileTouch(float x, float y, int phase);

  protected:
    void paintEvent(Tempest::PaintEvent& e) override;

  private:
    float unit() const;
    void  fit();
    void  feedKey(const Tempest::KeyEvent& e, bool down);

    ImGuiBackend backend;
    ModPanel     panel;

    bool  active  = false;
    bool  paused  = false;
    float touchY  = 0;
  };
