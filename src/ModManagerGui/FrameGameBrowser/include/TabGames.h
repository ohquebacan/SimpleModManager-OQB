//
// Created by Adrien BLANCHET on 21/06/2020.
// OQB: rediseno. La pestana de juegos se dibuja entera en lugar de una lista de
// borealis: portadas en cuadricula, pildoras arriba y abajo, y los ajustes en
// un panel a la derecha (R).
//

#ifndef SIMPLEMODMANAGER_TABGAMES_H
#define SIMPLEMODMANAGER_TABGAMES_H


#include "ConfigHandler.h"
#include "GameBrowser.h"
#include "Selector.h"

#include "borealis.hpp"

#include <map>
#include <string>
#include <vector>


class FrameRoot;

struct GameItem{
  std::string title{};      // nombre de la carpeta en /mods
  std::string realName{};   // nombre del juego segun la consola (o la carpeta)
  std::string titleId{};
  int nMods{-1};
  int nActive{-1};          // mods activos segun mods_status_cache.txt (-1 = no se sabe)
  uint32_t lastPlayed{0};   // ultima vez jugado (0 = nunca o desconocido)
  uint32_t minutesAgo{0};
  bool playedKnown{false};
  uint8_t* icon{nullptr};   // 0x20000 bytes, jpg
};

class TabGames : public brls::View {

public:
  explicit TabGames(FrameRoot* owner_);
  ~TabGames() override;

  void draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height, brls::Style* style, brls::FrameContext* ctx) override;
  void layout(NVGcontext* vg, brls::Style* style, brls::FontStash* stash) override {}
  brls::View* getDefaultFocus() override { return this; }
  void onFocusGained() override { this->focused = true; }
  void onFocusLost() override { this->focused = false; }
  void willAppear(bool resetState) override;

  // non native getters
  [[nodiscard]] const GameBrowser& getGameBrowser() const;
  [[nodiscard]] const ConfigHolder& getConfig() const;

  GameBrowser& getGameBrowser();
  ConfigHolder& getConfig();

private:
  FrameRoot* _owner_{};
  std::vector<GameItem> _gameList_;
  std::vector<int> _visible_;      // indices de _gameList_ tras filtrar y ordenar
  std::string _filter_{};
  int _selected_{0};
  int _firstRow_{0};
  bool _panel_{false};  // panel de ajustes abierto
  int _panelSel_{0};
  std::map<int, int> _images_;     // indice del juego -> imagen de NanoVG

  void rebuildList();
  void move(int delta_);
  void openSelected();
  void openGame(GameItem& g_);
  std::string _pendingOpen_{};     // juego que se abre al terminar de preparar sus mods
  void readActiveCounts();
  void openSearch();
  void setSort(int which_);
  int currentSort() const;

  void drawGrid(NVGcontext* vg, float x_, float y_, float w_, float h_);
  void drawPanel(NVGcontext* vg, float x_, float y_, float w_, float h_);
  void panelAction();

};


#endif //SIMPLEMODMANAGER_TABGAMES_H
