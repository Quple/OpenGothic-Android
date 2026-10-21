#pragma once

#include <Tempest/Platform>
#include <string>

#ifdef __ANDROID__
#include <filesystem>
#endif

class InstallDetect final {
  public:
    InstallDetect();

    std::u16string detectG2();
#if defined(__OSX__) || defined(__IOS__)
    static std::u16string applicationSupportDirectory();
#elif defined(__ANDROID__)
    static std::filesystem::path androidInternalDataPath();
    /// The ini of the mod that the launcher has chosen (the second line of game.txt): "-" is the game with no mod, empty is not said
    static std::string           androidModIni();
#endif

  private:
    std::u16string detectG2(std::u16string pfiles);

#ifdef __WINDOWS__
    static std::u16string programFiles(bool x86);
    std::u16string pfiles, pfilesX86;
#endif

#if defined(__OSX__) || defined(__IOS__)
    std::u16string appDir;
#endif
  };
