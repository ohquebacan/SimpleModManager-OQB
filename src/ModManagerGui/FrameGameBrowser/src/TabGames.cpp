//
// Created by Adrien BLANCHET on 21/06/2020.
// OQB: rediseno de la pestana de juegos (ver TabGames.h).
//

#include "TabGames.h"
#include "FrameModBrowser.h"
#include "FrameMods.h"
#include "OqbUi.h"
#include "FrameRoot.h"
#include "TabAbout.h"
#include "Toolbox.h"

#include "GenericToolbox.Switch.h"
#include "GenericToolbox.Vector.h"
#include "Logger.h"

#include <switch.h>
#include <zlib.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <map>
#include <fstream>
#include <sstream>

LoggerInit([]{
  Logger::setUserHeaderStr("[TabGames]");
});

namespace {

  /* Desde el firmware 21, las actualizaciones traen los nombres de la NACP
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

  uint64_t parseTid(const std::string& tid_){
    uint64_t tid{0};
    std::istringstream buffer(tid_);
    buffer >> std::hex >> tid;
    return tid;
  }

  // Nombre e icono de un juego instalado, desde su NACP.
  void infoFromTid(const std::string& tid_, std::string& name_, uint8_t*& icon_){
    name_.clear();
    icon_ = nullptr;
    if( tid_.empty() ) return;
    auto control = std::make_unique<NsApplicationControlData>();
    size_t controlSize{0};
    if( R_FAILED(nsGetApplicationControlData(NsApplicationControlSource_Storage, parseTid(tid_), control.get(), sizeof(NsApplicationControlData), &controlSize)) ) return;
    decompressNacpNames(&control->nacp);
    NacpLanguageEntry* lang{nullptr};
    if( R_SUCCEEDED(nacpGetLanguageEntry(&control->nacp, &lang)) and lang != nullptr ){
      name_ = std::string(lang->name, strnlen(lang->name, sizeof(lang->name)));
    }
    icon_ = new uint8_t[0x20000];
    std::memcpy(icon_, control->icon, 0x20000);
  }

  // Cuando se jugo, corto para una etiqueta.
  std::string playedShort(const GameItem& g_){
    if( not g_.playedKnown ) return "";
    const uint32_t m = g_.minutesAgo;
    if( m < 60 * 24 ) return "hoy";
    const uint32_t d = m / (60 * 24);
    if( d == 1 ) return "ayer";
    if( d < 60 ) return "hace " + std::to_string(d) + " d";
    return "hace " + std::to_string(d / 30) + " meses";
  }

  std::string lowerAscii(std::string s_){
    for( auto& c : s_ ){ c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
    return s_;
  }

  std::string modsText(int n_){ return std::to_string(n_) + (n_ == 1 ? " mod" : " mods"); }

  const char* kSortNames[] = {"recientes", "A-Z", "más mods"};

  // Geometria de la cuadricula (pantalla de 1280 x 720, como la maqueta).
  constexpr int kCols = 5;
  constexpr float kGap = 16.f, kCardH = 226.f;

}

TabGames::TabGames(FrameRoot* owner_) : _owner_(owner_) {
  LogWarning << "Building game tab..." << std::endl;

  auto gameList = this->getGameBrowser().getSelector().getEntryList();
  LogInfo << "Adding " << gameList.size() << " game folders..." << std::endl;

  /* Cache (OQB): por carpeta, su TID y el nombre del juego; los iconos aparte.
     Buscar el TID recorriendo las carpetas y pedir el control de cada juego
     era lo lento; con la cache solo se hace con las carpetas nuevas. */
  const std::string cacheDir = "/config/SimpleModManager/oqb_cache/";
  const std::string cachePath = cacheDir + "juegos.txt";
  GenericToolbox::mkdir(cacheDir);
  std::map<std::string, std::pair<std::string, std::string>> cache;  // carpeta -> (tid, nombre)
  if( GenericToolbox::isFile(cachePath) ){
    for( auto& line : GenericToolbox::dumpFileAsVectorString(cachePath) ){
      const auto a = line.find('\t'), b = line.find('\t', a + 1);
      if( a == std::string::npos or b == std::string::npos ) continue;
      cache[line.substr(0, a)] = {line.substr(a + 1, b - a - 1), line.substr(b + 1)};
    }
  }
  std::stringstream nuevo;
  bool cambio = false;

