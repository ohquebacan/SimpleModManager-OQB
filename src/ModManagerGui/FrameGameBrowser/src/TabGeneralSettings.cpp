//
// Created by Adrien BLANCHET on 21/06/2020.
//

#include <TabGeneralSettings.h>

#include "FrameRoot.h"


#include "Logger.h"

LoggerInit([]{
  Logger::setUserHeaderStr("[TabGeneralSettings]");
});

TabGeneralSettings::TabGeneralSettings(FrameRoot* owner_) : _owner_(owner_) {
  LogWarning << "Building general settings tab..." << std::endl;
  this->rebuildLayout();
  LogInfo << "General settings tab build." << std::endl;
}

void TabGeneralSettings::rebuildLayout() {

  itemInstallLocationPreset = new brls::ListItem(
    "\uE255 Dónde se instalan:",
    "La carpeta base donde se copian los mods.\n"\
    "- Con Atmosphère, los mods van en /atmosphere/. "\
    "Es el perfil \"default\". Ten en cuenta esa ruta al armar las carpetas de cada mod.\n"\
    "- Para que un juego use otra carpeta, ve a su pestaña Opciones y elige su perfil de instalación.",
    ""
  );
  itemInstallLocationPreset->setValue(this->getConfig().getCurrentPresetName() );
  itemInstallLocationPreset->getClickEvent()->subscribe([this](View* view) {
    brls::ValueSelectedEvent::Callback valueCallback = [this](int result) {
      if (result == -1) {
        return;
      }

      this->getConfig().setSelectedPresetIndex( result );
      _owner_->getGuiModManager().getGameBrowser().getConfigHandler().dumpConfigToFile();
      LogInfo << "Selected: " << this->getConfig().getCurrentPreset().name << " -> " << this->getConfig().getCurrentPreset().installBaseFolder << std::endl;
      this->itemInstallLocationPreset->setValue(this->getConfig().getCurrentPresetName() );
    };

    std::vector<std::string> presetNameList;
    presetNameList.reserve( this->getConfig().presetList.size() );
    for( auto& preset : this->getConfig().presetList ){
      presetNameList.emplace_back( preset.name + " \uE090 \"" + preset.installBaseFolder + "\"" );
    }

    brls::Dropdown::open(
      "Perfil de instalación actual:",
      presetNameList,
      valueCallback,
      this->getConfig().selectedPresetIndex
    );
  });
  this->addView(itemInstallLocationPreset);

  auto* itemStoredModsBaseFolder = new brls::ListItem(
    "\uE431 Dónde están guardados los mods:",
    "La carpeta donde SimpleModManager busca tus mods. Dentro, las carpetas van así:\n"\
    "./<juego-o-categoría>/<nombre-del-mod>/<archivos-del-mod>.",
    "");
  itemStoredModsBaseFolder->setValue( this->getConfig().baseFolder );
  this->addView(itemStoredModsBaseFolder);


  auto*  itemSortGames = new brls::ListItem(
      "\uE255 Ordenar los juegos por",
      "El orden de la lista de juegos (también se cambia con X en Juegos).\n",
      ""
  );
  itemSortGames->setValue( this->getConfig().sortGameList.toString() );


  // On click : show scrolling up menu
  itemSortGames->getClickEvent()->subscribe([this, itemSortGames](View* view) {
    LogInfo << "Opening itemSortGames selector..." << std::endl;

    // build the choice list + preselection
    int preSelection{0};
    std::vector<std::string> menuList;
    menuList.reserve( ConfigHolder::SortGameList::getEnumSize() );
    for( int iEnum = 0 ; iEnum < ConfigHolder::SortGameList::getEnumSize() ; iEnum++ ){
      menuList.emplace_back( ConfigHolder::SortGameList::toString(iEnum) );
      if( menuList.back() == this->getConfig().sortGameList.toString() ){ preSelection = iEnum; }
    }

    // function that will set the config preset from the Dropdown menu selection (int result)
    brls::ValueSelectedEvent::Callback valueCallback = [this, itemSortGames, menuList](int result) {
      if( result == -1 ){
        LogDebug << "Not selected. Return." << std::endl;
        // auto pop view
        return;
      }

      LogInfo << "Selected: " << menuList[result] << std::endl;
      this->getConfig().sortGameList = ConfigHolder::SortGameList::toEnum( menuList[result] );
      _owner_->getGuiModManager().getGameBrowser().getConfigHandler().dumpConfigToFile();
      itemSortGames->setValue( this->getConfig().sortGameList.toString() );

      brls::Application::popView();
      return;
    }; // Callback sequence

    brls::Dropdown::open(
        "Elige el orden",
        menuList, valueCallback,
        preSelection,
        true
    );

  });
  this->addView(itemSortGames);


  auto* itemUseUI = new brls::ListItem("\uE072 Usar la interfaz de texto", "Vuelve a la interfaz antigua, estilo consola.");

  itemUseUI->updateActionHint(brls::Key::B, "Volver");
  itemUseUI->registerAction("Elegir", brls::Key::A, [&](){

    auto* dialog = new brls::Dialog("¿Cambiar a la interfaz de texto, estilo consola?");

    dialog->addButton("Sí", [&](brls::View* view) {
      this->getConfig().useGui = false;
      _owner_->getGuiModManager().getGameBrowser().getConfigHandler().dumpConfigToFile();
      brls::Application::quit();
    });
    dialog->addButton("No", [dialog](brls::View* view) {
      dialog->close();
    });

    dialog->setCancelable(true);
    dialog->open();

    return true;
  });
  this->addView(itemUseUI);

}

const ConfigHolder& TabGeneralSettings::getConfig() const{
  return _owner_->getGuiModManager().getGameBrowser().getConfigHandler().getConfig();
}
ConfigHolder& TabGeneralSettings::getConfig(){
  return _owner_->getGuiModManager().getGameBrowser().getConfigHandler().getConfig();
}
