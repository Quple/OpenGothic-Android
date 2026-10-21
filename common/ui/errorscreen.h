#pragma once

#include <Tempest/Widget>

#include <deque>
#include <set>
#include <string>
#include <string_view>
#include <vector>

/// A full screen that shows an error of the scripts or of the program with all the details (the call stack, the variables), so that a player on a
/// phone, who has no log at hand, can take a photo of the screen and give it to the developer. Every different error is shown once; a tap or a
/// key goes on with the game.
///
/// The errors of the scripts come from the log of the game: a block of the exception of the VM ("Internal Exception: ..." with the call stack)
/// or of MEM_Error of LeGo. The warnings of the scripts (MEM_Warn) and the single lines of the log are not shown.
class ErrorScreen : public Tempest::Widget {
  public:
    ErrorScreen();

    /// the errors of the log since the last call
    void poll();
    /// an error that the program has caught
    void report(std::string_view title, std::string_view details);

    bool isActive() const { return active; }
    void dismiss();
    void scroll(int lines);

    void keyDownEvent(Tempest::KeyEvent& e);
    void keyUpEvent  (Tempest::KeyEvent& e);
    void mouseDownEvent(Tempest::MouseEvent& e);
    void mouseWheelEvent(Tempest::MouseEvent& e);

    /// the lines of a block of the log -> the text of the screen; empty title: it is not an error that is shown (a warning of the scripts)
    struct Parsed {
      std::string              title;
      std::vector<std::string> details;
      };
    static Parsed parse(const std::vector<std::string>& block);

  protected:
    void paintEvent(Tempest::PaintEvent& e);

  private:
    struct Entry {
      std::string title;
      std::string text;     // the details, one line of the log after another
      };

    void add(Entry e);
    void next();
    void flush();

    std::deque<Entry>        pending;
    Entry                    current;
    bool                     active = false;
    int                      firstLine = 0;
    int                      seenCount = 0;
    int                      lastLines = 0;
    std::set<std::string>    seen;

    uint64_t                 serial = 0;
    bool                     inBlock = false;
    bool                     afterBlock = false;
    int                      idle = 0;
    std::vector<std::string> block;
  };
