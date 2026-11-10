//
// Created by Adrien BLANCHET on 21/06/2020.
//

#include "TabModBrowser.h"

#include "FrameModBrowser.h"


#include "GenericToolbox.Macro.h"
#include "Logger.h"


#include <future>


LoggerInit([]{
  Logger::setUserHeaderStr("[TabModBrowser]");
});


TabModBrowser::TabModBrowser(FrameModBrowser* owner_) : _owner_(owner_) {

  // Fetch the available mods
  auto modList = this->getModManager().getModList();

  if( modList.empty() ){
    LogInfo << "No mod found." << std::endl;

    _modItemList_.emplace_back();
    _modItemList_.back().item = new brls::ListItem(
        "No hay mods en " + this->getModManager().getGameFolderPath(),
        "Cada mod va en su carpeta: ./<nombre-del-mod>/<archivos como van en la consola>"
    );
    _modItemList_.back().item->show([](){}, false );
  }
  else{
    LogInfo << "Adding " << modList.size() << " mods..." << std::endl;

    _modItemList_.reserve(modList.size());
    for( auto& mod : modList ) {
      LogScopeIndent;
      LogInfo << "Adding mod: \"" << mod.modName << "\"" << std::endl;

      // memory allocation
      auto* item = new brls::ListItem(mod.modName, "", "");

      /* OQB: A activa o apaga el mod segun como este (antes A siempre instalaba
         y X desactivaba, cada uno con su pregunta). Activar copia los archivos
         del mod; apagar los quita. */
      item->getClickEvent()->subscribe([&, mod](View* view) {
        const auto preset = this->getModManager().fetchCurrentPreset().name;
        double frac{0};
        for( auto& m : this->getModManager().getModList() ){ if( m.modName == mod.modName ){ frac = m.getStatusFraction(preset); } }
        if( frac > 0 ){
          _owner_->getGuiModManager().startRemoveModThread( mod.modName );
        }
        else{
          _owner_->getGuiModManager().startApplyModThread( mod.modName );
        }
        return true;
      });
      item->updateActionHint(brls::Key::A, "Activar / apagar");

      // create the holding struct
      _modItemList_.emplace_back();
      _modItemList_.back().modIndex = int(_modItemList_.size() ) - 1;
      _modItemList_.back().item = item;
    }
  }

  this->updateDisplayedModsStatus();

  // add to view
  for( auto& modItem : _modItemList_ ){
    this->addView( modItem.item );
  }

}

void TabModBrowser::draw(NVGcontext *vg, int x, int y, unsigned int width, unsigned int height, brls::Style *style,
                         brls::FrameContext *ctx) {

  ScrollView::draw(vg, x, y, width, height, style, ctx);

  if( _owner_->getGuiModManager().isTriggerUpdateModsDisplayedStatus() ){
    LogDebug << "Updating mod status display..." << std::endl;
    updateDisplayedModsStatus();
    _owner_->getGuiModManager().setTriggerUpdateModsDisplayedStatus( false );
  }
}

void TabModBrowser::updateDisplayedModsStatus(){
  LogInfo << __METHOD_NAME__ << std::endl;

  auto& modEntryList = _owner_->getGameBrowser().getModManager().getModList();
  LogReturnIf( modEntryList.empty(), "No mod in this folder. Nothing to update." );

  auto currentPreset = this->getModManager().fetchCurrentPreset().name;
  LogInfo << "Will display mod status with install preset: " << currentPreset << std::endl;

  for( size_t iMod = 0 ; iMod < modEntryList.size() ; iMod++ ){

    // processing tag
    double frac = modEntryList[iMod].getStatusFraction(currentPreset);
    // OQB: el estado en palabras, no "ACTIVE" / "PARTIAL (3/7)".
    const std::string status = modEntryList[iMod].getStatus(currentPreset);
    std::string label;
    if( status.empty() ) label = "Sin revisar";
    else if( status == "NO FILE" ) label = "Sin archivos";
    else if( frac == 0 ) label = "Apagado";
    else if( frac == 1 ) label = "Activo";
    else{
      const auto open = status.find('(');
      label = "A medias " + (open == std::string::npos ? std::string() : status.substr(open));
    }
    _modItemList_[iMod].item->setValue( label );

    NVGcolor color;
    // processing color
    if     ( frac == 0 ){
      // inactive color
      color = GenericToolbox::Borealis::grayNvgColor;
    }
    else if( frac == 1 ){
      // applied color (less saturated green)
      color = nvgRGB(88, 195, 169);
    }
    else{
      // partial color
      color = GenericToolbox::Borealis::orangeNvgColor;
    }
    _modItemList_[iMod].item->setValueActiveColor( color );
  }
}

const ModManager& TabModBrowser::getModManager() const{
  return _owner_->getGameBrowser().getModManager();
}
