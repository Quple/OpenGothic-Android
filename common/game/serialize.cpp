#include "serialize.h"

#include <cstring>
#include <exception>
#include <limits>
#include <sstream>

#include "savegameheader.h"
#include "world/world.h"
#include "world/fplock.h"
#include "world/waypoint.h"

#include <Tempest/MemReader>
#include <Tempest/MemWriter>
#include <Tempest/Log>
#include <Tempest/Application>

// the messages of the errors: the sizes are 64 bit, string_frm prints 32 bit only
template<class ... Args>
static std::string msgOf(const Args& ... a) {
  std::ostringstream o;
  (o << ... << a);
  return o.str();
  }

size_t Serialize::writeFunc(void* pOpaque, uint64_t file_ofs, const void* pBuf, size_t n) {
  auto& self = *reinterpret_cast<Serialize*>(pOpaque);
  file_ofs += mz_zip_get_archive_file_start_offset(&self.impl);
  if(file_ofs!=self.curOffset) {
    self.impl.m_last_error = MZ_ZIP_FILE_SEEK_FAILED;
    return 0;
    }
  size_t ret = self.fout->write(pBuf,n);
  self.curOffset+=ret;
  return ret;
  }

size_t Serialize::readFunc(void* pOpaque, uint64_t file_ofs, void* pBuf, size_t n) {
  auto& self = *reinterpret_cast<Serialize*>(pOpaque);
  file_ofs += mz_zip_get_archive_file_start_offset(&self.impl);
  if(self.curOffset<file_ofs) {
    size_t diff = size_t(file_ofs-self.curOffset);
    size_t mv   = self.fin->seek(diff);
    self.curOffset += uint64_t(mv);
    if(diff!=mv) {
      self.impl.m_last_error = MZ_ZIP_FILE_SEEK_FAILED;
      return 0;
      }
    }
  if(self.curOffset>file_ofs) {
    size_t diff = size_t(self.curOffset-file_ofs);
    size_t mv   = self.fin->unget(diff);
    self.curOffset -= uint64_t(mv);
    if(diff!=mv) {
      self.impl.m_last_error = MZ_ZIP_FILE_SEEK_FAILED;
      return 0;
      }
    }
  size_t ret = self.fin->read(pBuf,n);
  self.curOffset+=ret;
  return ret;
  }

Serialize::Serialize() {}

//static uint64_t time0 = 0;

Serialize::Serialize(Tempest::ODevice& fout) : fout(&fout) {
  //time0 = Tempest::Application::tickCount();
  entryBuf .reserve(1*1024*1024);
  entryName.reserve(256);

  impl.m_pWrite           = Serialize::writeFunc;
  impl.m_pIO_opaque       = this;
  impl.m_zip_type         = MZ_ZIP_TYPE_USER;
  if(!mz_zip_writer_init_v2(&impl, 0, 0))
    throw std::runtime_error("unable to create game archive");
  }

Serialize::Serialize(Tempest::IDevice& fin) : fin(&fin) {
  entryBuf .resize(1*1024*1024);
  entryName.reserve(256);

  impl.m_pRead            = Serialize::readFunc;
  impl.m_pIO_opaque       = this;
  impl.m_zip_type         = MZ_ZIP_TYPE_USER;
  archiveSize             = fin.size();
  if(!mz_zip_reader_init(&impl, archiveSize, 0)) {
    const auto err = mz_zip_get_error_string(mz_zip_get_last_error(&impl));
    const std::string msg = msgOf("unable to read game archive: ", err, " (archive size ", archiveSize, " bytes)");
    Tempest::Log::e("save-game: ", msg.c_str());
    throw SaveGameError(msg.c_str());
    }
  }

Serialize::~Serialize() noexcept(false) {
  if(fin!=nullptr)
    mz_zip_reader_end(&impl);
  if(fout==nullptr)
    return;
  try {
    closeEntry();
    if(!mz_zip_writer_finalize_archive(&impl) || !fout->flush())
      throw std::runtime_error("unable to finalize game archive");
    }
  catch(...) {
    mz_zip_writer_end(&impl);
    if(std::uncaught_exceptions()==0)
      throw;
    return;
    }
  mz_zip_writer_end(&impl);
  }

std::string_view Serialize::worldName() const {
  if(ctx!=nullptr)
    return ctx->name();
  return "_";
  }