  _gameList_.reserve( gameList.size() );
  for( auto& gameEntry : gameList ){
    LogScopeIndent;
    LogInfo << "Adding game folder: \"" << gameEntry.title << "\"" << std::endl;

    GameItem g;
    g.title = gameEntry.title;
    const std::string gamePath{GenericToolbox::joinPath(this->getConfig().baseFolder, gameEntry.title)};
    g.nMods = int( GenericToolbox::lsDirs(gamePath).size() );

    auto hit = cache.find(g.title);
    const std::string iconPath = hit != cache.end() ? cacheDir + hit->second.first + ".jpg" : "";
    if( hit != cache.end() and not hit->second.first.empty() and GenericToolbox::isFile(iconPath) ){
      g.titleId = hit->second.first;
      g.realName = hit->second.second;
      std::ifstream f(iconPath, std::ios::binary);
      std::vector<char> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
      g.icon = new uint8_t[0x20000]();
      std::memcpy(g.icon, data.data(), std::min<size_t>(data.size(), 0x20000));
    }
    else{
      g.titleId = GenericToolbox::Switch::Utils::lookForTidInSubFolders(gamePath);
      /* El nombre real del juego, no el de la carpeta: las carpetas que creo
         SimpleModDownloader con un juego nuevo pueden tener "?" o simbolos raros
         (nombres comprimidos de la NACP), y asi no habia forma de encontrarlo. */
      infoFromTid(g.titleId, g.realName, g.icon);
      if( g.icon != nullptr and not g.titleId.empty() ){
        std::ofstream f(cacheDir + g.titleId + ".jpg", std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(g.icon), 0x20000);
      }
      cambio = true;
    }
    if( g.realName.empty() ){ g.realName = g.title; }
    if( not g.titleId.empty() and g.icon != nullptr ){ nuevo << g.title << '\t' << g.titleId << '\t' << g.realName << '\n'; }
    _gameList_.emplace_back(std::move(g));
  }
  if( cambio or cache.size() != _gameList_.size() ){
    GenericToolbox::dumpStringInFile(cachePath, nuevo.str());
  }

  // Cuando se jugo cada juego por ultima vez (para ordenar por recientes).
  if( R_SUCCEEDED(pdmqryInitialize()) ){
    for( auto& g : _gameList_ ){
      if( g.titleId.empty() ) continue;
      const uint64_t tid = parseTid(g.titleId);
      PdmLastPlayTime last{};
      s32 total{0};
      if( R_SUCCEEDED(pdmqryQueryLastPlayTime(false, &last, &tid, 1, &total)) and total > 0 and last.flag ){
        g.lastPlayed = last.timestamp_user;
        g.minutesAgo = last.last_played_minutes;
        g.playedKnown = true;
      }
    }
    pdmqryExit();
  }
  this->readActiveCounts();

