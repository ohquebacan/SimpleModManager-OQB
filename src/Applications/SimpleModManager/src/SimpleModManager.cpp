//
// Created by Adrien Blanchet on 14/04/2023.
//


#include "SimpleModManager.h"

#include <FrameRoot.h>
#include "OqbUi.h"

#include "ConfigHandler.h"

#include "Logger.h"

#include <borealis.hpp>

#include <string>
#include <cstdlib>
#include "iostream"

#include "switch.h"


LoggerInit([]{
  Logger::setUserHeaderStr("[SimpleModManager.nro]");
});


int main(int argc, char* argv[]){
  LogInfo << "SimpleModManager is starting..." << std::endl;

  // https://github.com/jbeder/yaml-cpp/wiki/Tutorial
//  YAML::Node config = YAML::LoadFile("config.yaml");
//  if (config["lastLogin"]) {
//    std::cout << "Last logged in: " << config["lastLogin"].as<std::string>() << "\n";
//  }
//  const auto username = config["username"].as<std::string>();
//  const auto password = config["password"].as<std::string>();

  /* OQB: sin la interfaz de texto. Si la configuracion quedo en modo texto
     (la opcion vieja de Ajustes), se vuelve a la interfaz normal. */
  {
    ConfigHandler c;
    if( not c.getConfig().useGui ){
      c.getConfig().useGui = true;
      c.dumpConfigToFile();
    }
  }
  runGui();

  // Exit
  return EXIT_SUCCESS;
}


void runGui(){
  LogInfo << "Starting GUI..." << std::endl;
  LogThrowIf(R_FAILED(nsInitialize()), "nsInitialize Failed");

  brls::Logger::setLogLevel(brls::LogLevel::ERROR);

  brls::i18n::loadTranslations("en-US");
  LogThrowIf(not brls::Application::init("SimpleModManager"), "Unable to init Borealis application");

  // OQB: colores Amatista y letras de OQB; bateria y Wi-Fi para la barra de arriba.
  oqbui::usarPaleta(oqbui::leerTema());
  oqbui::aplicarTema();
  oqbui::cargarFuentes();
  const bool psm = R_SUCCEEDED(psmInitialize());
  const bool nifm = R_SUCCEEDED(nifmInitialize(NifmServiceType_User));

  LogInfo << "Creating root frame..." << std::endl;
  auto* mainFrame = new FrameRoot();

  LogInfo << "Pushing to view" << std::endl;
  brls::Application::pushView( mainFrame );
  mainFrame->registerAction( "", brls::Key::PLUS, []{return true;}, true );
  mainFrame->updateActionHint( brls::Key::PLUS, "" ); // make the change visible

  while( brls::Application::mainLoop() ){  }

  if( nifm ) nifmExit();
  if( psm ) psmExit();
  nsExit();
}
