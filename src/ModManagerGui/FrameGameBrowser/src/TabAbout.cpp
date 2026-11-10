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

TabAbout::TabAbout() {
  LogWarning << "Building about tab..." << std::endl;

  // Subtitle
  auto* shortDescription = new brls::Label(
    brls::LabelStyle::REGULAR,
    "SimpleModManager es una app homebrew para Nintendo Switch que activa y apaga los mods y archivos de configuración de tu SD. Versión OQB con buscador, orden y nombres reales de los juegos.",
    true
  );
  shortDescription->setHorizontalAlign(NVG_ALIGN_CENTER);
  this->addView(shortDescription);

  auto* table = new brls::BoxLayout(brls::BoxLayoutOrientation::HORIZONTAL);
  table->setSpacing(22);
  table->setHeight(260);

  auto* leftBox = new brls::BoxLayout(brls::BoxLayoutOrientation::VERTICAL);
  leftBox->setSpacing(22);
  leftBox->setWidth(500);
  leftBox->setParent(table);

  leftBox->addView(new brls::Header("Versión " + Toolbox::getAppVersion() + " - Novedades"));
  auto *changelog = new brls::Label(
    brls::LabelStyle::DESCRIPTION,
    " - Sin barra lateral: juegos a pantalla completa (R abre Ajustes)\n - Pantalla de mods nueva con interruptores\n - Pantalla de juegos nueva: buscador, orden y mods activos\n - Nombres reales de los juegos nuevos\n - Aviso cuando dos mods tocan los mismos archivos\n - Todo en español\n",
    true
  );
  changelog->setHorizontalAlign(NVG_ALIGN_LEFT);
  leftBox->addView(changelog);

  leftBox->addView(new brls::Header("Derechos"));
  auto *copyright = new brls::Label(
    brls::LabelStyle::DESCRIPTION,
    "SimpleModManager tiene licencia GPL-v3.0\n" \
        "\u00A9 2019 - 2026 Nadrino",
    true
  );
  copyright->setHorizontalAlign(NVG_ALIGN_CENTER);
  leftBox->addView(copyright);

  auto* rightBox = new brls::BoxLayout(brls::BoxLayoutOrientation::VERTICAL);
  rightBox->setSpacing(22);
  rightBox->setWidth(200);
  rightBox->setParent(table);

  rightBox->addView(new brls::Label(brls::LabelStyle::DESCRIPTION, " "));

  auto* portrait = new brls::Image("romfs:/images/portrait.jpg");
  portrait->setScaleType(brls::ImageScaleType::SCALE);
  portrait->setHeight(200);
  portrait->setParent(rightBox);
  rightBox->addView(portrait);
  auto* portraitText = new brls::Label(brls::LabelStyle::SMALL, "Autor: Nadrino · versión OQB", true);
  portraitText->setHorizontalAlign(NVG_ALIGN_CENTER);
  rightBox->addView(portraitText);

  table->addView(leftBox);
  table->addView(rightBox);

  this->addView(table);

  this->addView(new brls::Header("Agradecimientos"));
  auto *links = new brls::Label(
    brls::LabelStyle::SMALL,
    "\uE017  SimpleModManager usa Borealis, una librería de interfaz acelerada por hardware\n" \
         "\uE017  Gracias al equipo de RetroNX por su ayuda con Borealis\n",
    true
  );
  this->addView(links);

  LogInfo << "About tab built." << std::endl;
}