  /* Navegacion: la cruceta mueve por la cuadricula; R abre los ajustes en un
     panel a la derecha y L o B lo cierran. Con el panel abierto, la cruceta
     recorre sus opciones. */
  this->registerAction("", brls::Key::DUP, [this]{
    if( _panel_ ){ _panelSel_ = std::max(0, _panelSel_ - 1); return true; }
    this->move(-kCols);
    return true;
  }, true);
  this->registerAction("", brls::Key::DDOWN, [this]{
    if( _panel_ ){ _panelSel_ = std::min(4, _panelSel_ + 1); return true; }
    // Abajo desde una fila incompleta de arriba: al ultimo juego.
    if( _selected_ + kCols >= int(_visible_.size()) and _selected_ / kCols < (int(_visible_.size()) - 1) / kCols ){
      _selected_ = int(_visible_.size()) - 1;
      return true;
    }
    this->move(kCols);
    return true;
  }, true);
  this->registerAction("", brls::Key::DLEFT, [this]{ if( not _panel_ ) this->move(-1); return true; }, true);
  this->registerAction("", brls::Key::DRIGHT, [this]{ if( not _panel_ ) this->move(1); return true; }, true);
  this->registerAction("", brls::Key::L, [this]{ _panel_ = false; return true; }, true);
  this->registerAction("", brls::Key::R, [this]{ _panel_ = true; return true; }, true);
  this->registerAction("", brls::Key::B, [this]{
    if( not _panel_ ) return false;  // sin panel, B sale de la app (FrameRoot)
    _panel_ = false;
    return true;
  }, true);
  this->registerAction("", brls::Key::A, [this]{
    if( _panel_ ) this->panelAction();
    else this->openSelected();
    return true;
  }, true);
  this->registerAction("", brls::Key::Y, [this]{ if( not _panel_ ) this->openSearch(); return true; }, true);
  this->registerAction("", brls::Key::X, [this]{
    if( not _panel_ ) this->setSort((this->currentSort() + 1) % 3);
    return true;
  }, true);
  this->registerAction("", brls::Key::MINUS, [this]{
    if( _panel_ or _filter_.empty() ) return true;
    _filter_.clear();
    this->rebuildList();
    return true;
  }, true);

  this->rebuildList();
  LogInfo << "Game tab build." << std::endl;
}

TabGames::~TabGames(){
  NVGcontext* vg = brls::Application::getNVGContext();
  for( auto& img : _images_ ){ if( img.second > 0 ) nvgDeleteImage(vg, img.second); }
  for( auto& g : _gameList_ ){ delete[] g.icon; }
}

// Al volver de los mods de un juego, sus activos pueden haber cambiado.
void TabGames::willAppear(bool resetState){
  this->readActiveCounts();
  brls::View::willAppear(resetState);
}

/* Cuantos mods estan activos en cada juego, segun el resumen que SimpleModManager
   guarda en cada carpeta (mods_status_cache.txt): "perfil: mod = ESTADO = fraccion".
   Leerlo es instantaneo; revisar los archivos de cada mod no. */
void TabGames::readActiveCounts(){
  const std::string preset = this->getConfig().getCurrentPresetName();
  for( auto& g : _gameList_ ){
    const std::string cache = GenericToolbox::joinPath(this->getConfig().baseFolder, g.title, "mods_status_cache.txt");
    if( not GenericToolbox::isFile(cache) ){ g.nActive = -1; continue; }
    int n{0};
    for( auto& line : GenericToolbox::dumpFileAsVectorString(cache) ){
      const auto colon = line.find(": ");
      if( colon == std::string::npos or line.substr(0, colon) != preset ) continue;
      const auto a = line.find(" = ", colon);
      if( a == std::string::npos ) continue;
      const auto b = line.find(" = ", a + 3);
      const std::string status = line.substr(a + 3, b == std::string::npos ? std::string::npos : b - a - 3);
      if( status == "ACTIVE" ) n++;
    }
    g.nActive = n;
  }
}

void TabGames::rebuildList(){
  std::vector<int> order(_gameList_.size());
  for( size_t i = 0; i < order.size(); i++ ) order[i] = int(i);

  auto byName = [this](int a_, int b_){ return lowerAscii(_gameList_[a_].realName) < lowerAscii(_gameList_[b_].realName); };
  switch( this->getConfig().sortGameList.value ){
    case ConfigHolder::SortGameList::Alphabetical:
      std::stable_sort(order.begin(), order.end(), byName);
      break;
    case ConfigHolder::SortGameList::NbMods:
      std::stable_sort(order.begin(), order.end(), [&](int a_, int b_){
        if( _gameList_[a_].nMods != _gameList_[b_].nMods ) return _gameList_[a_].nMods > _gameList_[b_].nMods;
        return byName(a_, b_);
      });
      break;
    default:  // Recientes
      std::stable_sort(order.begin(), order.end(), [&](int a_, int b_){
        if( _gameList_[a_].lastPlayed != _gameList_[b_].lastPlayed ) return _gameList_[a_].lastPlayed > _gameList_[b_].lastPlayed;
        return byName(a_, b_);
      });
      break;
  }

  const std::string filter = lowerAscii(_filter_);
  _visible_.clear();
  for( int i : order ){
    const auto& g = _gameList_[i];
    if( not filter.empty()
        and lowerAscii(g.realName).find(filter) == std::string::npos
        and lowerAscii(g.title).find(filter) == std::string::npos ) continue;
    _visible_.push_back(i);
  }
  _selected_ = 0;
  _firstRow_ = 0;
}

