#include "netlink.h"

#include <algorithm>
#include <cstring>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <ifaddrs.h>
#  include <net/if.h>
#  include <arpa/inet.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

using namespace Scripting::net;

namespace {

#ifdef _WIN32
using Sock = SOCKET;
constexpr Sock Invalid = INVALID_SOCKET;

// Winsock: char* buffers, int sizes
using Bytes   = const char*;
using Chunk   = int;
using AddrLen = int;

int  lastError()           { return WSAGetLastError(); }
bool wouldBlock(int e)     { return e==WSAEWOULDBLOCK || e==WSAEINPROGRESS || e==WSAEALREADY; }
void closeSock(Sock s)     { closesocket(s); }
bool setNonBlocking(Sock s) {
  u_long on = 1;
  return ioctlsocket(s,FIONBIO,&on)==0;
  }

/// Winsock must be started once for the process
struct Startup {
  Startup()  { WSADATA d; started = WSAStartup(MAKEWORD(2,2),&d)==0; }
  ~Startup() { if(started) WSACleanup(); }
  bool started = false;
  };
bool ensureStarted() {
  static Startup s;
  return s.started;
  }
#else
using Sock = int;
constexpr Sock Invalid = -1;

// POSIX (Android too): void* buffers, size_t sizes, socklen_t for addresses
using Bytes   = const void*;
using Chunk   = size_t;
using AddrLen = socklen_t;

int  lastError()           { return errno; }
bool wouldBlock(int e)     { return e==EWOULDBLOCK || e==EAGAIN || e==EINPROGRESS || e==EALREADY || e==EINTR; }
void closeSock(Sock s)     { ::close(s); }
bool setNonBlocking(Sock s) {
  const int f = fcntl(s,F_GETFL,0);
  return f>=0 && fcntl(s,F_SETFL,f|O_NONBLOCK)==0;
  }
bool ensureStarted() { return true; }
#endif

#ifndef MSG_NOSIGNAL
#  define MSG_NOSIGNAL 0
#endif

/// send and recv with the types of the platform: the sizes and the result are the same for the callers
int sendBytes(Sock s, const char* data, size_t size) {
  return int(::send(s,static_cast<Bytes>(data),Chunk(size),MSG_NOSIGNAL));
  }

int recvBytes(Sock s, char* data, size_t size) {
  return int(::recv(s,data,Chunk(size),0));
  }

std::string errorText(int code) {
  return "error "+std::to_string(code);
  }

void tune(Sock s) {
  int on = 1;
  setsockopt(s,IPPROTO_TCP,TCP_NODELAY,reinterpret_cast<const char*>(&on),sizeof(on));
#ifdef SO_NOSIGPIPE
  setsockopt(s,SOL_SOCKET,SO_NOSIGPIPE,reinterpret_cast<const char*>(&on),sizeof(on));
#endif
  }

std::string addressOf(const sockaddr* sa) {
  char buf[64] = {};
  if(sa->sa_family==AF_INET) {
    auto* in = reinterpret_cast<const sockaddr_in*>(sa);
    inet_ntop(AF_INET,&in->sin_addr,buf,sizeof(buf));
    return std::string(buf)+":"+std::to_string(ntohs(in->sin_port));
    }
  if(sa->sa_family==AF_INET6) {
    auto* in = reinterpret_cast<const sockaddr_in6*>(sa);
    inet_ntop(AF_INET6,&in->sin6_addr,buf,sizeof(buf));
    return std::string("[")+buf+"]:"+std::to_string(ntohs(in->sin6_port));
    }
  return "?";
  }

/// Whether a socket that is connecting is ready: 1 yes, 0 not yet, -1 the connection has failed
int connectState(Sock s, int& error) {
  fd_set wr, ex;
  FD_ZERO(&wr);
  FD_ZERO(&ex);
  FD_SET(s,&wr);
  FD_SET(s,&ex);
  timeval tv = {0,0};
  const int r = select(int(s)+1,nullptr,&wr,&ex,&tv);
  if(r<0) {
    error = lastError();
    return -1;
    }
  if(r==0)
    return 0;

  int       err = 0;
  socklen_t len = sizeof(err);
  if(getsockopt(s,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&err),&len)!=0)
    err = lastError();
  if(err!=0 || FD_ISSET(s,&ex)) {
    error = err;
    return -1;
    }
  return 1;
  }

}


