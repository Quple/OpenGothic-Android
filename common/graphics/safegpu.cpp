#include "safegpu.h"

#include <Tempest/Log>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>

#include "commandline.h"

using namespace Tempest;

namespace {

const char* const LockFile  = "safegpu.lock";
const char* const ModeFile  = "safegpu.mode";
const char* const CrashFile = "crash.log";

struct State {
  std::mutex        sync;
  std::atomic<int>  level{SafeGpu::Normal};
  std::string       identity;
  std::FILE*        lock   = nullptr;
  int               armed  = 0;
  bool              lockFailed = false;
  };

State& state() {
  static State s;
  return s;
  }

struct Saved {
  bool        valid = false;
  int         level = 0;
  std::string identity;
  std::string shader;
  };

Saved readFile(const char* name) {
  Saved r;
  std::ifstream in(name);
  if(!in)
    return r;
  std::string line;
  while(std::getline(in,line)) {
    while(!line.empty() && (line.back()=='\r' || line.back()=='\n'))
      line.pop_back();
    const auto eq = line.find('=');
    if(eq==std::string::npos)
      continue;
    const std::string key = line.substr(0,eq);
    const std::string val = line.substr(eq+1);
    if(key=="level") {
      try { r.level = std::stoi(val); } catch(...) { r.level = 0; }
      }
    else if(key=="gpu")
      r.identity = val;
    else if(key=="shader")
      r.shader = val;
    }
  r.valid = !r.identity.empty();
  return r;
  }

void removeFile(const char* name) {
  std::error_code ec;
  std::filesystem::remove(name,ec);
  }

void writeMode(const std::string& identity, int level, const std::string& shader) {
  std::ofstream out(ModeFile,std::ios::trunc);
  if(!out) {
    Log::e("Safe GPU mode: unable to write ",ModeFile," (it will be forgotten after the next start)");
    return;
    }
  out << "level=" << level << "\n";
  out << "gpu="   << identity << "\n";
  out << "shader=" << shader << "\n";
  }

/// the crash.log that is newer than the lock: the crash handler has seen the death (not a kill of the application by the user)
bool crashAfterLock() {
  std::error_code ec;
  const auto lockTime  = std::filesystem::last_write_time(LockFile,ec);
  if(ec)
    return false;
  const auto crashTime = std::filesystem::last_write_time(CrashFile,ec);
  if(ec)
    return false;
  return crashTime>=lockTime;
  }

// the lock keeps the name of the shader; the whole file is rewritten (it has to stay small, one write of the system)
void writeLock(State& s, std::string_view shader) {
  if(s.lock==nullptr)
    return;
  std::rewind(s.lock);
  // a record of a fixed size, the rest of it is empty lines: a shorter name does not leave the tail of the previous one
  char buf[512];
  std::fill(std::begin(buf),std::end(buf),'\n');
  std::snprintf(buf,sizeof(buf)-1,"gpu=%s\nshader=%.*s",s.identity.c_str(),int(std::min<size_t>(shader.size(),160)),shader.data());
  buf[std::strlen(buf)] = '\n';
  std::fwrite(buf,1,sizeof(buf),s.lock);
  std::fflush(s.lock);
  }

}

void SafeGpu::init(std::string_view gpuName) {
  auto& s = state();
  std::lock_guard<std::mutex> guard(s.sync);

  s.identity = std::string(gpuName);
  std::replace(s.identity.begin(),s.identity.end(),'\n',' ');

  const int forced = CommandLine::inst().safeGpuMode();
  if(forced==0) {
    removeFile(ModeFile);
    removeFile(LockFile);
    s.level = Normal;
    Log::i("Safe GPU mode: off (command line)");
    return;
    }

  int level = Normal;
  const Saved mode = readFile(ModeFile);
  if(mode.valid) {
    if(mode.identity==s.identity) {
      level = std::clamp(mode.level,int(Normal),int(Minimal));
      } else {
      Log::i("Safe GPU mode: the saved state is of another GPU (",mode.identity.c_str(),"), ignored");
      removeFile(ModeFile);
      }
    }

  const Saved lock = readFile(LockFile);
  if(lock.valid) {
    if(lock.identity==s.identity) {
      // the previous start never finished the compilation of the shaders
      const bool crashed = crashAfterLock();
      Log::e("Safe GPU mode: the previous start died while the driver was compiling the shader '",lock.shader.c_str(),"'",
             crashed ? " (crash.log confirms)" : "");
      if(level==Normal)
        level = Safe; else
      if(level==Safe && crashed)
        level = Minimal;
      writeMode(s.identity,level,lock.shader);
      } else {
      Log::i("Safe GPU mode: the lock is of another GPU (",lock.identity.c_str(),"), ignored");
      }
    removeFile(LockFile);
    }

  if(forced==1)
    level = std::max(level,int(Safe));
  else if(forced==2)
    level = Minimal;

  s.level = level;
  if(level!=Normal)
    Log::e("Safe GPU mode: level ",level," for ",s.identity.c_str(),
           " (-safegpu off turns it off, or delete ",ModeFile,")");
  }

SafeGpu::Level SafeGpu::level() {
  return Level(state().level.load());
  }

void SafeGpu::trace(std::string_view shader) {
  auto& s = state();
  std::lock_guard<std::mutex> guard(s.sync);
  writeLock(s,shader);
  }

SafeGpu::Scope::Scope() {
  auto& s = state();
  std::lock_guard<std::mutex> guard(s.sync);
  if(s.armed++>0)
    return;
  s.lock = std::fopen(LockFile,"wb");
  if(s.lock==nullptr && !s.lockFailed) {
    s.lockFailed = true;
    Log::e("Safe GPU mode: unable to create ",LockFile,", a crash of the driver will not be remembered");
    }
  writeLock(s,"(start)");
  }

SafeGpu::Scope::~Scope() {
  auto& s = state();
  std::lock_guard<std::mutex> guard(s.sync);
  if(--s.armed>0)
    return;
  if(s.lock!=nullptr) {
    std::fclose(s.lock);
    s.lock = nullptr;
    removeFile(LockFile);
    }
  }
