#pragma once

#include <Tempest/Event>
#include <Tempest/Painter>
#include <Tempest/Sprite>
#include <Tempest/TextureAtlas>

struct ImGuiContext;

/// Dear ImGui drawn with the painter of Tempest, so it works with every graphics api of the game and on Android.
/// The triangles of ImGui are drawn one by one with a texture of the font, the anti-aliasing is off.
class ImGuiBackend final {
  public:
    explicit ImGuiBackend(Tempest::TextureAtlas& atlas);
    ~ImGuiBackend();

    ImGuiBackend(const ImGuiBackend&) = delete;

    /// Size of the interface: the font and the sizes of the widgets
    void setScale(float scale);

    void newFrame(int width, int height);
    void render(Tempest::Painter& p);

    // --- input
    void mouseMove  (float x, float y);
    void mouseButton(int button, bool down);
    void mouseWheel (float x, float y);
    /// the finger is a mouse that scrolls
    void scrollByPixels(float dy);
    void key (Tempest::Event::KeyType key, bool down);
    void text(uint32_t unicode);

    bool wantsKeyboard() const;
    bool wantsText() const;

  private:
    void buildFont();

    Tempest::TextureAtlas& atlas;
    ImGuiContext*          ctx   = nullptr;
    Tempest::Sprite        font;
    float                  scale = 0;
  };
