//
// Created by Adrien BLANCHET on 22/06/2020.
//

#include "TabModOptions.h"
#include "FrameModBrowser.h"




#include "Logger.h"

LoggerInit([]{
  Logger::setUserHeaderStr("[TabModOptions]");
});


TabModOptions::TabModOptions(FrameModBrowser* owner_) : _owner_(owner_) {  }

void TabModOptions::buildFolderInstallPresetItem() {

  _itemConfigPreset_ = new brls::ListItem(
    "\uE255 Perfil de instalación",
    "La carpeta donde se instalan los mods de este juego.\n",
    ""
  );

  _itemConfigPreset_->setValue("El mismo del menú principal");
  if( not this->getModManager().getCurrentPresetName().empty() ){
    _itemConfigPreset_->setValue( this->getModManager().getCurrentPresetName() );
  }

  // On click : show scrolling up menu
  _itemConfigPreset_->getClickEvent()->subscribe([this](View* view) {
    LogInfo << "Opening config preset selector..." << std::endl;

    // build the choice list + preselection
    int preSelection{0};
    std::vector<std::string> menuList;
    menuList.reserve( 1 + this->getModManager().getConfig().presetList.size() );
    menuList.emplace_back( "El mismo del menú principal" );
    int iPreset{-1};
    for( auto& preset: this->getModManager().getConfig().presetList ){
      iPreset++;
      menuList.emplace_back( preset.name + " \uE090 \"" + preset.installBaseFolder + "\"" );
      if( preset.name == this->getModManager().getCurrentPresetName() ){ preSelection = 1 + iPreset; }
    }

    // function that will set the config preset from the Dropdown menu selection (int result)
    brls::ValueSelectedEvent::Callback valueCallback = [this](int result) {
      if( result == -1 ){
        LogDebug << "Not selected. Return." << std::endl;
        // auto pop view
        return;
      }

      if( result == 0 ){
        LogDebug << "Same as config selected. Deleting file..." << std::endl;
        this->getModManager().setCustomPreset("");
        _itemConfigPreset_->setValue( "El mismo del menú principal" );
      }
      else{
        LogDebug << "Selected " << result - 1 << std::endl;
        this->getModManager().setCustomPreset( this->getModManager().getConfig().presetList[result - 1].name );
        _itemConfigPreset_->setValue( this->getModManager().getCurrentPresetName() );
      }

      _owner_->getGuiModManager().checkAllMods( true );
      brls::Application::popView();
      return;
    }; // Callback sequence

    brls::Dropdown::open(
        "Elige el perfil de instalación de este juego",
        menuList, valueCallback,
        preSelection,
        true
    );

  });

}
void TabModOptions::buildResetModsCacheItem() {

  _itemResetModsCache_ = new brls::ListItem(
    "\uE877 Volver a revisar todos los mods",
    "\tBorra el estado guardado y revisa si cada archivo de cada mod está instalado.\n"
              "\tSi tocaste archivos sin SimpleModManager, el estado que se muestra puede estar mal. "
              "Pasa sobre todo al editar archivos de un mod o cuando otro programa reemplaza archivos instalados.",
    ""
  );

  _itemResetModsCache_->getClickEvent()->subscribe([this](View* view){

    auto* dialog = new brls::Dialog("¿Volver a revisar todos los archivos de los mods?");

    dialog->addButton("Sí", [&, dialog](brls::View* view) {
      // first, close the dialog box before the async routine starts
      dialog->close();

      // starts the async routine
      _owner_->getGuiModManager().startCheckAllModsThread();
    });
    dialog->addButton("No", [dialog](brls::View* view) {
      dialog->close();
    });

    dialog->setCancelable(true);
    dialog->open();

  });

}
void TabModOptions::buildDisableAllMods() {

  _itemDisableAllMods_ = new brls::ListItem(
    "\uE872 Apagar todos los mods",
    "Quita los archivos de todos los mods instalados.\n"
              "Solo se borran los archivos idénticos a los de la carpeta del mod.",
    ""
  );

  _itemDisableAllMods_->getClickEvent()->subscribe([this](View* view){

    auto* dialog = new brls::Dialog("¿Apagar todos los mods de este juego?");

    dialog->addButton("Sí", [&, dialog](brls::View* view) {
      // first, close the dialog box before the async routine starts
      dialog->close();

      // starts the async routine
      _owner_->getGuiModManager().startRemoveAllModsThread();
    });
    dialog->addButton("No", [dialog](brls::View* view) {
      dialog->close();
    });

    dialog->setCancelable(true);
    dialog->open();

  });

}
void TabModOptions::buildGameIdentificationItem(){

  _itemGameIdentification_ = new brls::ListItem(
    "TitleID del juego",
    "",
    "Valor actual: "
  );

  if( _owner_->getIcon() != nullptr ){
    _itemGameIdentification_->setThumbnail( _owner_->getIcon(), 0x20000 );
    _itemGameIdentification_->setSubLabel(
        _itemGameIdentification_->getSubLabel() + _owner_->getTitleId()
    );
  }
  else{
    _itemGameIdentification_->setSubLabel(
        _itemGameIdentification_->getSubLabel() + "No encontrado."
    );
  }

}

void TabModOptions::initialize() {

  this->buildFolderInstallPresetItem();
  this->buildResetModsCacheItem();
  this->buildDisableAllMods();
  this->buildGameIdentificationItem();

  // finally add to view
  this->addView(_itemResetModsCache_);
  this->addView(_itemConfigPreset_);
  this->addView(_itemDisableAllMods_);
  this->addView(_itemGameIdentification_);

}

void TabModOptions::draw(NVGcontext *vg, int x, int y, unsigned int width, unsigned int height, brls::Style *style,
                         brls::FrameContext *ctx) {
  ScrollView::draw(vg, x, y, width, height, style, ctx);
}

const ModManager &TabModOptions::getModManager() const {
  return _owner_->getGameBrowser().getModManager();
}
ModManager &TabModOptions::getModManager() {
  return _owner_->getGameBrowser().getModManager();
}




