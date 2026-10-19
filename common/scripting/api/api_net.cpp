#include "scripting/modjs.h"

#include "scripting/net/netlink.h"

using namespace Scripting;

/// net: connections with the games of other players. The mod makes its own protocol on top of messages of text,
/// see docs/MODS.md. Every mod has connections of its own, they are closed when the mod stops.
namespace {

struct State {
  net::Hub hub;
  };

State* state(JSContext* ctx) {
  auto& m = js::mod(ctx);
  if(m.netState==nullptr)
    m.netState = std::make_shared<State>();
  return static_cast<State*>(m.netState.get());
  }

uint32_t peerArg(JSContext* ctx, JSValueConst v) {
  uint32_t id = 0;
  if(JS_ToUint32(ctx,&id,v)!=0)
    return 0;
  return id;
  }

/// Text of a message: a string is sent as it is, other values as JSON
bool payload(JSContext* ctx, JSValueConst v, std::string& out) {
  if(JS_IsString(v)) {
    out = js::str(ctx,v);
    return true;
    }
  JSValue j = JS_JSONStringify(ctx,v,JS_UNDEFINED,JS_UNDEFINED);
  if(JS_IsException(j)) {
    JS_FreeValue(ctx,JS_GetException(ctx));
    return false;
    }
  if(JS_IsUndefined(j))
    return false;
  out = js::str(ctx,j);
  JS_FreeValue(ctx,j);
  return true;
  }

uint16_t portArg(JSContext* ctx, JSValueConst v, bool& ok) {
  int32_t p = 0;
  ok = JS_ToInt32(ctx,&p,v)==0 && p>=0 && p<=65535;
  return uint16_t(p);
  }

JSValue listen(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  bool     ok = false;
  uint16_t port = argc>0 ? portArg(ctx,argv[0],ok) : uint16_t(0);
  if(!ok)
    return JS_ThrowRangeError(ctx,"net.listen(port): the port is a number from 0 to 65535");

  std::string error;
  auto* s = state(ctx);
  if(!s->hub.listen(port,error))
    return JS_ThrowInternalError(ctx,"%s",error.c_str());
  return JS_NewInt32(ctx,s->hub.port());
  }

/// net.connect(host, port): the number of the peer, the connection is made later (net:connect or net:error)
JSValue connect(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<2)
    return JS_ThrowTypeError(ctx,"net.connect(host, port)");
  bool     ok   = false;
  uint16_t port = portArg(ctx,argv[1],ok);
  if(!ok || port==0)
    return JS_ThrowRangeError(ctx,"net.connect(host, port): the port is a number from 1 to 65535");

  std::string error;
  const uint32_t id = state(ctx)->hub.connect(js::str(ctx,argv[0]),port,error);
  if(id==0)
    return JS_ThrowInternalError(ctx,"%s",error.c_str());
  return JS_NewUint32(ctx,id);
  }

JSValue send(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  std::string text;
  if(argc<2 || !payload(ctx,argv[1],text))
    return JS_FALSE;
  return JS_NewBool(ctx,state(ctx)->hub.send(peerArg(ctx,argv[0]),text));
  }

/// net.broadcast(data, exceptPeer)
JSValue broadcast(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  std::string text;
  if(argc<1 || !payload(ctx,argv[0],text))
    return JS_FALSE;
  const uint32_t except = argc>1 ? peerArg(ctx,argv[1]) : 0;
  state(ctx)->hub.broadcast(text,except);
  return JS_TRUE;
  }

JSValue disconnect(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc>0)
    state(ctx)->hub.disconnect(peerArg(ctx,argv[0]));
  return JS_UNDEFINED;
  }

JSValue close(JSContext* ctx, JSValueConst, int, JSValueConst*) {
  state(ctx)->hub.close();
  return JS_UNDEFINED;
  }

JSValue address(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
  if(argc<1)
    return JS_UNDEFINED;
  return JS_NewString(ctx,state(ctx)->hub.address(peerArg(ctx,argv[0])).c_str());
  }