void TabGames::openSearch(){
  brls::Swkbd::openForText([this](std::string text){
    _filter_ = text;
    this->rebuildList();
  }, "Buscar juego", "", 40, _filter_);
}

int TabGames::currentSort() const{
  const auto v = this->getConfig().sortGameList.value;
  return v == ConfigHolder::SortGameList::Alphabetical ? 1 : v == ConfigHolder::SortGameList::NbMods ? 2 : 0;
}

void TabGames::setSort(int which_){
  auto& sort = this->getConfig().sortGameList;
  sort = which_ == 1 ? ConfigHolder::SortGameList::Alphabetical : which_ == 2 ? ConfigHolder::SortGameList::NbMods : ConfigHolder::SortGameList::Recent;
  this->getGameBrowser().getConfigHandler().dumpConfigToFile();
  this->rebuildList();
}

void TabGames::move(int delta_){
  if( _visible_.empty() ) return;
  _selected_ = std::clamp(_selected_ + delta_, 0, int(_visible_.size()) - 1);
}

void TabGames::openSelected(){
  if( _visible_.empty() or not _pendingOpen_.empty() ) return;
  GameItem& g = _gameList_[_visible_[_selected_]];
  /* OH! QUÉ BACÁN: antes de abrir, los .zip/.7z/.rar se descomprimen, los
     .pchtxt pasan a .ips y los mods con otra estructura se ordenan (OqbMods).
     Solo si hace falta; con su ventana de progreso, y al terminar se abre. */
  const std::string gamePath{GenericToolbox::joinPath(this->getConfig().baseFolder, g.title)};
  const std::string base = getGameBrowser().getModManager().fetchCurrentPreset().installBaseFolder;
  if( oqb::mods::NecesitaPreparar(gamePath, g.titleId, base) ){
    LogWarning << "Preparing mods of \"" << g.title << "\"" << std::endl;
    _pendingOpen_ = g.title;
    _owner_->getGuiModManager().startPrepareGameThread(gamePath, g.titleId, base);
    return;
  }
  this->openGame(g);
}
void TabGames::openGame(GameItem& g){
  LogWarning << "Opening \"" << g.title << "\"" << std::endl;
  getGameBrowser().selectGame( g.title );
  auto* mods = new FrameMods( &_owner_->getGuiModManager(), g.realName, g.icon );
  mods->registerAction("", brls::Key::PLUS, []{return true;}, true);
  brls::Application::pushView(mods, brls::ViewAnimation::SLIDE_LEFT);
}

