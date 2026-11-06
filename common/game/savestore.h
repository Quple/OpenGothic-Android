#pragma once

#include <Tempest/Pixmap>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "savegameheader.h"

/// The saves on the disk. There is no limit of the slots: every file "<game>save_slot_N.sav" is a save,
/// a new one gets the first number that nobody has.
/// The quick saves are kQuickSlots files used in turn: 0 (the file of the older versions) and
/// kQuickBase+1 ... kQuickBase+kQuickSlots-1. The next quick save overwrites the oldest of them, so a broken
/// file never leaves the player without a quick save.
class SaveStore final {
  public:
    static constexpr size_t kQuickSlots = 5;
    static constexpr size_t kQuickBase  = 1000000;

    /// slot -> 0..kQuickSlots-1, or -1 when the slot is not a quick one
    static int                quickIndex(size_t slot);

    struct Entry {
      size_t         slot    = 0;
      std::string    file;
      SaveGameHeader hdr;
      int32_t        chapter = 0;    // 0 - the save does not tell (made before the list of the saves)
      int32_t        level   = 0;
      std::string    topic;          // the topic of the journal that has changed last
      int64_t        stamp   = 0;    // when it was made, seconds

      bool           isQuick() const { return quickIndex(slot)>=0; }
      /// 1..kQuickSlots, 0 for a normal save
      int            quickNumber() const { return quickIndex(slot)+1; }
      };

    /// the newest first
    static std::vector<Entry> list();
    /// the file name of the next save
    static std::string        newFile();
    /// the file name for the next quick save: a free (or broken) quick slot, otherwise the oldest one
    static std::string        nextQuickFile();
    /// the newest quick save that can be read, empty when there is none
    static std::string        newestQuickFile();

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
