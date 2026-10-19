#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ImGuiInputTextCallbackData;


/// The window of the mods drawn with ImGui. One file draws one tab: modpanel_mods.cpp, modpanel_console.cpp, modpanel_about.cpp.
class ModPanel final {
  public:
    ModPanel();

    /// Draws the window in the frame of ImGui. Returns false when the player has closed it.
    bool draw(float width, float height);

    /// The console gets the focus when the window is opened again
    void reset();

  private:
    void drawMods();
    void drawConsole();
    void drawAbout();

    static int inputCallback(ImGuiInputTextCallbackData* data);
    void       run();

    char                     input[512] = {};
    std::vector<std::string> history;
    int                      historyAt  = 0;
    uint64_t                 seenLog    = 0;
    bool                     focusInput = false;
  };
