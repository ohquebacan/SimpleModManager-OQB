//
// Created by Adrien BLANCHET on 22/06/2020.
//

#include "FrameRoot.h"

#include <TabGames.h>


#include "Toolbox.h"

#include "Logger.h"

LoggerInit([]{
  Logger::setUserHeaderStr("[FrameRoot]");
});


FrameRoot::FrameRoot() : FrameSinBarras() {
  LogWarning << "Build root frame..." << std::endl;

  this->setTitle("Mis mods");
  this->setFooterText( "SimpleModManager v" + Toolbox::getAppVersion() );
  this->setIcon("romfs:/images/icon_corner.png");
  this->setContentView( new TabGames(this) );

  LogInfo << "Root frame built." << std::endl;
}

bool FrameRoot::onCancel() {
  brls::Application::quit();
  return true;
}
