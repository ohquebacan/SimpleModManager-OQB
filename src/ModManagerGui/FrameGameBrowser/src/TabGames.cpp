//
// Created by Adrien BLANCHET on 21/06/2020.
//

#include "TabGames.h"
#include "FrameModBrowser.h"
#include "FrameRoot.h"

#include "GenericToolbox.Switch.h"
#include "GenericToolbox.Vector.h"
#include "Logger.h"

#include <switch.h>
#include <zlib.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <sstream>

LoggerInit([]{
  Logger::setUserHeaderStr("[TabGames]");
});

namespace {

  /* OQB: desde el firmware 21, las actualizaciones traen los nombres de la NACP
     comprimidos (zlib, para que entren 32 idiomas): el byte 0x3215 vale 1 y libnx
     los da tal cual, por eso salian "?" y simbolos raros. Esto los descomprime en
     su lugar. Ver switchbrew/libnx#714. */
  bool decompressNacpNames(NacpStruct* nacp){
    auto* raw = reinterpret_cast<u8*>(nacp);
    if( raw[0x3215] != 1 ) return true;
    u16 size = 0;
    std::memcpy(&size, raw, sizeof(size));
    if( size == 0 or size > 0x3000 - 2 ) return false;
    std::vector<u8> out(0x6000);
    z_stream z{};
    z.next_in = raw + 2;
    z.avail_in = size;
    z.next_out = out.data();
    z.avail_out = static_cast<uInt>(out.size());
    if( inflateInit2(&z, -15) != Z_OK ) return false;
    const int r = inflate(&z, Z_FINISH);
    inflateEnd(&z);
    if( r != Z_STREAM_END or z.total_out < 0x3000 ) return false;
    std::memcpy(raw, out.data(), 0x3000);
    raw[0x3215] = 0;
    return true;
  }

  // Nombre e icono de un juego instalado, desde su NACP.
  void infoFromTid(const std::string& tid_, std::string& name_, uint8_t*& icon_){
    name_.clear();
    icon_ = nullptr;
    if( tid_.empty() ) return;
    uint64_t tid{0};
    std::istringstream buffer(tid_);
    buffer >> std::hex >> tid;
    auto control = std::make_unique<NsApplicationControlData>();
    size_t controlSize{0};
    if( R_FAILED(nsGetApplicationControlData(NsApplicationControlSource_Storage, tid, control.get(), sizeof(NsApplicationControlData), &controlSize)) ) return;
    decompressNacpNames(&control->nacp);
    NacpLanguageEntry* lang{nullptr};
    if( R_SUCCEEDED(nacpGetLanguageEntry(&control->nacp, &lang)) and lang != nullptr ){
      name_ = std::string(lang->name, strnlen(lang->name, sizeof(lang->name)));
    }
    icon_ = new uint8_t[0x20000];
    std::memcpy(icon_, control->icon, 0x20000);
  }

  std::string playedText(const GameItem& g_){
    if( not g_.playedKnown ) return "";
    const uint32_t m = g_.minutesAgo;
    if( m < 60 ) return "jugado hace un rato";
    if( m < 60 * 24 ) return "jugado hace " + std::to_string(m / 60) + " h";
    const uint32_t d = m / (60 * 24);
    if( d == 1 ) return "jugado ayer";
    if( d < 60 ) return "jugado hace " + std::to_string(d) + " días";
    return "jugado hace " + std::to_string(d / 30) + " meses";
  }