void TabGames::draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height, brls::Style* style, brls::FrameContext* ctx){
  using namespace oqbui;
  // Terminó de preparar los mods: ahora sí se abre el juego, con la cuenta al día.
  if( not _pendingOpen_.empty() and not _owner_->getGuiModManager().isTaskRunning() ){
    const std::string title = _pendingOpen_;
    _pendingOpen_.clear();
    for( auto& g : _gameList_ ){
      if( g.title != title ) continue;
      g.nMods = int( GenericToolbox::lsDirs(GenericToolbox::joinPath(this->getConfig().baseFolder, g.title)).size() );
      this->openGame(g);
      break;
    }
  }
  const float al = this->alpha;
  caja(vg, x, y, width, height, 0.f, pal::fondo, al);

  const float left = x + 32.f, right = x + width - 32.f;

  // Arriba: pestanas, titulo (y la busqueda, si hay) y la hora.
  {
    const float top = y + 24.f;
    float cx = left + pestanas(vg, left, top, _panel_ ? 1 : 0, al) + 14.f;
    cx += titulo(vg, cx, top + 28.f, "TUS JUEGOS", al) + 16.f;
    const float wEstado = estado(vg, right, top, al);
    if( not _filter_.empty() ){
      nvgFontFaceId(vg, fuentes().texto);
      nvgFontSize(vg, 14.f);
      const std::string b = ajustar(vg, "«" + _filter_ + "» · " + std::to_string(_visible_.size()) + " de " + std::to_string(_gameList_.size()), right - wEstado - 20.f - cx - 20.f);
      etiqueta(vg, cx, top + 28.f, b, pal::chip, pal::cuerpo, al, false, 14.f);
    }
  }

  const float gridTop = y + 100.f, gridH = height - 100.f - 84.f;

  // Sin carpetas de juegos: como poner los mods.
  if( _gameList_.empty() ){
    const std::string base = this->getConfig().baseFolder;
    texto(vg, fuentes().negrita, 24.f, conAlfa(pal::texto, al), left, gridTop + 30.f, "No hay juegos con mods en " + base);
    nvgFontFaceId(vg, fuentes().texto);
    nvgFontSize(vg, 18.f);
    nvgTextLineHeight(vg, 1.4f);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, conAlfa(pal::tenue, al));
    nvgBeginPath(vg);
    const std::string help = "Copia cada mod así: " + base + "/<juego>/<nombre del mod>/<archivos del mod>. "
                             "SimpleModDownloader los deja en ese lugar al descargarlos. "
                             "La carpeta se puede cambiar en /config/SimpleModManager/parameters.ini.";
    nvgTextBox(vg, left, gridTop + 64.f, right - left, help.c_str(), nullptr);
    pie(vg, left, right, y + height, "", {{"R", "Ajustes"}, {"B", "Salir"}}, al);
  }
  else if( _visible_.empty() ){
    texto(vg, fuentes().texto, 18.f, conAlfa(pal::tenue, al), left, gridTop + 24.f, "Ningún juego coincide con «" + _filter_ + "».");
    pie(vg, left, right, y + height, "0 / " + std::to_string(_gameList_.size()), {{"−", "Ver todos"}, {"Y", "Buscar otra cosa", true}}, al);
  }
  else{
    this->drawGrid(vg, left, gridTop, right - left, gridH);
    if( not _panel_ ){
      pie(vg, left, right, y + height, std::to_string(_selected_ + 1) + " / " + std::to_string(_visible_.size()), {
          {"−", _filter_.empty() ? "" : "Quitar búsqueda"},
          {"Y", "Buscar"},
          {"X", std::string("Orden: ") + kSortNames[this->currentSort()]},
          {"A", "Abrir", true}}, al);
    }
  }

  if( _panel_ ){
    // Lo de atras se apaga y el panel entra por la derecha.
    caja(vg, x, y, width, height, 0.f, nvgRGBAf(pal::fondo.r, pal::fondo.g, pal::fondo.b, 0.72f), al);
    this->drawPanel(vg, x, y, width, height);
  }
}

