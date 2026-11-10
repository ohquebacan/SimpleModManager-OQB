//
// Created by Adrien BLANCHET on 21/06/2020.
// OQB: rediseno de la pestana de juegos (ver TabGames.h).
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

  std::string playedText(const GameItem& g_){
    if( not g_.playedKnown ) return "";
    const uint32_t m = g_.minutesAgo;
    if( m < 60 ) return "Jugado hace un rato";
    if( m < 60 * 24 ) return "Jugado hace " + std::to_string(m / 60) + " h";
    const uint32_t d = m / (60 * 24);
    if( d == 1 ) return "Jugado ayer";
    if( d < 60 ) return "Jugado hace " + std::to_string(d) + " días";
    return "Jugado hace " + std::to_string(d / 30) + " meses";
  }

  std::string lowerAscii(std::string s_){
    for( auto& c : s_ ){ c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
    return s_;
  }

  std::string modsText(int n_){ return std::to_string(n_) + (n_ == 1 ? " mod" : " mods"); }

  // Texto que entra en un ancho; si no, se corta con "…" (sin partir un caracter UTF-8).
  std::string fit(NVGcontext* vg, const std::string& text_, float width_){
    if( nvgTextBounds(vg, 0, 0, text_.c_str(), nullptr, nullptr) <= width_ ) return text_;
    std::string s = text_;
    while( not s.empty() ){
      do s.pop_back(); while( not s.empty() and (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80 );
      if( not s.empty() ) s.pop_back();
      const std::string test = s + "…";
      if( nvgTextBounds(vg, 0, 0, test.c_str(), nullptr, nullptr) <= width_ ) return test;
    }
    return "…";
  }

  // Colores fijos de estado (sirven en tema claro y oscuro).
  const NVGcolor kGreenBg = nvgRGBA(52, 213, 131, 46);
  const NVGcolor kGreenText = nvgRGB(46, 170, 110);

  float pill(NVGcontext* vg, float x_, float cy_, const std::string& text_, NVGcolor bg_, NVGcolor fg_, float alpha_, float size_ = 15.f){
    nvgFontSize(vg, size_);
    const float w = nvgTextBounds(vg, 0, 0, text_.c_str(), nullptr, nullptr) + 22.f, h = size_ + 11.f;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x_, cy_ - h / 2, w, h, h / 2);
    nvgFillColor(vg, nvgRGBAf(bg_.r, bg_.g, bg_.b, bg_.a * alpha_));
    nvgFill(vg);
    nvgFillColor(vg, nvgRGBAf(fg_.r, fg_.g, fg_.b, alpha_));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgBeginPath(vg);
    nvgText(vg, x_ + w / 2, cy_ + 1, text_.c_str(), nullptr);
    return w;
  }

}

