//
// OQB: pantalla de mods de un juego, como el mockup: sin barra lateral, cada mod
// con su interruptor, su estado y el aviso si choca con otro. Los grupos de mods,
// las opciones y los plugins siguen en su marco de siempre (X).
//

#ifndef SIMPLEMODMANAGER_FRAMEMODS_H
#define SIMPLEMODMANAGER_FRAMEMODS_H

#include "GuiModManager.h"

#include <borealis.hpp>
#include "OqbUi.h"

#include <map>
#include <string>
#include <vector>


class ModsView : public brls::View {

public:
  ModsView(GuiModManager* manager_, uint8_t* icon_, std::string title_);
  ~ModsView() override;

  void draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height, brls::Style* style, brls::FrameContext* ctx) override;
  void layout(NVGcontext* vg, brls::Style* style, brls::FontStash* stash) override {}
  brls::View* getDefaultFocus() override { return this; }
  void onFocusGained() override { this->focused = true; }
  void onFocusLost() override { this->focused = false; }

private:
  GuiModManager* _manager_{nullptr};
  uint8_t* _icon_{nullptr};
  int _image_{0};
  std::string _title_;

  struct Row{
    std::string name;
    int nFiles{0};
    std::vector<std::string> clashes;
  };
  std::vector<Row> _rows_;
  std::vector<int> _visible_;
  std::string _filter_;
  int _selected_{0};
  int _first_{0};
  bool _checked_{false};
  int _zone_{0};  // 0 lista, 1 buscador

  void scanMods();
  void rebuild();
  void move(int delta_);
  void toggleSelected();
  void openMore();
  void openSearch();
  double fractionOf(const std::string& modName_) const;
  std::string statusOf(const std::string& modName_) const;
};


class FrameMods : public FrameSinBarras {

public:
  FrameMods(GuiModManager* manager_, const std::string& title_, const uint8_t* icon_ = nullptr);
  ~FrameMods() override;

private:
  uint8_t* _icon_{nullptr};
};


#endif //SIMPLEMODMANAGER_FRAMEMODS_H
