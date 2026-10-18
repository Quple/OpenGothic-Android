#pragma once

#include <string>
#include <vector>

#include <Tempest/Widget>

#include "scripting/modhost.h"

/// F10: the mods of the game and the console of them
class ModMenu : public Tempest::Widget {
  public:
    ModMenu();

    bool isActive() const { return active; }
    void open();
    void close();
    void toggle();

    /// Repaints when the console has something new. Called every frame.
    void refresh();

    void keyDownEvent  (Tempest::KeyEvent&   e) override;
    void keyRepeatEvent(Tempest::KeyEvent&   e) override;
    void mouseDownEvent(Tempest::MouseEvent& e) override;
    void mouseWheelEvent(Tempest::MouseEvent& e) override;

    // the touch screen: a tap and a finger that scrolls
    void mobileTap(float x, float y);
    void mobileTouch(float x, float y, int phase);

  protected:
    void paintEvent(Tempest::PaintEvent& e) override;

  private:
    enum class Tab : uint8_t { Mods, Console };

    enum class Action : uint8_t { None, Close, TabMods, TabConsole, Rescan, Toggle, Reset, Auto, Select };

    struct Hit {
      Action action = Action::None;
      int    index  = -1;
      };
    struct Layout {
      Tempest::Rect panel, tabMods, tabConsole, close, body, rescan, input;
      int           rowH = 0;
      };

    void   fit();
    Layout layout() const;
    float  unit() const;
    Hit    hitTest(int x, int y) const;
    void   perform(const Hit& hit);
    void   type(const Tempest::KeyEvent& e);
    void   run();

    int    modsContent(const Layout& l) const;
    Tempest::Rect rowButton(const Layout& l, size_t row, int button, int scroll) const;

    bool                      active = false;
    bool                      paused = false;
    Tab                       tab    = Tab::Mods;
    int                       selected = 0;
    float                     modsScroll = 0;
    float                     logScroll  = 0;   // pixels from the newest line
    uint64_t                  seen = 0;

    std::string               input;
    std::vector<std::string>  history;
    size_t                    historyAt = 0;

    int                       drag = 0;         // 1 - mods, 2 - console
    float                     startY = 0, startScroll = 0;
  };
