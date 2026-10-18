#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <quickjs.h>

/// Small things the files of the mod system share
namespace Scripting {

std::filesystem::path toPath(std::string_view utf8);
std::string           toUtf8(const std::filesystem::path& path);
std::string           lowerCase(std::string s);
uint64_t              steadyMs();

/// Path inside of a mod: "a/b/c.js". Paths that leave the mod are refused.
bool        normalizePath(std::string_view path, std::string& out);
std::string dirName(std::string_view path);

bool readFile (const std::filesystem::path& path, std::string& out);
bool writeFile(const std::filesystem::path& path, std::string_view data);

/// Reads JSON with a runtime of its own, so the files do not depend on the contexts of the mods
class JsonReader final {
  public:
    explicit JsonReader(std::string_view json);
    ~JsonReader();
    JsonReader(const JsonReader&) = delete;

    bool ok() const { return JS_IsObject(root); }

    bool string (const char* key, std::string& out) const;
    bool integer(const char* key, int& out) const;
    /// a string or an array of strings
    std::vector<std::string> strings(const char* key) const;

  private:
    JSRuntime* rt   = nullptr;
    JSContext* ctx  = nullptr;
    JSValue    root = JS_UNDEFINED;
  };

}