std::vector<std::string> Hub::localAddresses() {
  std::vector<std::string> ret;
  auto add = [&](const std::string& a) {
    if(a.empty() || a.rfind("127.",0)==0 || a=="0.0.0.0" || a.rfind("169.254.",0)==0)
      return;
    if(std::find(ret.begin(),ret.end(),a)==ret.end())
      ret.push_back(a);
    };
  if(!ensureStarted())
    return ret;

  // the address that the system would use to reach the Internet: nothing is sent, the socket only chooses the interface
  {
    Sock s = ::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(s!=Invalid) {
      sockaddr_in to = {};
      to.sin_family = AF_INET;
      to.sin_port   = htons(53);
      inet_pton(AF_INET,"8.8.8.8",&to.sin_addr);
      if(::connect(s,reinterpret_cast<sockaddr*>(&to),sizeof(to))==0) {
        sockaddr_in me  = {};
        socklen_t   len = sizeof(me);
        if(getsockname(s,reinterpret_cast<sockaddr*>(&me),&len)==0) {
          char buf[64] = {};
          inet_ntop(AF_INET,&me.sin_addr,buf,sizeof(buf));
          add(buf);
          }
        }
      closeSock(s);
      }
  }

#ifdef _WIN32
  char host[256] = {};
  if(gethostname(host,sizeof(host))==0) {
    addrinfo hints = {};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if(getaddrinfo(host,nullptr,&hints,&res)==0) {
      for(addrinfo* a=res; a!=nullptr; a=a->ai_next) {
        char buf[64] = {};
        auto* in = reinterpret_cast<sockaddr_in*>(a->ai_addr);
        inet_ntop(AF_INET,&in->sin_addr,buf,sizeof(buf));
        add(buf);
        }
      freeaddrinfo(res);
      }
    }
#else
  ifaddrs* list = nullptr;
  if(getifaddrs(&list)==0) {
    for(ifaddrs* i=list; i!=nullptr; i=i->ifa_next) {
      if(i->ifa_addr==nullptr || i->ifa_addr->sa_family!=AF_INET || !(i->ifa_flags&IFF_UP) || (i->ifa_flags&IFF_LOOPBACK))
        continue;
      char buf[64] = {};
      inet_ntop(AF_INET,&reinterpret_cast<sockaddr_in*>(i->ifa_addr)->sin_addr,buf,sizeof(buf));
      add(buf);
      }
    freeifaddrs(list);
    }
#endif
  return ret;
  }

Hub::Hub() {
  ensureStarted();
  }

Hub::~Hub() {
  close();
  }

void Hub::close() {
  for(auto& l:links) {
    if(l.second.sock!=InvalidSocket)
      closeSock(l.second.sock);
    }
  links.clear();
  if(server!=InvalidSocket)
    closeSock(server);
  server     = InvalidSocket;
  listenPort = 0;
  }

bool Hub::listen(uint16_t port, std::string& error) {
  if(!ensureStarted()) {
    error = "the network is not available";
    return false;
    }
  if(server!=InvalidSocket) {
    error = "already listening on "+std::to_string(listenPort);
    return false;
    }

  Sock s = ::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  if(s==Invalid) {
    error = "no socket, "+errorText(lastError());
    return false;
    }
  int on = 1;
  setsockopt(s,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&on),sizeof(on));

  sockaddr_in addr = {};
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port        = htons(port);
  if(::bind(s,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))!=0 || ::listen(s,8)!=0 || !setNonBlocking(s)) {
    error = "cannot listen on "+std::to_string(port)+", "+errorText(lastError());
    closeSock(s);
    return false;
    }

  // the port 0 means any port
  socklen_t len = sizeof(addr);
  if(getsockname(s,reinterpret_cast<sockaddr*>(&addr),&len)==0)
    port = ntohs(addr.sin_port);

  server     = s;
  listenPort = port;
  return true;
  }

