#include "gaplog.h"

#include <Tempest/Log>
#include <zenkit/DaedalusVm.hh>

#include <cstdio>
#include <fstream>

using namespace Compatibility;

namespace {
void appendEscaped(std::string& out, std::string_view s) {
  out += '"';
  for(char c:s) {
    const auto u = uint8_t(c);
    switch(c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        if(u<0x20 || u>=0x7F) {
          // the names of the scripts are cp1252 and the hidden symbols have \xFF: JSON gets them as escapes, not UTF-8
          char buf[8];
          std::snprintf(buf,sizeof(buf),"\\u%04x",unsigned(u));
          out += buf;
          } else {
          out += c;
          }
      }
    }
  out += '"';
  }

std::string hex(uint32_t v) {
  char buf[16];
  std::snprintf(buf,sizeof(buf),"0x%08x",unsigned(v));
  return buf;
  }
}

GapLog& GapLog::inst() {
  static GapLog log;
  return log;
  }

void GapLog::setVm(const zenkit::DaedalusVm* v) {
  std::lock_guard<std::recursive_mutex> guard(sync);
  vm = v;
  }

std::string_view GapLog::kindName(Kind k) {
  switch(k) {
    case Kind::Call:        return "call";
    case Kind::Hook:        return "hook";
    case Kind::Unmapped:    return "unmapped";
    case Kind::Field:       return "field";
    case Kind::External:    return "external";
    case Kind::ScriptError: return "script";
    }
  return "?";
  }

std::vector<std::string> GapLog::scriptStack() const {
  std::vector<std::string> ret;
  if(vm==nullptr)
    return ret;
  auto& cs = vm->call_stack();
  for(size_t i=cs.size(); i>0 && ret.size()<10; --i) {
    auto* f = cs[i-1].function;
    if(f!=nullptr)
      ret.emplace_back(f->name());
    }
  return ret;
  }

void GapLog::note(Kind kind, uint32_t address, std::string_view name, std::string_view cls,
                  std::string_view detail, std::initializer_list<uint32_t> regs) {
  note(kind,address,name,cls,detail,std::vector<uint32_t>(regs));
  }

void GapLog::note(Kind kind, uint32_t address, std::string_view name, std::string_view cls,
                  std::string_view detail, const std::vector<uint32_t>& regs) {
  std::lock_guard<std::recursive_mutex> guard(sync);
  ++all;
  // a call or an address is one gap whatever name it has (the name is looked up only the first time)
  const bool named = kind!=Kind::Call && kind!=Kind::Unmapped;
  Key key{uint8_t(kind),address,named ? std::string(name) : std::string(),std::string(cls)};
  auto it = entries.find(key);
  if(it!=entries.end()) {
    it->second.count++;
    dirty = true;
    return;
    }
  if(entries.size()>=MaxEntries)
    return;
  if((kind==Kind::Unmapped || kind==Kind::Field) && ++perKind[uint8_t(kind)]>MaxPerAddressKind)
    return;
  Entry e;
  e.kind    = kind;
  e.address = address;
  e.name    = name;
  e.cls     = cls;
  e.detail  = detail;
  e.stack   = scriptStack();
  e.regs    = regs;
  e.count   = 1;
  auto& ref = entries.emplace(std::move(key),std::move(e)).first->second;
  dirty = true;
  Tempest::Log::i("IKGAP ",entryJson(ref));
  }

std::string GapLog::entryJson(const Entry& e) const {
  std::string s = "{\"kind\":";
  appendEscaped(s,kindName(e.kind));
  if(e.kind==Kind::Field)
    s += ",\"address\":" + std::to_string(e.address); else
    s += ",\"address\":\"" + hex(e.address) + "\"";
  if(!e.name.empty())   { s += ",\"name\":";   appendEscaped(s,e.name);   }
  if(!e.cls.empty())    { s += ",\"class\":";  appendEscaped(s,e.cls);    }
  if(!e.detail.empty()) { s += ",\"detail\":"; appendEscaped(s,e.detail); }
  if(!e.regs.empty()) {
    s += ",\"regs\":[";
    for(size_t i=0; i<e.regs.size(); ++i) {
      if(i>0)
        s += ',';
      s += '"' + hex(e.regs[i]) + '"';
      }
    s += ']';
    }
  s += ",\"stack\":[";
  for(size_t i=0; i<e.stack.size(); ++i) {
    if(i>0)
      s += ',';
    appendEscaped(s,e.stack[i]);
    }
  s += "],\"count\":" + std::to_string(e.count) + "}";
  return s;
  }

std::string GapLog::json() const {
  std::lock_guard<std::recursive_mutex> guard(sync);
  std::string s = "{\"schema\":1,\"total\":" + std::to_string(all) + ",\"gaps\":[\n";
  bool first = true;
  for(auto& [k,e]:entries) {
    if(!first)
      s += ",\n";
    first = false;
    s += entryJson(e);
    }
  s += "\n]}\n";
  return s;
  }

size_t GapLog::size() const {
  std::lock_guard<std::recursive_mutex> guard(sync);
  return entries.size();
  }

uint64_t GapLog::total() const {
  std::lock_guard<std::recursive_mutex> guard(sync);
  return all;
  }

void GapLog::flush() {
  std::lock_guard<std::recursive_mutex> guard(sync);
  if(!dirty)
    return;
  // next to log.txt (the working directory of the game, the files of the application on Android)
  std::ofstream f("ikarus-gaps.json",std::ios::binary|std::ios::trunc);
  if(!f) {
    Tempest::Log::e("GapLog: cannot write ikarus-gaps.json");
    return;
    }
  const auto data = json();
  f.write(data.data(),std::streamsize(data.size()));
  dirty = false;
  }
