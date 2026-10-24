#pragma once

#include <Tempest/Widget>
#include <Tempest/Texture2d>
#include <Tempest/Timer>
#include <Tempest/Color>

#include <zenkit/DaedalusVm.hh>
#include <zenkit/addon/daedalus.hh>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "game/savegameheader.h"
#include "game/questlog.h"
#include "utils/keycodec.h"
#include "ui/savescreen.h"

class Gothic;
class MenuRoot;
class Npc;
class GthFont;

class GameMenu : public Tempest::Widget {
  public:
    GameMenu(MenuRoot& owner, KeyCodec& keyCodec, zenkit::DaedalusVm& vm, std::string_view menuSection, KeyCodec::Action keyClose);
    ~GameMenu() override;
    void resetVm(zenkit::DaedalusVm* vm);

    void setPlayer(const Npc& pl);

    void onKeyboard(KeyCodec::Action k);
    void onBeginTouch(Tempest::Point pos);
    void onDragTouch(Tempest::Point pos);
    void onEndTouch(bool activate);
    void onTick();
#ifdef OPENGOTHIC_TOUCH_UI
    bool isTouchMenu() const { return touch.list || touch.log || saves!=nullptr || optView.host; }
    /// the name of a save is typed on the screen of the saves
    bool wantsText() const { return saves!=nullptr && saves->wantsText(); }
    void textKey(const Tempest::KeyEvent& e);
    /// closes what is open on the screen of the saves, false when the menu should go back
    bool closeInner();
    void mobileTouch(float x, float y, int phase);
    void mobileTap(float x, float y);
    void mobileScroll(float dy);
#endif
    void processMusicTheme();

    KeyCodec::Action keyClose() const { return kClose; }

  protected:
    void paintEvent (Tempest::PaintEvent& event) override;
    void resizeEvent(Tempest::SizeEvent&  event) override;

  private:
    enum class QuestStat : uint8_t {
      Current   = uint8_t(QuestLog::Status::Running),
      Old       = uint8_t(QuestLog::Status::Success),
      Failed    = uint8_t(QuestLog::Status::Failed),
      Log       = uint8_t(5),
      };

    struct ListContentDialog;
    struct ListViewDialog;
    struct KeyEditDialog;
    struct SavNameDialog;
    bool                                  statusMenu = false;   // the status screen of the player

    struct Item {
      std::string                         name;
      std::shared_ptr<zenkit::IMenuItem>  handle={};
      const Tempest::Texture2d*           img=nullptr;
      SaveGameHeader                      savHdr;
      Tempest::Pixmap                     savPriview;
      int32_t                             value   = 0;
      int32_t                             scroll  = 0;
      bool                                visible = true;
      };

    MenuRoot&                             owner;
    KeyCodec&                             keyCodec;
    zenkit::DaedalusVm*                   vm = nullptr;
    Tempest::Timer                        timer;
    const Tempest::Texture2d*             up   = nullptr;
    const Tempest::Texture2d*             down = nullptr;

    std::shared_ptr<zenkit::IMenu>        menu={};
    const Tempest::Texture2d*             back=nullptr;
    const Tempest::Texture2d*             slider=nullptr;
    Tempest::Texture2d                    savThumb;
    std::vector<char>                     textBuf;

    Item                                  hItems[zenkit::IMenu::item_count];
    Item*                                 ctrlInput = nullptr;
    Item*                                 touchSlider = nullptr;
    bool                                  touchActive = false;
    uint32_t                              curItem=0;
#ifdef OPENGOTHIC_TOUCH_UI
    /// Script menus are shown as a scrollable list of rows on touch devices
    struct TouchState {
      bool                                list      = false;
      bool                                log       = false;
      int                                 offset    = 0;
      int                                 slider    = -1;    // item of the slider under the finger
      float                               startX    = 0;
      float                               startY    = 0;
      float                               preview   = 0;     // slider value, until the finger is released
      bool                                dragging  = false;
      bool                                consumed  = false; // the gesture was a slider drag, not a tap
      };
    TouchState                            touch;

    /// The menus that are only a list of buttons (the main menu) are drawn in a column on the left (gamemenu_main.cpp)
    struct MainRows {
      std::vector<uint32_t> rows;      // the buttons
      std::vector<int>      top;       // place of each of them, from the top of the first one
      std::vector<bool>     groupEnd;  // a divider follows the button
      int                   rowH    = 1;
      int                   visible = 1;
      int                   y0      = 0;   // the first button
      int                   cx      = 0;   // the middle of the column
      std::vector<Tempest::Rect>             images;   // the logo and the title of the game
      std::vector<const Tempest::Texture2d*> imgTex;
      };
    bool                                  isButtonList() const;
    MainRows                              mainRows() const;
    int                                   visibleRows() const;
    void                                  paintMainList(Tempest::Painter& p);

    /// The saves and the loading of them have a screen of their own
    std::unique_ptr<SaveScreen>           saves;
    void                                  finishSaves();

    /// The options are one screen: the categories on the left, the menu of the chosen one in the panel on the right (gamemenu_options.cpp)
    struct OptionsView {
      bool                  host    = false;     // the menu of the options with the categories
      bool                  built   = false;
      bool                  drag    = false;     // the finger has started in the panel
      std::vector<uint32_t> cats;                // items of the categories
      int                   active  = -1;
      GameMenu*             pane    = nullptr;
      };
    OptionsView                           optView;
    bool                                  embedded = false;   // the menu of a category, it is drawn in the panel of the options
    std::string                           panelTitle;