void TabGames::drawGrid(NVGcontext* vg, float x_, float y_, float w_, float h_){
  using namespace oqbui;
  const float al = this->alpha;
  const float cw = (w_ - kGap * (kCols - 1)) / kCols, step = kCardH + kGap;
  const int rows = (int(_visible_.size()) + kCols - 1) / kCols;
  const int fullRows = std::max(1, int((h_ + kGap) / step));
  const int selRow = _selected_ / kCols;
  if( selRow < _firstRow_ ) _firstRow_ = selRow;
  if( selRow >= _firstRow_ + fullRows ) _firstRow_ = selRow - fullRows + 1;

  nvgSave(vg);
  nvgScissor(vg, x_ - 4.f, y_ - 4.f, w_ + 8.f, h_ + 4.f);
  for( int r = _firstRow_; r < rows and (r - _firstRow_) * step < h_; r++ ){
    for( int c = 0; c < kCols; c++ ){
      const int vi = r * kCols + c;
      if( vi >= int(_visible_.size()) ) break;
      const int gi = _visible_[vi];
      GameItem& g = _gameList_[gi];
      const float cx = x_ + c * (cw + kGap), cy = y_ + (r - _firstRow_) * step;

      auto it = _images_.find(gi);
      if( it == _images_.end() ){
        it = _images_.emplace(gi, g.icon ? nvgCreateImageMem(vg, 0, g.icon, 0x20000) : 0).first;
      }
      // Portada: el icono cubre la tarjeta (cuadrado, centrado); sin icono, color y siglas.
      nvgBeginPath(vg);
      nvgRoundedRect(vg, cx, cy, cw, kCardH, 14.f);
      if( it->second > 0 ){
        const float side = std::max(cw, kCardH);
        nvgFillPaint(vg, nvgImagePattern(vg, cx + (cw - side) / 2, cy + (kCardH - side) / 2, side, side, 0, it->second, al));
      }
      else nvgFillColor(vg, conAlfa(colorDe(g.realName), al));
      nvgFill(vg);
      if( it->second <= 0 ){
        texto(vg, fuentes().marca, 40.f, nvgRGBAf(1, 1, 1, 0.55f * al), cx + 16.f, cy + 36.f, siglas(g.realName));
      }

      // Abajo, un velo para que se lea el nombre.
      nvgBeginPath(vg);
      nvgRoundedRect(vg, cx, cy, cw, kCardH, 14.f);
      nvgFillPaint(vg, nvgLinearGradient(vg, 0, cy + kCardH - 104.f, 0, cy + kCardH - 50.f, nvgRGBAf(pal::fondo.r, pal::fondo.g, pal::fondo.b, 0.f), conAlfa(nvgRGBAf(pal::fondo.r, pal::fondo.g, pal::fondo.b, 0.92f), al)));
      nvgFill(vg);

      nvgFontFaceId(vg, fuentes().negrita);
      nvgFontSize(vg, 16.f);
      texto(vg, fuentes().negrita, 16.f, conAlfa(pal::texto, al), cx + 12.f, cy + kCardH - 44.f, ajustar(vg, g.realName, cw - 24.f));
      float ex = cx + 12.f;
      ex += etiqueta(vg, ex, cy + kCardH - 19.f, modsText(g.nMods), pal::chip, pal::texto, al, true, 12.f) + 6.f;
      const std::string played = playedShort(g);
      if( not played.empty() and ex + 60.f < cx + cw ) etiqueta(vg, ex, cy + kCardH - 19.f, played, pal::chip, pal::tenue, al, true, 12.f);

      // Arriba a la derecha: cuantos estan activos.
      if( g.nActive > 0 ){
        const std::string t = std::to_string(g.nActive) + (g.nActive == 1 ? " activo" : " activos");
        const float tw = anchoTexto(vg, fuentes().texto, 14.f, t), bw = 10.f + 8.f + 6.f + tw + 10.f;
        const float bx = cx + cw - 10.f - bw, by = cy + 10.f;
        caja(vg, bx, by, bw, 26.f, 13.f, nvgRGBAf(pal::fondo.r, pal::fondo.g, pal::fondo.b, 0.78f), al);
        nvgBeginPath(vg);
        nvgCircle(vg, bx + 14.f, by + 13.f, 4.f);
        nvgFillColor(vg, conAlfa(pal::verde, al));
        nvgFill(vg);
        texto(vg, fuentes().texto, 14.f, conAlfa(pal::verde, al), bx + 24.f, by + 14.f, t);
      }

      // Borde: el elegido en acento y mas grueso.
      const bool sel = vi == _selected_ and not _panel_;
      NVGcolor borde = sel ? pal::acento : pal::bordeSuave;
      nvgBeginPath(vg);
      nvgRoundedRect(vg, cx + (sel ? 1.5f : 0.5f), cy + (sel ? 1.5f : 0.5f), cw - (sel ? 3.f : 1.f), kCardH - (sel ? 3.f : 1.f), 14.f);
      nvgStrokeColor(vg, conAlfa(borde, al));
      nvgStrokeWidth(vg, sel ? 3.f : 1.f);
      nvgStroke(vg);
    }
  }
  nvgRestore(vg);

  // Barra de desplazamiento a la derecha.
  if( rows > fullRows ){
    const float tx = x_ + w_ + 14.f;
    caja(vg, tx, y_, 4.f, h_, 2.f, mezcla(pal::fondo, pal::borde, 0.25f), al);
    const float barH = std::max(40.f, h_ * fullRows / float(rows));
    const float barY = y_ + (h_ - barH) * _firstRow_ / float(std::max(1, rows - fullRows));
    caja(vg, tx, barY, 4.f, barH, 2.f, pal::borde, al);
  }
}

