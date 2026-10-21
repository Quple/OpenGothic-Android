#pragma once

#include <Tempest/Pixmap>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "savegameheader.h"

/// The saves on the disk. There is no limit of the slots: every file "<game>save_slot_N.sav" is a save,
/// a new one gets the first number that nobody has (0 is the quick save).
class SaveStore final {
  public:
    struct Entry {
      size_t         slot    = 0;
      std::string    file;
      SaveGameHeader hdr;
      int32_t        chapter = 0;    // 0 - the save does not tell (made before the list of the saves)
      int32_t        level   = 0;
      std::string    topic;          // the topic of the journal that has changed last
      int64_t        stamp   = 0;    // when it was made, seconds

      bool           isQuick() const { return slot==0; }
      };

    /// the newest first
    static std::vector<Entry> list();
    /// the file name of the next save
    static std::string        newFile();

    static bool               remove(const Entry& e);
    /// writes the save again, with the new name in the header
    static bool               rename(const Entry& e, std::string_view name);
    static Tempest::Pixmap    preview(const Entry& e);

    /// "Khorinis" for "NEWWORLD.ZEN"
    static std::string        placeName(std::string_view world, bool polish);
    /// "20.10.2026" and "21:34" with 0x1f between them: the screen draws a dot there, the font of the game may not have it
    static std::string        dateText(const SaveGameHeader& hdr);
    /// "23 h 27 min"
    static std::string        playTimeText(const SaveGameHeader& hdr);
  };