void Serialize::closeEntry() {
  if(fout==nullptr)
    return;
  if(entryBuf.empty())
    return;

  mz_uint level  = entryBuf.size()>256 ? MZ_BEST_SPEED : MZ_NO_COMPRESSION;
  mz_bool status = mz_zip_writer_add_mem(&impl, entryName.c_str(), entryBuf.data(), entryBuf.size(), level);
  entryBuf .clear();
  entryName.clear();
  if(!status)
    throw std::runtime_error("unable to write entry in game archive");
  }

bool Serialize::implSetEntry(std::string_view fname) {
  size_t prefix = 0;
  if(fout!=nullptr) {
    while(prefix<fname.size() && prefix<entryName.size()) {
      if(entryName[prefix]!=fname[prefix])
        break;
      ++prefix;
      }
    }
  closeEntry();
  entryName = fname;
  if(fout!=nullptr) {
    for(size_t i=prefix; i<entryName.size(); ++i) {
      if(entryName[i]=='/' && i+1<entryName.size()) {
        const char prev = entryName[i+1];
        entryName[i+1] = '\0';
        const auto it = outFileList.insert(entryName.c_str());
        if(it.second) {
          mz_bool status = mz_zip_writer_add_mem(&impl, entryName.c_str(), NULL, 0, MZ_NO_COMPRESSION);
          if(!status)
            throw std::runtime_error("unable to allocate entry in game archive");
          }
        entryName[i+1] = prev;
        }
      }
    return true;
    }
  if(fin!=nullptr) {
    // an entry is never larger than the program could have written in memory (the biggest one are the pictures and the Ikarus files)
    static const uint64_t maxEntry = 1024ull*1024ull*1024ull;

    mz_uint32 id = mz_uint32(-1);
    entryExists  = false;
    entryIndex   = mz_uint32(-1);
    entrySize    = 0;
    entryPacked  = 0;
    readOffset   = 0;
    entryBuf.clear();

    if(mz_zip_reader_locate_file_v2(&impl, entryName.c_str(), nullptr, 0, &id)) {
      mz_zip_archive_file_stat stat = {};
      if(!mz_zip_reader_file_stat(&impl,id,&stat)) {
        const std::string msg = msgOf("unable to stat the entry '", entryName.c_str(), "' (index ", id, "): ",
                       mz_zip_get_error_string(mz_zip_get_last_error(&impl)), "; stage '", stage.c_str(), "'");
        Tempest::Log::e("save-game: ", msg.c_str());
        throw SaveGameError(msg.c_str());
        }
      entryExists = true;
      entryIndex  = id;
      entrySize   = stat.m_uncomp_size;
      entryPacked = stat.m_comp_size;

      if(stat.m_uncomp_size>maxEntry) {
        const std::string msg = msgOf("the entry is too large: ", describeEntry());
        Tempest::Log::e("save-game: ", msg.c_str());
        throw SaveGameError(msg.c_str());
        }
      if(stat.m_uncomp_size>0) {
        entryBuf.resize(size_t(stat.m_uncomp_size));
        // the check of the CRC is inside: a damaged entry must not become zeroes that are read as a valid data
        if(!mz_zip_reader_extract_to_mem(&impl,id,entryBuf.data(),entryBuf.size(),0)) {
          const auto err = mz_zip_get_error_string(mz_zip_get_last_error(&impl));
          entryBuf.clear();
          const std::string msg = msgOf("unable to extract the entry: ", err, "; ", describeEntry(),
                         ", archive offset ", curOffset, " of ", archiveSize);
          Tempest::Log::e("save-game: ", msg.c_str());
          throw SaveGameError(msg.c_str());
          }
        }
      }
    return !entryBuf.empty();
    }
  return false;
  }

size_t Serialize::remaining() const {
  if(fin==nullptr || readOffset>=entryBuf.size())
    return 0;
  return entryBuf.size()-size_t(readOffset);
  }

std::string Serialize::describeEntry() const {
  std::string ret = msgOf("stage '", stage, "', entry '", entryName, "'");
  if(entryExists) {
    const std::string d = msgOf(" (index ", entryIndex, ", ", entrySize, " bytes, ", entryPacked, " packed), read offset ", readOffset);
    ret += d;
    } else {
    ret += " (the entry is not in the archive)";
    }
  return ret;
  }

void Serialize::failRead(size_t requested) const {
  const std::string msg = msgOf("unable to read save-game file: ", requested, " bytes requested, only ", remaining(), " left; ",
                 describeEntry(), ", save version ", curVer, "/", wldVer);
  Tempest::Log::e("save-game: ", msg.c_str());
  throw SaveGameError(msg.c_str());
  }

