//
// Created by Adrien BLANCHET on 20/06/2020.
//

#include "TabAbout.h"
#include <Toolbox.h>

#include "Logger.h"

#include <borealis.hpp>



LoggerInit([]{
  Logger::setUserHeaderStr("[TabAbout]");
});

/* OQB: lista simple (encabezados y textos). La tabla de dos columnas con la
   foto tenia alto fijo (260) y, con el texto en espanol mas largo, la pestana
   se quedaba colgada al abrirla. */
TabAbout::TabAbout() {
  LogWarning << "Building about tab..." << std::endl;

  auto addText = [this](brls::LabelStyle style_, const std::string& text_){
    auto* label = new brls::Label(style_, text_, true);
    label->setHorizontalAlign(NVG_ALIGN_LEFT);
    this->addView(label);
  };

  addText(brls::LabelStyle::REGULAR,
          "SimpleModManager es una app homebrew para Nintendo Switch que activa y apaga los mods y archivos de configuración de tu SD.");

  this->addView(new brls::Header("Versión " + Toolbox::getAppVersion() + " (OQB) - Novedades"));
  addText(brls::LabelStyle::DESCRIPTION,
          "- Juegos a pantalla completa, sin barra lateral (R abre Ajustes)\n"
          "- Pantalla de mods con interruptores y estado\n"
          "- Buscador, orden y mods activos en la lista de juegos\n"
          "- Nombres reales de los juegos nuevos\n"
          "- Aviso cuando dos mods tocan los mismos archivos\n"
          "- Todo en español");

  this->addView(new brls::Header("Créditos"));
  addText(brls::LabelStyle::DESCRIPTION,
          "Autor: Nadrino. Versión OQB: OH! QUÉ BACÁN.\n"
          "Licencia GPL-v3.0. \u00A9 2019 - 2026 Nadrino.");

  this->addView(new brls::Header("Agradecimientos"));
  addText(brls::LabelStyle::SMALL,
          "SimpleModManager usa Borealis, una librería de interfaz acelerada por hardware.\n"
          "Gracias al equipo de RetroNX por su ayuda con Borealis.");

  LogInfo << "About tab built." << std::endl;
}