namespace {
  struct FilaAjuste{ std::string nombre; std::string valor; bool flecha; std::string ayuda; };
}

void TabGames::drawPanel(NVGcontext* vg, float x_, float y_, float w_, float h_){
  using namespace oqbui;
  const float al = this->alpha;
  const auto& cfg = this->getConfig();
  const std::vector<FilaAjuste> filas = {
      {"Perfil de instalación", cfg.getCurrentPresetName(), false,
       "Dónde se copian los mods al activarlos: " + cfg.getCurrentPreset().installBaseFolder +
       ". Con Atmosphère es el perfil «default» (/atmosphere). Cada juego puede usar otro desde X › Opciones, en su pantalla de mods."},
      {"Carpeta de los mods", cfg.baseFolder, false,
       "Donde SimpleModManager busca tus mods: " + cfg.baseFolder + "/<juego>/<nombre del mod>/<archivos>. "
       "Se cambia en /config/SimpleModManager/parameters.ini."},
      {"Orden de los juegos", kSortNames[this->currentSort()], false,
       "Recientes pone primero lo último que jugaste; A-Z, por nombre; más mods, los que tienen más. A cambia el orden; X también lo cambia en la lista."},
      {"Tema", kTemas[temaActual()].nombre, false,
       "Los colores de la app. Son las mismas gamas de OQB Updater y JKSV BACÁN. A pasa al siguiente; se guarda solo."},
      {"Acerca de", "", true,
       "SimpleModManager " + Toolbox::getAppVersion() + ", versión de OH! QUÉ BACÁN. Autor: Nadrino. Licencia GPL-3.0."},
  };
  _panelSel_ = std::clamp(_panelSel_, 0, int(filas.size()) - 1);

  const float pw = 640.f, px = x_ + w_ - pw;
  caja(vg, px, y_, pw, h_, 0.f, pal::lateral, al);
  caja(vg, px, y_, 1.f, h_, 0.f, pal::borde, al);

  // Titulo en una pildora.
  {
    const float tw = anchoTexto(vg, fuentes().marca, 40.f, "AJUSTES");
    const float bw = 18.f + 24.f + 12.f + tw + 22.f;
    NVGcolor b = pal::borde;
    caja(vg, px + 32.f, y_ + 24.f, bw, 56.f, 28.f, pal::panel, al, &b);
    iconoAjustes(vg, px + 32.f + 18.f + 12.f, y_ + 52.f, conAlfa(pal::texto, al));
    titulo(vg, px + 32.f + 18.f + 24.f + 12.f, y_ + 52.f, "AJUSTES", al);
  }

  // Filas.
  float ry = y_ + 100.f;
  const float rx = px + 32.f, rw = pw - 64.f, rh = 62.f;
  for( int i = 0; i < int(filas.size()); i++ ){
    const auto& f = filas[i];
    const bool sel = i == _panelSel_;
    if( sel ){
      NVGcolor b = pal::acento;
      caja(vg, rx, ry, rw, rh, 12.f, pal::elegido, al, &b, 3.f);
    }
    nvgBeginPath(vg);
    nvgCircle(vg, rx + 26.f, ry + rh / 2, 4.f);
    nvgFillColor(vg, conAlfa(pal::acento, al));
    nvgFill(vg);
    texto(vg, fuentes().texto, 21.f, conAlfa(pal::texto, al), rx + 50.f, ry + rh / 2 + 1.f, f.nombre);
    if( f.flecha ){
      const float ax = rx + rw - 28.f, ay = ry + rh / 2;
      nvgBeginPath(vg);
      nvgMoveTo(vg, ax - 3.f, ay - 7.f);
      nvgLineTo(vg, ax + 4.f, ay);
      nvgLineTo(vg, ax - 3.f, ay + 7.f);
      nvgStrokeColor(vg, conAlfa(pal::verde, al));
      nvgStrokeWidth(vg, 2.2f);
      nvgLineCap(vg, NVG_ROUND);
      nvgLineJoin(vg, NVG_ROUND);
      nvgStroke(vg);
    }
    else{
      const float nameW = anchoTexto(vg, fuentes().texto, 21.f, f.nombre);
      nvgFontFaceId(vg, fuentes().texto);
      nvgFontSize(vg, 19.f);
      const std::string v = ajustar(vg, f.valor, rw - 50.f - nameW - 40.f);
      texto(vg, fuentes().texto, 19.f, conAlfa(pal::verde, al), rx + rw - 20.f, ry + rh / 2 + 1.f, v, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    }
    ry += rh + 6.f;
  }

  // A la izquierda, la tarjeta que explica la opcion elegida.
  {
    const auto& f = filas[_panelSel_];
    const float cw = 320.f, cx = px - 24.f - cw, cy = y_ + 96.f, pad = 24.f;
    nvgFontFaceId(vg, fuentes().texto);
    nvgFontSize(vg, 17.f);
    nvgTextLineHeight(vg, 1.45f);
    float b[4];
    nvgTextBoxBounds(vg, cx + pad, 0, cw - pad * 2, f.ayuda.c_str(), nullptr, b);
    const float ch = 22.f + 24.f + 10.f + (b[3] - b[1]) + 22.f;
    NVGcolor br = pal::borde;
    caja(vg, cx, cy, cw, ch, 16.f, pal::panel, al, &br);
    texto(vg, fuentes().negrita, 19.f, conAlfa(pal::texto, al), cx + pad, cy + 22.f + 12.f, f.nombre);
    nvgFontFaceId(vg, fuentes().texto);
    nvgFontSize(vg, 17.f);
    nvgTextLineHeight(vg, 1.45f);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, conAlfa(pal::cuerpo, al));
    nvgBeginPath(vg);
    nvgTextBox(vg, cx + pad, cy + 22.f + 24.f + 10.f, cw - pad * 2, f.ayuda.c_str(), nullptr);
  }

  const bool info = _panelSel_ == 1;  // la carpeta de los mods solo se muestra
  pie(vg, px + 32.f, px + pw - 32.f, y_ + h_, std::to_string(_panelSel_ + 1) + " / " + std::to_string(filas.size()),
      {{"A", info ? "" : (_panelSel_ == 2 or _panelSel_ == 3 ? "Cambiar" : "Elegir"), true}, {"B", "Cerrar"}}, al);
}