uint32_t Hub::connect(const std::string& host, uint16_t port, std::string& error) {
  if(!ensureStarted()) {
    error = "the network is not available";
    return 0;
    }
  if(links.size()>=MaxPeers) {
    error = "too many connections";
    return 0;
    }

  addrinfo hints = {};
  hints.ai_family   = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  if(getaddrinfo(host.c_str(),std::to_string(port).c_str(),&hints,&res)!=0 || res==nullptr) {
    error = "cannot find "+host;
    return 0;
    }

  Sock s = Invalid;
  int  e = 0;
  for(addrinfo* a=res; a!=nullptr; a=a->ai_next) {
    s = ::socket(a->ai_family,a->ai_socktype,a->ai_protocol);
    if(s==Invalid) {
      e = lastError();
      continue;
      }
    if(!setNonBlocking(s)) {
      e = lastError();
      closeSock(s);
      s = Invalid;
      continue;
      }
    tune(s);
    if(::connect(s,a->ai_addr,AddrLen(a->ai_addrlen))!=0 && !wouldBlock(lastError())) {
      e = lastError();
      closeSock(s);
      s = Invalid;
      continue;
      }
    break;
    }
  freeaddrinfo(res);
  if(s==Invalid) {
    error = "cannot connect to "+host+":"+std::to_string(port)+", "+errorText(e);
    return 0;
    }

  Peer p;
  p.sock       = s;
  p.connecting = true;
  p.incoming   = false;
  p.address    = host+":"+std::to_string(port);
  const uint32_t id = nextPeer++;
  links.emplace(id,std::move(p));
  return id;
  }

bool Hub::hasPeer(uint32_t peer) const {
  auto it = links.find(peer);
  return it!=links.end() && !it->second.connecting;
  }

std::vector<uint32_t> Hub::peers() const {
  std::vector<uint32_t> ret;
  for(auto& l:links) {
    if(!l.second.connecting)
      ret.push_back(l.first);
    }
  return ret;
  }

std::string Hub::address(uint32_t peer) const {
  auto it = links.find(peer);
  return it==links.end() ? std::string() : it->second.address;
  }

bool Hub::send(uint32_t peer, std::string_view data) {
  auto it = links.find(peer);
  if(it==links.end() || data.size()>MaxMessage)
    return false;
  Peer& p = it->second;
  if(p.out.size()+data.size()+4>MaxQueue)
    return false;

  const uint32_t n = uint32_t(data.size());
  const char head[4] = {char(n&0xFF),char((n>>8)&0xFF),char((n>>16)&0xFF),char((n>>24)&0xFF)};
  p.out.append(head,4);
  p.out.append(data);
  return true;
  }

void Hub::broadcast(std::string_view data, uint32_t except) {
  for(auto& l:links) {
    if(l.first!=except && !l.second.connecting)
      send(l.first,data);
    }
  }

void Hub::disconnect(uint32_t peer) {
  auto it = links.find(peer);
  if(it==links.end())
    return;
  // what is queued gets one try to reach the other side
  Peer& p = it->second;
  if(!p.connecting && !p.out.empty())
    sendBytes(p.sock,p.out.data(),p.out.size());
  if(p.sock!=InvalidSocket)
    closeSock(p.sock);
  links.erase(it);
  }

void Hub::drop(uint32_t id, std::vector<Event>& out, const std::string& why, bool tell) {
  auto it = links.find(id);
  if(it==links.end())
    return;
  const bool connected = !it->second.connecting;
  if(it->second.sock!=InvalidSocket)
    closeSock(it->second.sock);
  links.erase(it);

  if(!tell)
    return;
  Event e;
  e.type = connected ? Event::Disconnect : Event::Error;
  e.peer = id;
  e.text = why;
  out.push_back(std::move(e));
  }

