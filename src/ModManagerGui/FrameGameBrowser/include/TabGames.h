//
// Created by Adrien BLANCHET on 21/06/2020.
//

#ifndef SIMPLEMODMANAGER_TABGAMES_H
#define SIMPLEMODMANAGER_TABGAMES_H


#include "ConfigHandler.h"
#include "GameBrowser.h"
#include "Selector.h"

#include "borealis.hpp"

#include <vector>


struct GameItem;
class FrameRoot;

class TabGames : public brls::List {

public:
  explicit TabGames(FrameRoot* owner_);

  // non native getters
  [[nodiscard]] const GameBrowser& getGameBrowser() const;
  [[nodiscard]] const ConfigHolder& getConfig() const;

  GameBrowser& getGameBrowser();
  ConfigHolder& getConfig();

private:
  FrameRoot* _owner_{};
  std::vector<GameItem> _gameList_;
  std::string _filter_{};  // OQB: texto del buscador
  brls::ListItem* _emptyItem_{nullptr};
  bool _built_{false};

  void rebuildList();

};

struct GameItem{
  std::string title{};      // nombre de la carpeta en /mods
  std::string realName{};   // OQB: nombre del juego segun la consola (o la carpeta)
  int nMods{-1};
  uint32_t lastPlayed{0};   // OQB: ultima vez jugado (0 = nunca o desconocido)
  uint32_t minutesAgo{0};
  bool playedKnown{false};

  // memory is fully handled by brls
  brls::ListItem* item{nullptr};
};


#endif //SIMPLEMODMANAGER_TABGAMES_H
