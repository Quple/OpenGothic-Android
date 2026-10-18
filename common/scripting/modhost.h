#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <quickjs.h>

/// JavaScript mods. The host knows nothing about the game, it is told about it by a binder (see ModHost::binder).
/// The objects of the scripts are in the files of the api/ folder, one file for one object.
namespace Scripting {

constexpr const char* HostVersion = "1.0.0";
constexpr int         ApiVersion  = 1;

/// Files of a mod: a folder or a zip file
class ModSource {
  public:
    virtual ~ModSource() = default;

    virtual bool        read  (std::string_view path, std::string& out) const = 0;
    virtual bool        exists(std::string_view path) const = 0;

    /// Opens a folder or a zip file with a mod. Returns null if there is no mod.
    static std::shared_ptr<ModSource> open(const std::filesystem::path& path);
  };

struct Manifest {
  std::string              id;
  std::string              name;
  std::string              version = "1.0.0";
  std::string              author;
  std::string              description;
  std::string              main    = "index.js";
  bool                     module  = false;     // "type": "module"
  std::vector<std::string> game;                // empty: any
  int                      apiVersion = ApiVersion;

  static bool parse(std::string_view json, Manifest& out, std::string& error);
  };

class Mod final {
  public:
    enum class State : uint8_t {
      Stopped,
      Running,
      Failed
      };

    Manifest                     manifest;
    std::filesystem::path        path;
    std::shared_ptr<ModSource>   source;
    State                        state   = State::Stopped;
    bool                         enabled = false;   // started with the game
    std::string                  error;
    uint64_t                     startedAt = 0;
    size_t                       errors    = 0;

    bool isRunning() const { return state==State::Running; }

    // What the scripts of the mod have left in the context. Owned by the host and the objects of api/.
    struct Handler {
      uint32_t id   = 0;
      JSValue  fn   = JS_UNDEFINED;
      bool     once = false;
      };
    struct Timer {
      uint32_t             id       = 0;
      uint64_t             due      = 0;
      uint64_t             interval = 0;
      JSValue              fn       = JS_UNDEFINED;
      std::vector<JSValue> args;
      };
    struct Command {
      JSValue     fn = JS_UNDEFINED;
      std::string help;
      };

    JSContext*                                 ctx = nullptr;
    std::map<std::string,std::vector<Handler>> events;
    std::vector<Timer>                         timers;
    std::map<std::string,Command>              commands;
    std::map<std::string,JSValue>              modules;     // files loaded with require
    JSValue                                    storage      = JS_UNDEFINED;
    bool                                       storageDirty = false;
    uint64_t                                   storageSaved = 0;
    uint32_t                                   nextId       = 1;
  };

class ModHost final {
  public:
    struct LogLine {
      enum Level : uint8_t { Info, Warn, Error, Debug };

      uint64_t    serial = 0;
      Level       level  = Info;
      std::string source;
      std::string text;
      };

    static ModHost& inst();

    /// Folders with mods: every folder with manifest.json or zip file is a mod
    void addRoot(const std::filesystem::path& dir);
    const std::vector<std::filesystem::path>& roots() const { return rootDirs; }

    /// Where mods keep their data and where the list of enabled mods is stored
    void setDataDir(const std::filesystem::path& dir);
    const std::filesystem::path& dataDir() const { return data; }

    /// Mods that are not mentioned in the list of enabled mods are started when it is set
    void setEnabledByDefault(bool on) { enabledByDefault = on; }

    /// Name of the game: G1, G2. Mods can be made for one of them.
    void setGame(std::string_view g) { game = g; }
    const std::string& gameName() const { return game; }

    void setPlatform(std::string_view p) { platform = p; }
    const std::string& platformName() const { return platform; }

    /// Adds objects of the game to the context of a mod
    std::function<void(JSContext*, Mod&)> binder;

    /// Everything that is written by mods and by the host goes here too
    std::function<void(const LogLine&)>   logSink;

    // --- mods
    void scan();
    const std::vector<std::unique_ptr<Mod>>& mods() const { return list; }
    Mod* find(std::string_view id);

    bool start(Mod& mod);
    void stop (Mod& mod);
    bool reset(Mod& mod);
    void setEnabled(Mod& mod, bool on);

    void startEnabled();
    void stopAll();
    size_t runningCount() const;

    // --- frame
    void tick(uint64_t nowMs);
    uint64_t nowMs() const { return now; }

    /// Calls handlers of the event in all running mods. arg creates the argument in the context of a mod.
    using ArgBuilder = std::function<JSValue(JSContext*)>;
    void emit(std::string_view event, const ArgBuilder& arg = {});
    bool hasHandlers(std::string_view event) const;
    /// Calls handlers of one mod
    void emitTo(Mod& mod, const std::string& event, JSValueConst arg, bool hasArg);

    /// Calls a function of a mod, the time of it is limited
    JSValue invoke(Mod& mod, JSValueConst fn, JSValueConst self, int argc, JSValueConst* argv, std::string_view what);
    void    report(Mod& mod, std::string_view what);

    void saveStorage(Mod& mod);

    // --- console
    void execute(std::string_view line);
    std::string evaluate(Mod& mod, std::string_view code, bool& ok);
    std::vector<std::string> commandNames() const;

    void log(LogLine::Level level, std::string_view source, std::string_view text);
    const std::deque<LogLine>& lines() const { return history; }
    uint64_t serial() const { return nextSerial; }
    void clearLog() { history.clear(); }

  private:
    ModHost();
    ~ModHost();
    ModHost(const ModHost&) = delete;

    void   loadEnabled();
    void   saveEnabled() const;
    void   loadStorage(Mod& mod);
    void   release(Mod& mod);
    void   pumpJobs();
    bool   loadMain(Mod& mod);
    void   runTimers(Mod& mod);

    static int          interrupt(JSRuntime*, void* self);
    static char*        normalizeModule(JSContext* ctx, const char* base, const char* name, void* self);
    static JSModuleDef* loadModule(JSContext* ctx, const char* name, void* self);

    JSRuntime*                         rt = nullptr;
    std::vector<std::unique_ptr<Mod>>  list;
    std::vector<std::filesystem::path> rootDirs;
    std::filesystem::path              data;
    std::map<std::string,bool>         enabledState;
    bool                               enabledByDefault = true;
    std::string                        game     = "G2";
    std::string                        platform = "desktop";

    std::deque<LogLine>                history;
    uint64_t                           nextSerial = 1;

    Mod*                               running  = nullptr;
    uint64_t                           deadline = 0;
    uint64_t                           now      = 0;
  };

}