TabGames::TabGames(FrameRoot* owner_) : _owner_(owner_) {
  LogWarning << "Building game tab..." << std::endl;

  auto gameList = this->getGameBrowser().getSelector().getEntryList();
  LogInfo << "Adding " << gameList.size() << " game folders..." << std::endl;

  _gameList_.reserve( gameList.size() );
  for( auto& gameEntry : gameList ){
    LogScopeIndent;
    LogInfo << "Adding game folder: \"" << gameEntry.title << "\"" << std::endl;

    GameItem g;
    g.title = gameEntry.title;
    const std::string gamePath{GenericToolbox::joinPath(this->getConfig().baseFolder, gameEntry.title)};
    g.nMods = int( GenericToolbox::lsDirs(gamePath).size() );
    g.titleId = GenericToolbox::Switch::Utils::lookForTidInSubFolders(gamePath);

    /* El nombre real del juego, no el de la carpeta: las carpetas que creo
       SimpleModDownloader con un juego nuevo pueden tener "?" o simbolos raros
       (nombres comprimidos de la NACP), y asi no habia forma de encontrarlo. */
    infoFromTid(g.titleId, g.realName, g.icon);
    if( g.realName.empty() ){ g.realName = g.title; }
    _gameList_.emplace_back(std::move(g));
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

  this->registerAction("", brls::Key::DUP, [this]{ this->move(-1); return true; }, true);
  this->registerAction("", brls::Key::DDOWN, [this]{ this->move(1); return true; }, true);
  this->registerAction("", brls::Key::L, [this]{ this->move(-8); return true; }, true);
  this->registerAction("", brls::Key::R, [this]{ this->move(8); return true; }, true);
  this->registerAction("Ver mods", brls::Key::A, [this]{ this->openSelected(); return true; });
  this->registerAction("Buscar", brls::Key::Y, [this]{
    brls::Swkbd::openForText([this](std::string text){
      _filter_ = text;
      this->rebuildList();
    }, "Buscar juego", "", 40, _filter_);
    return true;
  });
  this->registerAction("Ordenar", brls::Key::X, [this]{
    auto& sort = this->getConfig().sortGameList;
    switch( sort.value ){
      case ConfigHolder::SortGameList::Recent: sort = ConfigHolder::SortGameList::Alphabetical; break;
      case ConfigHolder::SortGameList::Alphabetical: sort = ConfigHolder::SortGameList::NbMods; break;
      default: sort = ConfigHolder::SortGameList::Recent; break;
    }
    this->getGameBrowser().getConfigHandler().dumpConfigToFile();
    this->rebuildList();
    return true;
  });
  this->registerAction("Quitar búsqueda", brls::Key::MINUS, [this]{
    if( _filter_.empty() ) return true;
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
  _first_ = 0;
  this->updateHints();
}

void TabGames::updateHints(){
  this->updateActionHint(brls::Key::MINUS, _filter_.empty() ? "" : "Quitar búsqueda");
}

void TabGames::move(int delta_){
  if( _visible_.empty() ) return;
  _selected_ = std::clamp(_selected_ + delta_, 0, int(_visible_.size()) - 1);
}

void TabGames::openSelected(){
  if( _visible_.empty() ) return;
  const GameItem& g = _gameList_[_visible_[_selected_]];
  LogWarning << "Opening \"" << g.title << "\"" << std::endl;
  getGameBrowser().selectGame( g.title );
  auto* modsBrowser = new FrameModBrowser( &_owner_->getGuiModManager() );
  modsBrowser->setTitle( g.realName );
  brls::Application::pushView(modsBrowser, brls::ViewAnimation::SLIDE_LEFT);
  modsBrowser->registerAction("", brls::Key::PLUS, []{return true;}, true);
  modsBrowser->updateActionHint(brls::Key::PLUS, "");
}

void TabGames::draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height, brls::Style* style, brls::FrameContext* ctx){
  const NVGcolor text = this->a(ctx->theme->textColor);
  const NVGcolor faint = this->a(ctx->theme->descriptionColor);
  const NVGcolor accent = this->a(ctx->theme->highlightColor1);
  const NVGcolor veil = this->a(nvgRGBAf(ctx->theme->textColor.r, ctx->theme->textColor.g, ctx->theme->textColor.b, 0.07f));
  nvgFontFaceId(vg, ctx->fontStash->regular);

  const float pad = 30.f;
  const float left = x + pad, top = y + 22.f;
  const float panelW = std::min(330.f, width * 0.36f);
  const float listW = width - pad * 2 - panelW - 28.f;
  const float right = left + listW + 28.f;

  // Sin carpetas de juegos: como poner los mods.
  if( _gameList_.empty() ){
    const std::string base = this->getConfig().baseFolder;
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, text);
    nvgFontSize(vg, 24.f);
    nvgBeginPath(vg);
    nvgText(vg, left, top + 20.f, ("No hay juegos con mods en " + base).c_str(), nullptr);
    nvgFontSize(vg, 18.f);
    nvgTextLineHeight(vg, 1.4f);
    nvgFillColor(vg, faint);
    nvgBeginPath(vg);
    const std::string help = "Copia cada mod así: " + base + "/<juego>/<nombre del mod>/<archivos del mod>. "
                             "SimpleModDownloader los deja en ese lugar al descargarlos. "
                             "La carpeta se puede cambiar en /config/SimpleModManager/parameters.ini.";
    nvgTextBox(vg, left, top + 62.f, width - pad * 2, help.c_str(), nullptr);
    return;
  }

  // Buscador y orden.
  {
    const float h = 40.f;
    const char* labels[] = {"Recientes", "A-Z", "Más mods"};
    const auto sortValue = this->getConfig().sortGameList.value;
    const int current = sortValue == ConfigHolder::SortGameList::Alphabetical ? 1 : sortValue == ConfigHolder::SortGameList::NbMods ? 2 : 0;
    nvgFontSize(vg, 16.f);
    float chipsW = 0.f;
    for( auto* l : labels ) chipsW += nvgTextBounds(vg, 0, 0, l, nullptr, nullptr) + 30.f + 8.f;
    const float boxW = width - pad * 2 - chipsW - 12.f;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, left, top, boxW, h, 10.f);
    nvgFillColor(vg, veil);
    nvgFill(vg);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFontSize(vg, 17.f);
    nvgFillColor(vg, _filter_.empty() ? faint : text);
    nvgBeginPath(vg);
    const std::string search = _filter_.empty() ? "Buscar juego (Y)" : "Buscando: " + _filter_ + "   ·   " + std::to_string(_visible_.size()) + " de " + std::to_string(_gameList_.size());
    nvgText(vg, left + 16.f, top + h / 2 + 1, fit(vg, search, boxW - 32.f).c_str(), nullptr);

    float cx = left + boxW + 12.f;
    for( int i = 0; i < 3; i++ ){
      nvgFontSize(vg, 16.f);
      const float w = nvgTextBounds(vg, 0, 0, labels[i], nullptr, nullptr) + 30.f;
      nvgBeginPath(vg);
      nvgRoundedRect(vg, cx, top + 3, w, h - 6, (h - 6) / 2);
      if( i == current ){
        nvgFillColor(vg, nvgRGBAf(accent.r, accent.g, accent.b, 0.22f * this->alpha));
        nvgFill(vg);
      }
      else{
        nvgStrokeColor(vg, faint);
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);
      }
      nvgFillColor(vg, i == current ? accent : faint);
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgBeginPath(vg);
      nvgText(vg, cx + w / 2, top + h / 2 + 1, labels[i], nullptr);
      cx += w + 8.f;
    }
  }

  const float listTop = top + 58.f;
  const float rowH = 76.f;
  const int fits = std::max(1, int((y + height - 10.f - listTop) / rowH));
  if( _selected_ < _first_ ) _first_ = _selected_;
  if( _selected_ >= _first_ + fits ) _first_ = _selected_ - fits + 1;

  if( _visible_.empty() ){
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, faint);
    nvgFontSize(vg, 18.f);
    nvgBeginPath(vg);
    nvgText(vg, left + 4.f, listTop + 10.f, ("Ningún juego coincide con «" + _filter_ + "». Pulsa Y para buscar otra cosa o − para ver todos.").c_str(), nullptr);
    return;
  }

  // Lista.
  for( int k = 0; k < fits and _first_ + k < int(_visible_.size()); k++ ){
    const int vi = _first_ + k;
    const int gi = _visible_[vi];
    GameItem& g = _gameList_[gi];
    const float ry = listTop + k * rowH;

    if( vi == _selected_ ){
      nvgBeginPath(vg);
      nvgRoundedRect(vg, left, ry + 2, listW, rowH - 4, 12.f);
      nvgFillColor(vg, nvgRGBAf(accent.r, accent.g, accent.b, 0.13f * this->alpha));
      nvgFill(vg);
      nvgBeginPath(vg);
      nvgRoundedRect(vg, left, ry + 12, 4.f, rowH - 24, 2.f);
      nvgFillColor(vg, accent);
      nvgFill(vg);
    }

    auto it = _images_.find(gi);
    if( it == _images_.end() ){
      it = _images_.emplace(gi, g.icon ? nvgCreateImageMem(vg, 0, g.icon, 0x20000) : 0).first;
    }
    const float side = 54.f, ix = left + 14.f, iy = ry + (rowH - side) / 2;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, ix, iy, side, side, 9.f);
    if( it->second > 0 ) nvgFillPaint(vg, nvgImagePattern(vg, ix, iy, side, side, 0, it->second, this->alpha));
    else nvgFillColor(vg, veil);
    nvgFill(vg);

    float pillW = 0.f;
    if( g.nActive > 0 ){
      const std::string p = std::to_string(g.nActive) + (g.nActive == 1 ? " activo" : " activos");
      nvgFontSize(vg, 15.f);
      pillW = nvgTextBounds(vg, 0, 0, p.c_str(), nullptr, nullptr) + 22.f;
      pill(vg, left + listW - 14.f - pillW, ry + rowH / 2, p, kGreenBg, kGreenText, this->alpha);
    }
    const float tx = ix + side + 14.f, tw = left + listW - 28.f - pillW - tx;
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFontSize(vg, 20.f);
    nvgFillColor(vg, text);
    nvgBeginPath(vg);
    nvgText(vg, tx, ry + 15.f, fit(vg, g.realName, tw).c_str(), nullptr);
    std::string sub = modsText(g.nMods);
    const std::string played = playedText(g);
    if( not played.empty() ) sub += "  ·  " + played;
    nvgFontSize(vg, 15.f);
    nvgFillColor(vg, faint);
    nvgBeginPath(vg);
    nvgText(vg, tx, ry + 44.f, fit(vg, sub, tw).c_str(), nullptr);
  }

  // Barra de desplazamiento.
  if( int(_visible_.size()) > fits ){
    const float trackH = fits * rowH - 8.f;
    const float barH = std::max(30.f, trackH * fits / float(_visible_.size()));
    const float barY = listTop + 4.f + (trackH - barH) * _first_ / float(_visible_.size() - fits);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, left + listW + 8.f, barY, 4.f, barH, 2.f);
    nvgFillColor(vg, veil);
    nvgFill(vg);
  }

  // Panel del juego elegido.
  const GameItem& g = _gameList_[_visible_[_selected_]];
  float py = listTop;
  {
    const float side = 120.f;
    auto it = _images_.find(_visible_[_selected_]);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, right, py, side, side, 16.f);
    if( it != _images_.end() and it->second > 0 ) nvgFillPaint(vg, nvgImagePattern(vg, right, py, side, side, 0, it->second, this->alpha));
    else nvgFillColor(vg, veil);
    nvgFill(vg);
    py += side + 16.f;

    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFontSize(vg, 22.f);
    nvgTextLineHeight(vg, 1.2f);
    nvgFillColor(vg, text);
    float b[4];
    nvgTextBoxBounds(vg, right, py, panelW, g.realName.c_str(), nullptr, b);
    nvgBeginPath(vg);
    nvgTextBox(vg, right, py, panelW, g.realName.c_str(), nullptr);
    py += std::min(b[3] - b[1], 60.f) + 6.f;
    if( not g.titleId.empty() ){
      nvgFontSize(vg, 14.f);
      nvgFillColor(vg, faint);
      nvgBeginPath(vg);
      nvgText(vg, right, py, g.titleId.c_str(), nullptr);
      py += 26.f;
    }

    // Dos cuadros: mods y activos.
    const float boxW = (panelW - 10.f) / 2, boxH = 62.f;
    const std::string values[] = {std::to_string(g.nMods), g.nActive < 0 ? "—" : std::to_string(g.nActive)};
    const char* names[] = {"Mods", "Activos"};
    for( int i = 0; i < 2; i++ ){
      const float bx = right + i * (boxW + 10.f);
      nvgBeginPath(vg);
      nvgRoundedRect(vg, bx, py, boxW, boxH, 10.f);
      nvgFillColor(vg, veil);
      nvgFill(vg);
      nvgFontSize(vg, 14.f);
      nvgFillColor(vg, faint);
      nvgBeginPath(vg);
      nvgText(vg, bx + 12.f, py + 9.f, names[i], nullptr);
      nvgFontSize(vg, 24.f);
      nvgFillColor(vg, i == 1 and g.nActive > 0 ? this->a(kGreenText) : text);
      nvgBeginPath(vg);
      nvgText(vg, bx + 12.f, py + 29.f, values[i].c_str(), nullptr);
    }
    py += boxH + 14.f;

    const std::string played = playedText(g);
    nvgFontSize(vg, 16.f);
    nvgFillColor(vg, faint);
    nvgBeginPath(vg);
    nvgText(vg, right, py, played.empty() ? "Sin datos de cuándo se jugó" : played.c_str(), nullptr);
    py += 30.f;
    if( g.nActive < 0 ){
      nvgFontSize(vg, 14.f);
      nvgTextLineHeight(vg, 1.3f);
      nvgBeginPath(vg);
      nvgTextBox(vg, right, py, panelW, "Los activos se ven después de abrir el juego una vez.", nullptr);
    }
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