    struct PanelGeom {
      int rowsTop = 0, rowH = 1, visible = 1, padX = 0, trackX0 = 0, trackX1 = 0, valueR = 0;
      };
    /// the menu speaks Polish (the setting of the language is -1 when the game takes the language of the scripts)
    bool                                  scanPolish() const;
    PanelGeom                             panelGeom() const;
    std::string                           touchRowLabel(uint32_t index, uint32_t controlId) const;
    Tempest::Rect                         optionsSidebar() const;
    Tempest::Rect                         optionsPaneRect() const;
    void                                  optionsUpdate();
    void                                  optionsSelect(int category);
    void                                  optionsTouch(float x, float y, int phase);
    void                                  optionsTap(float x, float y);
    void                                  paintOptionsHost(Tempest::Painter& p);
    void                                  paintPanel(Tempest::Painter& p);

    /// The journal is a screen of its own
    struct LogView {
      Tempest::Rect                       frame;   // picture of the menu
      Tempest::Rect                       tab[4];
      Tempest::Rect                       area;    // list of the quests or the notes
      Tempest::Rect                       back;    // from the notes to the list
      };
    struct TouchLog {
      int                                 tab         = 0;
      int                                 sel         = 0;
      bool                                notes       = false;  // the notes of the quest are open
      float                               listScroll  = 0;
      float                               textScroll  = 0;
      int                                 listMax     = 0;
      int                                 textMax     = 0;
      int                                 drag        = 0;     // 1 - list, 2 - text
      float                               startY      = 0;
      float                               startScroll = 0;
      std::vector<std::pair<int,int>>     rows;                // position and height of the quests in the list
      };
    TouchLog                              touchLog;

    Tempest::Rect                         touchFrameRect() const;
    void                                  paintTouchFrame(Tempest::Painter& p, const Tempest::Rect& r);
    LogView                               touchLogView() const;
    std::vector<const QuestLog::Quest*>   touchLogQuests() const;
    std::array<std::string,4>             touchLogTabNames() const;
    void                                  mobileLogTouch(float x, float y, int phase);
    void                                  mobileLogTap(float x, float y);
    void                                  paintTouchLog(Tempest::Painter& p);

    uint32_t                              touchControl(uint32_t row) const;
    std::vector<uint32_t>                 touchRows() const;
    int                                   touchRowAt(float x, float y) const;
    void                                  paintTouch(Tempest::Painter& p);
#endif
    bool                                  exitFlag=false;
    bool                                  closeFlag=false;

    KeyCodec::Action                      kClose = KeyCodec::Escape;

    /// Scale of the texts. Menus of the original look are enlarged for a phone, texts follow them.
    float                                 fontScale() const;
    Tempest::Size                         originalSize() const;
    float                                 touchK = 1.f;

    void                                  drawItem(Tempest::Painter& p, Item& it);
    void                                  drawSlider(Tempest::Painter& p, Item& it, int x, int y, int w, int h);
    void                                  drawQuestList(Tempest::Painter& p, Item& it, int x, int y, int w, int h,
                                                        const QuestLog& log, QuestStat st);

    Item*                                 selectedItem();
    Tempest::Rect                         itemRect(const Item& item) const;
    Item*                                 selectedNextItem(Item* cur);
    Item*                                 selectedContentItem(Item* it);
    void                                  setSelection(int cur, int seek=1);
    void                                  initItems();
    void                                  getText(const Item &it, std::vector<char>& out);
    const GthFont&                        getTextFont(const Item &it);
    Item*                                 touchHitTest(Item& p, Tempest::Point pos);

    static bool                           isSelectable(const std::shared_ptr<zenkit::IMenuItem>& item);
    static bool                           isHorSelectable(const std::shared_ptr<zenkit::IMenuItem>& item);
    static bool                           isEnabled(const std::shared_ptr<zenkit::IMenuItem>& item);
    static bool                           isHidden(const std::shared_ptr<zenkit::IMenuItem>& item);
    static float                          sliderStep(const std::shared_ptr<zenkit::IMenuItem>& item);

    void                                  exec         (Item &item, int slideDx, KeyCodec::Action hint);
    void                                  execSingle   (Item &it,   int slideDx, KeyCodec::Action hint);
    void                                  execChgOption(Item &item, int slideDx);
    void                                  execSaveGame (const Item& item);
    bool                                  execLoadGame (const Item& item);
    void                                  execCommands (std::string str, bool isClick, KeyCodec::Action hint);

    bool                                  implUpdateSavThumb(Item& sel);
    size_t                                saveSlotId(const Item& sel);

    std::string_view                      strEnum(std::string_view en, int id, std::vector<char> &out);
    size_t                                strEnumSize(std::string_view en);

    void                                  updateValues();
    void                                  updateItem    (Item &item);
    void                                  updateSavTitle(Item& sel);
    void                                  updateSavThumb(Item& sel);
    void                                  updateVideo();
    void                                  setDefaultKeys(std::string_view preset);

    static QuestStat                      toStatus(std::string_view str);
    static bool                           isCompatible(const QuestLog::Quest& q, QuestStat st);
    static int32_t                        numQuests(const QuestLog* q, QuestStat st);

    void                                  set(std::string_view item, const Tempest::Texture2d* value);
    void                                  set(std::string_view item, const uint32_t value);
    void                                  set(std::string_view item, const int32_t  value);
    void                                  set(std::string_view item, const int32_t  value, const int32_t max);
    void                                  set(std::string_view item, std::string_view value);
  };
