#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

/// TCP connections of a mod. Nothing blocks: the mod calls poll() every frame.
/// A message is a piece of text with its length in front of it, so both sides see the messages whole.
namespace Scripting::net {

class Hub final {
  public:
    struct Event {
      enum Type : uint8_t { Connect, Disconnect, Message, Error };

      Type        type     = Message;
      uint32_t    peer     = 0;
      bool        incoming = false;   // Connect: the other side has connected to us
      std::string text;               // Message: the data, Disconnect and Error: the reason
      std::string address;            // Connect: address of the other side
      };

    constexpr static size_t MaxMessage = 4u*1024u*1024u;
    constexpr static size_t MaxQueue   = 16u*1024u*1024u;
    constexpr static size_t MaxPeers   = 32;

    Hub();
    ~Hub();
    Hub(const Hub&) = delete;

    /// Waits for connections of other games
    bool     listen(uint16_t port, std::string& error);
    /// Starts to connect, Connect or Error comes later. Returns the number of the peer, 0 on a failure.
    uint32_t connect(const std::string& host, uint16_t port, std::string& error);

    bool     send(uint32_t peer, std::string_view data);
    void     broadcast(std::string_view data, uint32_t except = 0);
    void     disconnect(uint32_t peer);
    /// Closes everything, no events are made
    void     close();

    void     poll(std::vector<Event>& out);

    bool     isListening() const { return server!=InvalidSocket; }
    uint16_t port()        const { return listenPort; }
    bool     hasPeer(uint32_t peer) const;
    std::vector<uint32_t> peers() const;
    std::string address(uint32_t peer) const;

  private:
#ifdef _WIN32
    using Socket = uintptr_t;
    static constexpr Socket InvalidSocket = ~Socket(0);
#else
    using Socket = int;
    static constexpr Socket InvalidSocket = -1;
#endif

    struct Peer {
      Socket      sock       = InvalidSocket;
      bool        connecting = false;
      bool        incoming   = false;
      std::string address;
      std::string in;       // bytes that have come and are not a message yet
      std::string out;      // bytes that wait for the network
      };

    void drop(uint32_t id, std::vector<Event>& out, const std::string& why, bool tell);
    void readPeer (uint32_t id, Peer& p, std::vector<Event>& out, bool& alive);
    void writePeer(uint32_t id, Peer& p, std::vector<Event>& out, bool& alive);

    Socket                    server     = InvalidSocket;
    uint16_t                  listenPort = 0;
    uint32_t                  nextPeer   = 1;
    std::map<uint32_t,Peer>   links;
  };

}
