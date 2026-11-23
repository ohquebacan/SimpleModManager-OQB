//
// Created by Adrien BLANCHET on 22/06/2020.
// OQB: sin barra lateral. La pantalla principal es la de juegos a todo el
// ancho; los ajustes se abren con R en un panel a la derecha (TabGames).
//

#ifndef SIMPLEMODMANAGER_FRAMEROOT_H
#define SIMPLEMODMANAGER_FRAMEROOT_H

#include "GuiModManager.h"

#include "borealis.hpp"
#include "OqbUi.h"


class FrameRoot : public FrameSinBarras {

public:
  FrameRoot();

  bool onCancel() override;

  const GuiModManager &getGuiModManager() const { return _guiModManager_; }
  GuiModManager &getGuiModManager(){ return _guiModManager_; }


private:
  GuiModManager _guiModManager_{};

};


#endif //SIMPLEMODMANAGER_FRAMEROOT_H