JSValue getter(JSContext* ctx, JSValueConst, int magic) {
  auto& hub = state(ctx)->hub;
  switch(magic) {
    case 0: return JS_NewBool(ctx,hub.isListening());
    case 1: return JS_NewInt32(ctx,hub.port());
    case 2: {
      JSValue arr = JS_NewArray(ctx);
      uint32_t n  = 0;
      for(uint32_t id:hub.peers())
        JS_SetPropertyUint32(ctx,arr,n++,JS_NewUint32(ctx,id));
      return arr;
      }
    default: break;
    }
  return JS_UNDEFINED;
  }

void defineGetter(JSContext* ctx, JSValueConst obj, const char* name, int magic) {
  JSAtom  a = JS_NewAtom(ctx,name);
  JSValue g = JS_NewCFunction2(ctx,reinterpret_cast<JSCFunction*>(&getter),name,0,JS_CFUNC_getter_magic,magic);
  JS_DefinePropertyGetSet(ctx,obj,a,g,JS_UNDEFINED,JS_PROP_C_W_E);
  JS_FreeAtom(ctx,a);
  }

}

void api::installNet(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue obj    = JS_NewObject(ctx);
  js::fn(ctx,obj,"listen",    listen,1);
  js::fn(ctx,obj,"connect",   connect,2);
  js::fn(ctx,obj,"send",      send,2);
  js::fn(ctx,obj,"broadcast", broadcast,2);
  js::fn(ctx,obj,"disconnect",disconnect,1);
  js::fn(ctx,obj,"close",     close,0);
  js::fn(ctx,obj,"address",   address,1);
  defineGetter(ctx,obj,"listening",0);
  defineGetter(ctx,obj,"port",     1);
  defineGetter(ctx,obj,"peers",    2);
  JS_SetPropertyStr(ctx,global,"net",obj);
  JS_FreeValue(ctx,global);
  }

/// net:connect {peer, address, incoming}, net:disconnect {peer, reason}, net:message {peer, data}, net:error {peer, error}
void api::pollNet(Mod& mod) {
  if(mod.netState==nullptr || mod.ctx==nullptr)
    return;

  // keeps the state alive, if the handlers stop the mod
  auto keep = mod.netState;
  auto* s   = static_cast<State*>(keep.get());

  std::vector<net::Hub::Event> events;
  s->hub.poll(events);

  auto& host = ModHost::inst();
  for(auto& e:events) {
    if(!mod.isRunning() || mod.ctx==nullptr)
      break;

    JSContext* ctx = mod.ctx;
    JSValue    arg = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx,arg,"peer",JS_NewUint32(ctx,e.peer));
    const char* name = "net:message";
    switch(e.type) {
      case net::Hub::Event::Connect:
        name = "net:connect";
        JS_SetPropertyStr(ctx,arg,"address", JS_NewString(ctx,e.address.c_str()));
        JS_SetPropertyStr(ctx,arg,"incoming",JS_NewBool(ctx,e.incoming));
        break;
      case net::Hub::Event::Disconnect:
        name = "net:disconnect";
        JS_SetPropertyStr(ctx,arg,"reason",JS_NewString(ctx,e.text.c_str()));
        break;
      case net::Hub::Event::Error:
        name = "net:error";
        JS_SetPropertyStr(ctx,arg,"error",JS_NewString(ctx,e.text.c_str()));
        break;
      case net::Hub::Event::Message:
        JS_SetPropertyStr(ctx,arg,"data",JS_NewStringLen(ctx,e.text.data(),e.text.size()));
        break;
      }
    host.emitTo(mod,name,arg,true);
    JS_FreeValue(ctx,arg);
    }
  }

void api::releaseNet(Mod& mod) {
  if(mod.netState==nullptr)
    return;
  static_cast<State*>(mod.netState.get())->hub.close();
  mod.netState.reset();
  }
