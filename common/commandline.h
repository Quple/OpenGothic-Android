#pragma once

#include <Tempest/Platform>
#include <Tempest/Dir>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "game/constants.h"

class VersionInfo;
class GothicNotFoundException : std::logic_error {
  using logic_error::logic_error;
  };

class CommandLine {
  public:
    CommandLine(int argc,const char** argv);
    static const CommandLine& inst();

    enum GraphicBackend : uint8_t {
      Vulkan,
      DirectX12
      };
    auto                graphicsApi() const -> GraphicBackend;
    std::u16string_view rootPath() const;
    std::u16string      scriptPath() const;
    std::u16string      scriptPath(ScriptLang lang) const;
    std::u16string      cutscenePath() const;
    std::u16string      cutscenePath(ScriptLang lang) const;
    std::u16string_view modPath() const { return gmod; }
    std::string         commandLine() const;
    std::string         parameterValue(std::string_view name) const;
    std::u16string      nestedPath(const std::initializer_list<const char16_t*> &name, Tempest::Dir::FileType type) const;

    bool                isDevMode()        const { return devmode;      }
    bool                isValidationMode() const { return isDebug;      }
    bool                isWindowMode()     const { return isWindow;     }
    bool                isRayQuery()       const { return isRQuery;     }
    GiMethod            isRtGi()           const { return isGi;         }
    bool                isMeshShading()    const { return isMeshSh;     }
    bool                isBindless()       const { return isBindlessSh; }
    /// -safegpu: -1 - automatic (the state that the previous start has left), 0 - off, 1 - on, 2 - minimal
    int                 safeGpuMode()      const { return safeGpu;      }
    bool                isVirtualShadow()  const { return isVsm;        }
    bool                isSoftwareShadow() const { return isRtSm;       }
    bool                doStartMenu()      const { return !noMenu;      }
    Benchmark           isBenchmarkMode()  const { return isBenchmark;  }
    bool                doForceG1()        const { return forceG1;      }
    bool                doForceG2()        const { return forceG2;      }
    bool                doForceG2NR()      const { return forceG2NR;    }
    bool                aaPreset()         const { return aaPresetId;   }
    /// the slot of the saved game of the command line (-save 3, -save q is the slot 0), -1: none
    int                 defaultSaveSlot()  const { return saveSlot;   }
    /// the errors of the scripts and of the program are shown on the screen (a phone has no log to read): the default of the touch version
    bool                doShowErrors()     const { return showErrors; }

    std::string         wrldDef;

  private:
    bool                validateGothicPath() const;

    GraphicBackend      graphics = GraphicBackend::Vulkan;
    std::u16string      gpath, gmod;
    std::u16string      gscript;
    std::u16string      gcutscene;
    std::vector<std::string> arguments;
    int                 saveSlot = -1;
#ifdef OPENGOTHIC_TOUCH_UI
    bool                showErrors   = true;
#else
    bool                showErrors   = false;
#endif
    bool                devmode      = false;
    bool                noMenu       = false;
    Benchmark           isBenchmark  = Benchmark::None;
    bool                isWindow     = false;
    bool                isDebug      = false;
#if defined(__OSX__) || defined(__ANDROID__)
    bool                isRQuery     = false;
    bool                isMeshSh     = false;
#else
    bool                isRQuery     = true;
    bool                isMeshSh     = true;
#endif
    bool                isBindlessSh = true;
    int                 safeGpu      = -1;
    bool                isVsm        = false;
    bool                isRtSm       = false;
    GiMethod            isGi         = GiMethod::None;
    bool                forceG1      = false;
    bool                forceG2      = false;
    bool                forceG2NR    = false;
    uint32_t            aaPresetId = 0;
  };

