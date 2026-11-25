#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

/// The last lines of the log of the game, for the menu of the developer (mods/devtools) and the mods. The file log.txt has all of them.
class GameLog {
  public:
    enum Level : uint8_t { Info, Error, Debug };

    struct Line {
      uint64_t    serial = 0;
      Level       level  = Info;
      std::string text;
      };

    /// Called by the output of the log, from any thread
    static void     push(Level level, const char* text);

    /// Lines with a serial after `after`, at most `max`. Returns the serial of the last line of the log.
    static uint64_t since(uint64_t after, size_t max, std::vector<Line>& out);
    static uint64_t serial();

    /// The last `max` lines into `out`, for crash.log: the info lines of log.txt are buffered and a crash loses them.
    /// Does nothing when another thread (or the crashed one) holds the log.
    static void     writeTail(std::ostream& out, size_t max);
  };