void Serialize::failMissingEntry() const {
  const std::string msg = msgOf("the save-game does not have the entry '", entryName.c_str(), "'; stage '", stage.c_str(),
                 "', save version ", curVer, "/", wldVer, ", ", mz_zip_reader_get_num_files(const_cast<mz_zip_archive*>(&impl)), " entries in the archive");
  Tempest::Log::e("save-game: ", msg.c_str());
  throw SaveGameError(msg.c_str());
  }

void Serialize::checkCount(uint64_t count, size_t itemSize, const char* what) {
  if(itemSize==0)
    itemSize = 1;
  if(count>remaining()/itemSize) {
    const std::string msg = msgOf("damaged size of the data: ", count, " ", what, " of ", itemSize, " bytes, only ", remaining(),
                   " bytes left; ", describeEntry());
    Tempest::Log::e("save-game: ", msg.c_str());
    throw SaveGameError(msg.c_str());
    }
  }

uint32_t Serialize::implDirectorySize(std::string_view e) {
  uint32_t count = 0;
  std::vector<char> filename;
  filename.reserve(256);
  for(mz_uint i=0; i<mz_zip_reader_get_num_files(&impl); ++i) {
    const auto length = mz_zip_reader_get_filename(&impl, i, nullptr, 0);
    if(length==0)
      throw std::runtime_error("unable to locate entry in game archive");
    filename.resize(length);
    if(mz_zip_reader_get_filename(&impl, i, filename.data(), length)!=length)
      throw std::runtime_error("unable to read entry name in game archive");
    std::string_view name(filename.data(), length-1);
    if(name.size()<=e.size() || !name.starts_with(e))
      continue;
    const auto child = name.substr(e.size());
    const auto separator = child.find('/');
    if(separator==std::string_view::npos || separator+1==child.size())
      ++count;
    }
  return count;
  }

void Serialize::writeBytes(const void* buf, size_t sz) {
  if(sz==0)
    return;
  size_t at = entryBuf.size();
  entryBuf.resize(entryBuf.size()+sz);
  std::memcpy(&entryBuf[at],buf,sz);
  }

void Serialize::readBytes(void* buf, size_t sz) {
  if(fin==nullptr)
    throw std::runtime_error("unable to read save-game file: the archive is open for writing");
  if(sz==0)
    return;
  if(sz>remaining())
    failRead(sz);
  std::memcpy(buf,entryBuf.data()+size_t(readOffset),sz);
  readOffset+=sz;
  }

void Serialize::implWrite(const std::string& s) {
  uint32_t sz=uint32_t(s.size());
  implWrite(sz);
  writeBytes(s.data(),sz);
  }

void Serialize::implRead(std::string& s) {
  uint32_t sz=0;
  implRead(sz);
  checkCount(sz,1,"characters");
  s.resize(sz);
  if(sz>0)
    readBytes(&s[0],sz);
  }

void Serialize::implWrite(std::string_view s) {
  uint32_t sz=uint32_t(s.size());
  implWrite(sz);
  writeBytes(s.data(),sz);
  }

void Serialize::implWrite(WeaponState w) {
  implWrite(uint8_t(w));
  }

void Serialize::implRead(WeaponState &w) {
  implRead(reinterpret_cast<uint8_t&>(w));
  }

void Serialize::implWrite(const WayPoint* wptr) {
  implWrite(wptr ? wptr->name : "");
  }

void Serialize::implRead(const WayPoint*& wptr) {
  implRead(tmpStr);
  wptr = ctx==nullptr ? nullptr : ctx->findPoint(tmpStr,false);
  }

void Serialize::implWrite(const ScriptFn& fn) {
  uint32_t v = uint32_t(-1);
  if(fn.ptr<uint64_t(std::numeric_limits<uint32_t>::max()))
    v = uint32_t(fn.ptr);
  implWrite(v);
  }

void Serialize::implRead(ScriptFn& fn) {
  uint32_t v=0;
  implRead(v);
  if(v==std::numeric_limits<uint32_t>::max())
    fn.ptr = size_t(-1); else
    fn.ptr = v;
  }

void Serialize::implWrite(const Npc* npc) {
  uint32_t id = ctx==nullptr ? uint32_t(-1) : ctx->npcId(npc);
  implWrite(id);
  }