void TabGames::panelAction(){
  switch( _panelSel_ ){
    case 0: {
      std::vector<std::string> names;
      names.reserve(this->getConfig().presetList.size());
      for( auto& preset : this->getConfig().presetList ){ names.emplace_back(preset.name + "  ·  " + preset.installBaseFolder); }
      brls::Dropdown::open("Perfil de instalación", names, [this](int result){
        if( result < 0 ) return;
        this->getConfig().setSelectedPresetIndex(result);
        this->getGameBrowser().getConfigHandler().dumpConfigToFile();
      }, this->getConfig().selectedPresetIndex);
      break;
    }
    case 2:
      this->setSort((this->currentSort() + 1) % 3);
      break;
    case 3: {
      const int t = (oqbui::temaActual() + 1) % oqbui::kNumTemas;
      oqbui::usarPaleta(t);
      oqbui::guardarTema(t);
      oqbui::aplicarTema();
      break;
    }
    case 4: {
      auto* frame = new brls::AppletFrame(true, true);
      frame->setTitle("Acerca de");
      frame->setIcon("romfs:/images/icon_corner.png");
      frame->setFooterText("SimpleModManager v" + Toolbox::getAppVersion());
      frame->setContentView(new TabAbout());
      frame->registerAction("", brls::Key::PLUS, []{ return true; }, true);
      brls::Application::pushView(frame, brls::ViewAnimation::SLIDE_LEFT);
      break;
    }
    default: break;
  }
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
