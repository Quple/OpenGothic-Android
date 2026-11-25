#pragma once

#include <cstdint>
#include <string>
#include <vector>

/// A run of the game without a person: an agent (or a script of CI) starts the game, the commands run once the world is
/// loaded, and the game ends by itself with a report. See docs/IKARUS_AUTOMATION.md.
///
///   Gothic2Notr -g <game> -game:<mod>.ini -nomenu -window -noerrors -autorun "wait 20; comcheck test ikarus; wait 5" -autoexit 300
///
/// The commands are separated by ';'. `wait <seconds>` waits (real time, with the world loaded), `waitfor <text> [seconds]`
/// waits for a line of the console of the mods with the text (since the command before it), `quit` ends the run, `gaps`
/// writes ikarus-gaps.json; any other command goes to the console of the mods (`comcheck ...`, `dlgall`, `eval <mod> <code>`,
/// the commands of the mods). After the last command the run ends. -autoexit <seconds> ends it in any case (default 600),
/// the report then says "timeout".
///
/// The report is autorun-report.json next to log.txt: the result, the commands, the lines of the console of the mods, the
/// errors of the log and the gaps of the compatibility layer (GapLog); ikarus-gaps.json is written too.
class AutoRun {
  public:
    static AutoRun& inst();

    bool active() const { return enabled; }
    /// From the main loop, every frame; `worldReady`: a world is loaded and nothing is loading
    void tick(bool worldReady);

  private:
    AutoRun();
    void finish(const char* result);
    void writeReport(const char* result);

    bool                     enabled   = false;
    bool                     finished  = false;
    std::vector<std::string> commands;
    size_t                   next      = 0;
    uint64_t                 startTime = 0;   // the start of the program
    uint64_t                 readyTime = 0;   // the world was there the first time
    uint64_t                 waitUntil = 0;
    uint64_t                 limitMs   = 600000;
    uint64_t                 logSerial = 0;
    uint64_t                 modSerial = 0;
    uint64_t                 cmdSerial = 0;   // the last line of the console before the last command
    uint64_t                 scanSerial = 0;
    std::string              waitText;        // waitfor
    uint64_t                 waitTextUntil = 0;
    std::vector<std::string> done;
  };