void Hub::readPeer(uint32_t id, Peer& p, std::vector<Event>& out, bool& alive) {
  char   buf[16384];
  size_t budget = 512*1024;   // a peer cannot take the whole frame

  while(budget>0) {
    const int n = recvBytes(p.sock,buf,sizeof(buf));
    if(n>0) {
      p.in.append(buf,size_t(n));
      budget -= std::min(budget,size_t(n));
      continue;
      }
    if(n==0) {
      alive = false;
      drop(id,out,"closed by the other side",true);
      return;
      }
    if(wouldBlock(lastError()))
      break;
    alive = false;
    drop(id,out,errorText(lastError()),true);
    return;
    }

  // messages that are whole
  size_t at = 0;
  while(p.in.size()-at>=4) {
    const auto*    b = reinterpret_cast<const unsigned char*>(p.in.data()+at);
    const uint32_t n = uint32_t(b[0]) | (uint32_t(b[1])<<8) | (uint32_t(b[2])<<16) | (uint32_t(b[3])<<24);
    if(n>MaxMessage) {
      alive = false;
      drop(id,out,"the message is too big",true);
      return;
      }
    if(p.in.size()-at-4<n)
      break;

    Event e;
    e.type = Event::Message;
    e.peer = id;
    e.text.assign(p.in.data()+at+4,n);
    out.push_back(std::move(e));
    at += 4+n;
    }
  if(at>0)
    p.in.erase(0,at);
  }

void Hub::writePeer(uint32_t id, Peer& p, std::vector<Event>& out, bool& alive) {
  while(!p.out.empty()) {
    const int n = sendBytes(p.sock,p.out.data(),std::min<size_t>(p.out.size(),65536));
    if(n>0) {
      p.out.erase(0,size_t(n));
      continue;
      }
    if(n<0 && wouldBlock(lastError()))
      return;
    alive = false;
    drop(id,out,errorText(lastError()),true);
    return;
    }
  }

void Hub::poll(std::vector<Event>& out) {
  // new connections
  while(server!=InvalidSocket) {
    sockaddr_storage sa  = {};
    socklen_t        len = sizeof(sa);
    Sock c = ::accept(server,reinterpret_cast<sockaddr*>(&sa),&len);
    if(c==Invalid)
      break;
    if(links.size()>=MaxPeers || !setNonBlocking(c)) {
      closeSock(c);
      continue;
      }
    tune(c);

    Peer p;
    p.sock     = c;
    p.incoming = true;
    p.address  = addressOf(reinterpret_cast<sockaddr*>(&sa));
    const uint32_t id = nextPeer++;

    Event e;
    e.type     = Event::Connect;
    e.peer     = id;
    e.incoming = true;
    e.address  = p.address;
    out.push_back(std::move(e));
    links.emplace(id,std::move(p));
    }

  // the numbers are taken first, because a peer can be dropped on the way
  std::vector<uint32_t> ids;
  ids.reserve(links.size());
  for(auto& l:links)
    ids.push_back(l.first);

  for(uint32_t id:ids) {
    auto it = links.find(id);
    if(it==links.end())
      continue;
    Peer& p = it->second;

    if(p.connecting) {
      int       err = 0;
      const int st  = connectState(p.sock,err);
      if(st==0)
        continue;
      if(st<0) {
        drop(id,out,"cannot connect to "+p.address+", "+errorText(err),true);
        continue;
        }
      p.connecting = false;

      Event e;
      e.type     = Event::Connect;
      e.peer     = id;
      e.incoming = false;
      e.address  = p.address;
      out.push_back(std::move(e));
      }

    bool alive = true;
    writePeer(id,p,out,alive);
    if(!alive)
      continue;
    readPeer(id,p,out,alive);
    }
  }
