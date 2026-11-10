//
// Created by Adrien BLANCHET on 22/06/2020.
//

#include "FrameRoot.h"

#include <TabGames.h>
#include <TabAbout.h>
#include <TabGeneralSettings.h>


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

// Ajustes y Acerca de, con sus pestanas, en un marco aparte.
void FrameRoot::openSettings() {
  auto* settings = new brls::TabFrame();
  settings->setTitle("Ajustes");
  settings->setIcon("romfs:/images/icon_corner.png");
  settings->setFooterText( "SimpleModManager v" + Toolbox::getAppVersion() );
  settings->addTab( "Ajustes", new TabGeneralSettings(this) );
  settings->addTab( "Acerca de", new TabAbout() );
  settings->registerAction("", brls::Key::PLUS, []{return true;}, true);
  brls::Application::pushView(settings, brls::ViewAnimation::SLIDE_LEFT);
}

bool FrameRoot::onCancel() {
  brls::Application::quit();
  return true;
}
