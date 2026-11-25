#pragma once

#include <cstdint>
#include <initializer_list>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace zenkit {
class DaedalusVm;
}

namespace Compatibility {

/// The machine-readable list of what the scripts of a mod asked for and did not get: a call of the engine that nobody
/// answers, a hook that nobody fires, memory that does not exist, a field of a character that the handle does not have, an
/// external without a binding, an error of a script.
///
/// The first time of every gap is a line of log.txt ("IKGAP {json}"), so it is in the reports of the phones too; the whole
/// list with the counts is ikarus-gaps.json next to log.txt (written at a save, at the end of the game and on request).
/// tools/ikarus/worklist.py turns them, with the static audit and the engine database, into tasks (docs/IKARUS_AUTOMATION.md).
class GapLog {
  public:
    enum class Kind : uint8_t {
      Call,         // CALL__thiscall & co: an address of the engine without a handler (Cpu32)
      Hook,         // HookEngine: a place of the engine that OpenGothic does not fire
      Unmapped,     // MEM_ReadInt & co: an address of the pretended memory where nothing lives
      Field,        // a field of oCNpc/oCItem that the handle does not fill (it reads 0, a write is lost)
      External,     // an external function of the scripts without a binding
      ScriptError,  // an exception of the VM
      };

    static GapLog& inst();

    /// The VM of the game: the functions on its call stack say who asked
    void     setVm(const zenkit::DaedalusVm* vm);

    /// `address`: the address (Call, Hook, Unmapped), the offset in the object (Field); `cls`: the class of the object (Field);
    /// `name`: the external, the error, the functions of a hook; `regs`: ECX and the stack (Call)
    void     note(Kind kind, uint32_t address, std::string_view name = {}, std::string_view cls = {},
                  std::string_view detail = {}, std::initializer_list<uint32_t> regs = {});
    void     note(Kind kind, uint32_t address, std::string_view name, std::string_view cls,
                  std::string_view detail, const std::vector<uint32_t>& regs);

    /// Writes ikarus-gaps.json (all the gaps with their counts)
    void     flush();
    size_t   size() const;
    uint64_t total() const;
    /// The gaps as JSON, for the mods (debug.gaps()) and the autorun
    std::string json() const;

    static std::string_view kindName(Kind k);

  private:
    GapLog() = default;

    struct Entry {
      Kind                     kind    = Kind::Call;
      uint32_t                 address = 0;
      std::string              name, cls, detail;
      std::vector<std::string> stack;
      std::vector<uint32_t>    regs;
      uint64_t                 count   = 0;
      };
    using Key = std::tuple<uint8_t,uint32_t,std::string,std::string>;

    std::string                  entryJson(const Entry& e) const;
    std::vector<std::string>     scriptStack() const;

    mutable std::recursive_mutex sync;
    const zenkit::DaedalusVm*    vm = nullptr;
    std::map<Key,Entry>          entries;
    uint64_t                     all = 0;
    bool                         dirty = false;
    uint32_t                     perKind[8] = {};
    static constexpr size_t      MaxEntries = 4000;
    // a script that walks over memory that does not exist must not push the other gaps out
    static constexpr uint32_t    MaxPerAddressKind = 1000;
  };

}