void Serialize::implRead(const Npc *&npc) {
  uint32_t id=uint32_t(-1);
  implRead(id);
  npc = ctx==nullptr ? nullptr : ctx->npcById(id);
  }

void Serialize::implWrite(Npc *npc) {
  uint32_t id = ctx==nullptr ? uint32_t(-1) : ctx->npcId(npc);
  implWrite(id);
  }

void Serialize::implRead(Npc *&npc) {
  uint32_t id=uint32_t(-1);
  read(id);
  npc = ctx==nullptr ? nullptr : ctx->npcById(id);
  }

void Serialize::implWrite(Interactive* mobsi) {
  uint32_t id = ctx==nullptr ? uint32_t(-1) : ctx->mobsiId(mobsi);
  implWrite(id);
  }

void Serialize::implRead(Interactive*& mobsi) {
  uint32_t id=uint32_t(-1);
  implRead(id);
  mobsi = ctx==nullptr ? nullptr : ctx->mobsiById(id);
  }

void Serialize::implWrite(const SaveGameHeader& p) {
  p.save(*this);
  }

void Serialize::implRead(SaveGameHeader& p) {
  p = SaveGameHeader(*this);
  }

void Serialize::implWrite(const Tempest::Pixmap& p) {
  implWrite(p, "png");
  }

void Serialize::implWrite(const Tempest::Pixmap& p, const char* ext) {
  std::vector<uint8_t> tmp;
  tmp.reserve(4*1024*1024);
  Tempest::MemWriter w{tmp};
  p.save(w, ext);
  writeBytes(tmp.data(),tmp.size());
  }

void Serialize::implRead(Tempest::Pixmap& p) {
  if(remaining()==0)
    failRead(1);
  Tempest::MemReader r{entryBuf.data()+size_t(readOffset),remaining()};
  p = Tempest::Pixmap(r);
  readOffset += r.cursorPosition();
  }

void Serialize::implWrite(const zenkit::INpc& h) {
  write(uint32_t(h.symbol_index()));
  write(h.id,h.name,h.slot,h.effect,int32_t(h.type));
  write(int32_t(h.flags));
  write(h.attribute,h.hitchance,h.protection,h.damage);
  write(h.damage_type,h.guild,h.level);
  write(h.mission);
  write(h.fight_tactic,h.weapon,h.voice,h.voice_pitch,h.body_mass);
  write(h.daily_routine,h.start_aistate);
  write(h.spawnpoint,h.spawn_delay,h.senses,h.senses_range);
  write(h.aivar);
  write(h.wp,h.exp,h.exp_next,h.lp,h.bodystate_interruptable_override,h.no_focus);
  }

void Serialize::readNpc(zenkit::DaedalusVm& vm, std::shared_ptr<zenkit::INpc>& h) {
  uint32_t instanceSymbol=0;
  read(instanceSymbol);

  auto sym = vm.find_symbol_by_index(instanceSymbol);

  if (sym != nullptr) {
    vm.allocate_instance(h, sym);
    } else {
    Tempest::Log::e("Cannot load serialized NPC ", instanceSymbol, ": Symbol not found.");
    }

  read(h->id,h->name,h->slot,h->effect, reinterpret_cast<int32_t&>(h->type));
  read(reinterpret_cast<int32_t&>(h->flags));
  read(h->attribute,h->hitchance,h->protection,h->damage);
  read(h->damage_type,h->guild,h->level);
  read(h->mission);
  read(h->fight_tactic,h->weapon,h->voice,h->voice_pitch,h->body_mass);
  read(h->daily_routine,h->start_aistate);
  read(h->spawnpoint,h->spawn_delay,h->senses,h->senses_range);
  read(h->aivar);
  read(h->wp,h->exp,h->exp_next,h->lp,h->bodystate_interruptable_override,h->no_focus);
  }

void Serialize::implWrite(const FpLock &fp) {
  fp.save(*this);
  }

void Serialize::implRead(FpLock &fp) {
  fp.load(*this);
  }

void Serialize::implWrite(const zenkit::AnimationSample& i) {
  writeBytes(&i,sizeof(i));
  }

void Serialize::implRead(zenkit::AnimationSample& i) {
  if (version() < 40) {
    read(i.position.x,i.position.y,i.position.z);
    read(i.rotation.x,i.rotation.y,i.rotation.z,i.rotation.w);
    } else {
    readBytes (&i,sizeof(i));
    }
  }
