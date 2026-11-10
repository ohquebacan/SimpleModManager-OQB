//
// Created by Adrien BLANCHET on 21/06/2020.
//

#include "TabModBrowser.h"

#include "FrameModBrowser.h"


#include "GenericToolbox.Macro.h"
#include "Logger.h"


#include <future>
#include <set>
#include "GenericToolbox.Fs.h"


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
      item->getClickEvent()->subscribe([this, mod](View* view) {
        this->toggleMod( mod.modName );
        return true;
      });
      item->updateActionHint(brls::Key::A, "Activar / apagar");

      // create the holding struct
      _modItemList_.emplace_back();
      _modItemList_.back().modIndex = int(_modItemList_.size() ) - 1;
      _modItemList_.back().item = item;
    }
  }

  this->findConflicts();
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
    auto clash = _conflicts_.find(modEntryList[iMod].modName);
    if( clash != _conflicts_.end() ){
      std::string names;
      for( size_t k = 0; k < clash->second.size() and k < 3; k++ ) names += (k ? ", " : "") + clash->second[k];
      if( clash->second.size() > 3 ) names += " y " + std::to_string(clash->second.size() - 3) + " más";
      _modItemList_[iMod].item->setSubLabel( "Choca con " + names );
    }

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

/* OQB: dos mods chocan si tienen algun archivo con la misma ruta: el que se
   activa despues pisa al otro, y apagar uno borra ese archivo del otro. */
void TabModBrowser::findConflicts(){
  _conflicts_.clear();
  auto& modList = this->getModManager().getModList();
  if( modList.size() < 2 ) return;
  std::map<std::string, std::vector<int>> owners;
  for( size_t i = 0; i < modList.size(); i++ ){
    const std::string modPath = GenericToolbox::joinPath(this->getModManager().getGameFolderPath(), modList[i].modName);
    for( auto& f : GenericToolbox::lsFilesRecursive(modPath) ){
      const std::string name = GenericToolbox::getFileName(f);
      if( name.empty() or name[0] == '.' ) continue;  // .DS_Store, ._* y similares
      owners[f].push_back(int(i));
    }
  }
  std::map<int, std::set<int>> pairs;
  for( auto& o : owners ){
    if( o.second.size() < 2 ) continue;
    for( int a : o.second ) for( int b : o.second ) if( a != b ) pairs[a].insert(b);
  }
  for( auto& p : pairs ){
    auto& v = _conflicts_[modList[p.first].modName];
    for( int b : p.second ) v.push_back(modList[b].modName);
  }
}

void TabModBrowser::toggleMod(const std::string& modName_){
  const auto preset = this->getModManager().fetchCurrentPreset().name;
  double frac{0};
  for( auto& m : this->getModManager().getModList() ){ if( m.modName == modName_ ){ frac = m.getStatusFraction(preset); } }
  if( frac > 0 ){
    _owner_->getGuiModManager().startRemoveModThread( modName_ );
    return;
  }

  // Al activar: si choca con otro que ya esta activo, se avisa antes.
  std::vector<std::string> activeClashes;
  auto it = _conflicts_.find(modName_);
  if( it != _conflicts_.end() ){
    for( auto& other : it->second ){
      for( auto& m : this->getModManager().getModList() ){
        if( m.modName == other and m.getStatusFraction(preset) > 0 ) activeClashes.push_back(other);
      }
    }
  }
  if( activeClashes.empty() ){
    _owner_->getGuiModManager().startApplyModThread( modName_ );
    return;
  }
  std::string list;
  for( size_t i = 0; i < activeClashes.size(); i++ ) list += (i ? ", " : "") + ("\"" + activeClashes[i] + "\"");
  auto* dialog = new brls::Dialog("\"" + modName_ + "\" toca los mismos archivos que " + list +
      ", que está activo. Si lo activas, esos archivos quedan con este mod y el otro queda a medias. "
      "Lo más seguro es apagar el otro primero.");
  dialog->addButton("Activar igual", [this, modName_, dialog](brls::View* view){
    dialog->close();
    _owner_->getGuiModManager().startApplyModThread( modName_ );
  });
  dialog->addButton("Cancelar", [dialog](brls::View* view){ dialog->close(); });
  dialog->setCancelable(true);
  dialog->open();
}

const ModManager& TabModBrowser::getModManager() const{
  return _owner_->getGameBrowser().getModManager();
}
