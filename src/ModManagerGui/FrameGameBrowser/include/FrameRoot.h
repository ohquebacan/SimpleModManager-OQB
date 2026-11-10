//
// Created by Adrien BLANCHET on 22/06/2020.
// OQB: sin barra lateral. La pantalla principal es la de juegos a todo el
// ancho; Ajustes y Acerca de se abren con R en su propio marco.
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

  void openSettings();

private:
  GuiModManager _guiModManager_{};

};


#endif //SIMPLEMODMANAGER_FRAMEROOT_H