  std::string lowerAscii(std::string s_){
    for( auto& c : s_ ){ c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
    return s_;
  }

  std::string sortLabel(ConfigHolder::SortGameList s_){
    switch( s_.value ){
      case ConfigHolder::SortGameList::Recent: return "Recientes";
      case ConfigHolder::SortGameList::Alphabetical: return "A-Z";
      case ConfigHolder::SortGameList::NbMods: return "Más mods";
      default: return "Sin orden";
    }
  }

}

TabGames::TabGames(FrameRoot* owner_) : _owner_(owner_) {
  LogWarning << "Building game tab..." << std::endl;

  auto gameList = this->getGameBrowser().getSelector().getEntryList();

  if( gameList.empty() ){
    LogInfo << "No game found." << std::endl;

    std::stringstream ssTitle;
    std::stringstream ssSubTitle;

    auto baseFolder = this->getConfig().baseFolder;
    ssTitle << "No hay carpetas de juegos en " << baseFolder;

    ssSubTitle << "- Para agregar mods, cópialos así: ";
    ssSubTitle << baseFolder << "/<nombre-del-juego>/<nombre-del-mod>/<archivos-y-carpetas-del-mod>." << std::endl;
    ssSubTitle << "- También puedes cambiar esta carpeta (" + baseFolder;
    ssSubTitle << ") en /config/SimpleModManager/parameters.ini";

    auto* item = new brls::ListItem( ssTitle.str(), ssSubTitle.str() );
    item->show([](){}, false);
    this->addView( item );
    return;
  }

  LogInfo << "Adding " << gameList.size() << " game folders..." << std::endl;

  std::vector<std::string> tids;
  _gameList_.reserve( gameList.size() );
  for( auto& gameEntry : gameList ){
    LogScopeIndent;
    LogInfo << "Adding game folder: \"" << gameEntry.title << "\"" << std::endl;

    std::string gamePath{GenericToolbox::joinPath(this->getConfig().baseFolder, gameEntry.title)};
    int nMods = int( GenericToolbox::lsDirs(gamePath).size() );

    // looking for tid is quite slow... Slows down the boot up
    std::string titleId{ GenericToolbox::Switch::Utils::lookForTidInSubFolders(gamePath) };
    std::string realName;
    uint8_t* icon{nullptr};
    infoFromTid(titleId, realName, icon);
    tids.emplace_back(titleId);

    /* OQB: el nombre real del juego, no el de la carpeta. Las carpetas que creo
       SimpleModDownloader con un juego nuevo pueden tener "?" o simbolos raros
       (nombres comprimidos de la NACP), y asi no habia forma de encontrarlo. */
    if( realName.empty() ){ realName = gameEntry.title; }

    auto* item = new brls::ListItem(realName, "", std::to_string(nMods) + (nMods == 1 ? " mod" : " mods"));
    if(icon != nullptr){ item->setThumbnail(icon, 0x20000); }
    item->getClickEvent()->subscribe([&, gameEntry, realName](View* view) {
      LogWarning << "Opening \"" << gameEntry.title << "\"" << std::endl;
      getGameBrowser().selectGame( gameEntry.title );
      auto* modsBrowser = new FrameModBrowser( &_owner_->getGuiModManager() );
      modsBrowser->setTitle( realName );  // OQB: el nombre real, no el de la carpeta
      brls::Application::pushView(modsBrowser, brls::ViewAnimation::SLIDE_LEFT);
      modsBrowser->registerAction("", brls::Key::PLUS, []{return true;}, true);
      modsBrowser->updateActionHint(brls::Key::PLUS, ""); // make the change visible
    });
    item->updateActionHint(brls::Key::A, "Abrir");

    _gameList_.emplace_back();
    _gameList_.back().title = gameEntry.title;
    _gameList_.back().realName = realName;
    _gameList_.back().item = item;
    _gameList_.back().nMods = nMods;
  }

  // OQB: cuando se jugo cada juego por ultima vez (para ordenar por recientes).
  if( R_SUCCEEDED(pdmqryInitialize()) ){
    for( size_t i = 0; i < _gameList_.size(); i++ ){
      if( tids[i].empty() ) continue;
      uint64_t tid{0};
      std::istringstream buffer(tids[i]);
      buffer >> std::hex >> tid;
      PdmLastPlayTime last{};
      s32 total{0};
      if( R_SUCCEEDED(pdmqryQueryLastPlayTime(false, &last, &tid, 1, &total)) and total > 0 and last.flag ){
        _gameList_[i].lastPlayed = last.timestamp_user;
        _gameList_[i].minutesAgo = last.last_played_minutes;
        _gameList_[i].playedKnown = true;
      }
    }
    pdmqryExit();
  }
  for( auto& g : _gameList_ ){
    const std::string played = playedText(g);
    std::string sub = std::to_string(g.nMods) + (g.nMods == 1 ? " mod" : " mods");
    if( not played.empty() ) sub += "  ·  " + played;
    g.item->setSubLabel(sub);
  }

  // Los items los maneja la lista a mano (se sacan y se vuelven a poner al filtrar).
  _emptyItem_ = new brls::ListItem("Ningún juego coincide con la búsqueda", "Pulsa Y para buscar otra cosa.");

  this->registerAction("Buscar", brls::Key::Y, [this]{
    brls::Swkbd::openForText([this](std::string text){
      _filter_ = text;
      this->rebuildList();
    }, "Buscar juego", "", 40, _filter_);
    return true;
  });
  this->registerAction("Ordenar: " + sortLabel(this->getConfig().sortGameList), brls::Key::X, [this]{
    auto& sort = this->getConfig().sortGameList;
    switch( sort.value ){
      case ConfigHolder::SortGameList::Recent: sort = ConfigHolder::SortGameList::Alphabetical; break;
      case ConfigHolder::SortGameList::Alphabetical: sort = ConfigHolder::SortGameList::NbMods; break;
      default: sort = ConfigHolder::SortGameList::Recent; break;
    }
    this->getGameBrowser().getConfigHandler().dumpConfigToFile();
    this->updateActionHint(brls::Key::X, "Ordenar: " + sortLabel(sort));
    this->rebuildList();
    return true;
  });

  this->rebuildList();
  _built_ = true;
  LogInfo << "Game tab build." << std::endl;
}

void TabGames::rebuildList(){
  if( _gameList_.empty() ) return;

  switch( this->getConfig().sortGameList.value ){
    case ConfigHolder::SortGameList::Alphabetical:
      std::stable_sort(_gameList_.begin(), _gameList_.end(), [](const GameItem& a_, const GameItem& b_){
        return lowerAscii(a_.realName) < lowerAscii(b_.realName);
      });
      break;
    case ConfigHolder::SortGameList::NbMods:
      std::stable_sort(_gameList_.begin(), _gameList_.end(), [](const GameItem& a_, const GameItem& b_){
        return a_.nMods > b_.nMods;
      });
      break;
    case ConfigHolder::SortGameList::Recent:
      std::stable_sort(_gameList_.begin(), _gameList_.end(), [](const GameItem& a_, const GameItem& b_){
        if( a_.lastPlayed != b_.lastPlayed ) return a_.lastPlayed > b_.lastPlayed;
        return lowerAscii(a_.realName) < lowerAscii(b_.realName);
      });
      break;
    default:
      break;
  }

  // Sacar todo sin liberar: los items se reutilizan.
  this->clear(false);

  const std::string filter = lowerAscii(_filter_);
  brls::View* first{nullptr};
  for( auto& game : _gameList_ ){
    if( not filter.empty()
        and lowerAscii(game.realName).find(filter) == std::string::npos
        and lowerAscii(game.title).find(filter) == std::string::npos ) continue;
    this->addView( game.item );
    if( first == nullptr ) first = game.item;
  }
  if( first == nullptr ){
    this->addView( _emptyItem_ );
    first = _emptyItem_;
  }
  this->updateActionHint(brls::Key::Y, _filter_.empty() ? "Buscar" : "Buscar: " + _filter_);
  // Al armar la pestana todavia no esta en pantalla: el foco lo da el marco.
  if( _built_ ){ brls::Application::giveFocus(first); }
}

const GameBrowser& TabGames::getGameBrowser() const{
  return _owner_->getGuiModManager().getGameBrowser();
}
const ConfigHolder& TabGames::getConfig() const{
  return getGameBrowser().getConfigHandler().getConfig();
}

GameBrowser& TabGames::getGameBrowser(){
  return _owner_->getGuiModManager().getGameBrowser();
}
ConfigHolder& TabGames::getConfig(){
  return getGameBrowser().getConfigHandler().getConfig();
}
