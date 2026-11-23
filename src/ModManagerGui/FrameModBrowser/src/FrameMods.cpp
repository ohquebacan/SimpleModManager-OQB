//
// OQB: pantalla de mods de un juego (ver FrameMods.h).
//

#include "FrameMods.h"
#include "FrameModBrowser.h"

#include "GenericToolbox.Switch.h"
#include "GenericToolbox.Fs.h"
#include "Logger.h"

#include <algorithm>
#include <cstring>
#include <set>

LoggerInit([]{
  Logger::setUserHeaderStr("[FrameMods]");
});

namespace {

  std::string lowerAscii(std::string s_){
    for( auto& c : s_ ){ c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
    return s_;
  }

  // Que tipo de mod es, por las rutas de sus archivos.
  int kindOfPath(const std::string& path_){
    const std::string p = "/" + lowerAscii(path_);
    if( p.find("/exefs_patches/") != std::string::npos or p.find("/nro_patches/") != std::string::npos ) return 2;
    if( p.size() > 4 and p.compare(p.size() - 4, 4, ".ips") == 0 ) return 2;
    if( p.find("/romfs/") != std::string::npos ) return 0;
    if( p.find("/exefs/") != std::string::npos ) return 1;
    return 4;
  }

  // Geometria de las tarjetas (pantalla de 1280 x 720, como la maqueta).
  constexpr int kCols = 2;
  constexpr float kGap = 14.f, kCardH = 118.f;

}

ModsView::ModsView(GuiModManager* manager_, uint8_t* icon_, std::string title_)
    : _manager_(manager_), _icon_(icon_), _title_(std::move(title_)) {
  this->scanMods();
  this->refreshStatus();
  this->rebuild();

  this->registerAction("", brls::Key::DUP, [this]{ this->move(-kCols); return true; }, true);
  this->registerAction("", brls::Key::DDOWN, [this]{
    if( _selected_ + kCols >= int(_visible_.size()) and _selected_ / kCols < (int(_visible_.size()) - 1) / kCols ){
      _selected_ = int(_visible_.size()) - 1;
      return true;
    }
    this->move(kCols);
    return true;
  }, true);
  this->registerAction("", brls::Key::DLEFT, [this]{ this->move(-1); return true; }, true);
  this->registerAction("", brls::Key::DRIGHT, [this]{ this->move(1); return true; }, true);
  this->registerAction("", brls::Key::L, [this]{ this->move(-8); return true; }, true);
  this->registerAction("", brls::Key::R, [this]{ this->move(8); return true; }, true);
  this->registerAction("", brls::Key::A, [this]{ this->toggleSelected(); return true; }, true);
  this->registerAction("", brls::Key::X, [this]{ this->openMore(); return true; }, true);
  this->registerAction("", brls::Key::Y, [this]{ this->openSearch(); return true; }, true);
  this->registerAction("", brls::Key::MINUS, [this]{
    if( _filter_.empty() ) return true;
    _filter_.clear();
    this->rebuild();
    return true;
  }, true);
}

ModsView::~ModsView(){
  if( _image_ > 0 ) nvgDeleteImage(brls::Application::getNVGContext(), _image_);
  this->freeThumbs();
}

/* Los archivos de cada mod: cuantos son y que mods comparten alguno. Dos mods
   chocan si tienen un archivo con la misma ruta: el que se activa despues pisa
   al otro, y apagar uno borra ese archivo del otro. */
void ModsView::freeThumbs(){
  NVGcontext* vg = brls::Application::getNVGContext();
  for( auto& r : _rows_ ){ if( r.image > 0 ) nvgDeleteImage(vg, r.image); r.image = -1; }
}

void ModsView::scanMods(){
  auto& mm = _manager_->getGameBrowser().getModManager();
  auto& modList = mm.getModList();
  this->freeThumbs();
  _rows_.clear();
  /* Miniaturas que deja SimpleModDownloader al descargar un mod:
     /config/SimpleModManager/miniaturas/<carpeta del juego>/<carpeta del mod>.jpg */
  std::string gameDir = mm.getGameFolderPath();
  while( not gameDir.empty() and gameDir.back() == '/' ) gameDir.pop_back();
  const std::string thumbDir = "/config/SimpleModManager/miniaturas/" + GenericToolbox::getFileName(gameDir) + "/";
  std::map<std::string, std::vector<int>> owners;
  for( size_t i = 0; i < modList.size(); i++ ){
    Row r;
    r.name = modList[i].modName;
    const std::string modPath = GenericToolbox::joinPath(mm.getGameFolderPath(), r.name);
    int kinds[5] = {0, 0, 0, 0, 0};
    for( auto& f : GenericToolbox::lsFilesRecursive(modPath) ){
      const std::string name = GenericToolbox::getFileName(f);
      if( name.empty() or name[0] == '.' ) continue;  // .DS_Store, ._* y similares
      r.nFiles++;
      kinds[kindOfPath(f)]++;
      owners[f].push_back(int(i));
    }
    int distinct = 0;
    r.kind = 4;
    for( int k = 0; k < 3; k++ ){ if( kinds[k] > 0 ){ distinct++; r.kind = k; } }
    if( distinct > 1 ) r.kind = 3;
    if( GenericToolbox::isFile(thumbDir + r.name + ".jpg") ) r.thumb = thumbDir + r.name + ".jpg";
    _rows_.emplace_back(std::move(r));
  }
  std::map<int, std::set<int>> pairs;
  for( auto& o : owners ){
    if( o.second.size() < 2 ) continue;
    for( int a : o.second ) for( int b : o.second ) if( a != b ) pairs[a].insert(b);
  }
  for( auto& p : pairs ) for( int b : p.second ) _rows_[p.first].clashes.push_back(_rows_[b].name);
}

void ModsView::openSearch(){
  brls::Swkbd::openForText([this](std::string text){
    _filter_ = text;
    this->rebuild();
  }, "Buscar mod", "", 40, _filter_);
}

void ModsView::rebuild(){
  _visible_.clear();
  const std::string f = lowerAscii(_filter_);
  for( size_t i = 0; i < _rows_.size(); i++ ){
    if( f.empty() or lowerAscii(_rows_[i].name).find(f) != std::string::npos ) _visible_.push_back(int(i));
  }
  _selected_ = 0;
  _first_ = 0;
}

void ModsView::move(int delta_){
  if( _visible_.empty() ) return;
  _selected_ = std::clamp(_selected_ + delta_, 0, int(_visible_.size()) - 1);
}

void ModsView::refreshStatus(){
  auto& mm = _manager_->getGameBrowser().getModManager();
  const auto preset = mm.fetchCurrentPreset().name;
  _status_.clear();
  for( auto& m : mm.getModList() ){ _status_[m.modName] = {m.getStatusFraction(preset), m.getStatus(preset)}; }
}

double ModsView::fractionOf(const std::string& modName_) const{
  auto it = _status_.find(modName_);
  return it == _status_.end() ? 0 : it->second.first;
}

std::string ModsView::statusOf(const std::string& modName_) const{
  auto it = _status_.find(modName_);
  return it == _status_.end() ? "" : it->second.second;
}

void ModsView::toggleSelected(){
  if( _visible_.empty() ) return;
  const Row& r = _rows_[_visible_[_selected_]];
  if( this->fractionOf(r.name) > 0 ){
    _manager_->startRemoveModThread( r.name );
    return;
  }
  std::vector<std::string> active;
  for( auto& other : r.clashes ){ if( this->fractionOf(other) > 0 ) active.push_back(other); }
  if( active.empty() ){
    _manager_->startApplyModThread( r.name );
    return;
  }
  std::string list;
  for( size_t i = 0; i < active.size(); i++ ) list += (i ? ", " : "") + ("\"" + active[i] + "\"");
  auto* dialog = new brls::Dialog("\"" + r.name + "\" toca los mismos archivos que " + list +
      ", que está activo. Si lo activas, esos archivos quedan con este mod y el otro queda a medias. "
      "Lo más seguro es apagar el otro primero.");
  const std::string name = r.name;
  dialog->addButton("Activar igual", [this, name, dialog](brls::View* view){
    dialog->close();
    _manager_->startApplyModThread( name );
  });
  dialog->addButton("Cancelar", [dialog](brls::View* view){ dialog->close(); });
  dialog->setCancelable(true);
  dialog->open();
}

void ModsView::openMore(){
  auto* frame = new FrameModBrowser( _manager_ );
  frame->setTitle( _title_ );
  frame->registerAction("", brls::Key::PLUS, []{return true;}, true);
  brls::Application::pushView(frame, brls::ViewAnimation::SLIDE_LEFT);
}

void ModsView::draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height, brls::Style* style, brls::FrameContext* ctx){
  using namespace oqbui;
  const float al = this->alpha;
  caja(vg, x, y, width, height, 0.f, pal::fondo, al);
  const float left = x + 32.f, right = x + width - 32.f;

  auto& mm = _manager_->getGameBrowser().getModManager();

  // Al terminar una tarea (activar, apagar, revisar), se toma el estado nuevo.
  if( _manager_->isTriggerUpdateModsDisplayedStatus() ){
    _manager_->setTriggerUpdateModsDisplayedStatus(false);
    this->refreshStatus();
  }

  /* La primera vez que se abre un juego no hay estado guardado de sus mods:
     se revisan una vez (con su ventana de progreso), como hacia la app antes. */
  if( not _checked_ ){
    _checked_ = true;
    bool any = false;
    for( auto& r : _rows_ ){ if( not this->statusOf(r.name).empty() ){ any = true; break; } }
    if( not any and not _rows_.empty() ){ _manager_->startCheckAllModsThread(); }
  }

  // Arriba: el juego en una pildora (icono, nombre, activos y perfil) y la hora.
  {
    const float top = y + 24.f, h = 56.f, cy = top + h / 2;
    const float wEstado = estado(vg, right, top, al);
    int active = 0;
    for( auto& r : _rows_ ){ if( this->fractionOf(r.name) == 1 ) active++; }
    const std::string activos = std::to_string(active) + " de " + std::to_string(_rows_.size()) + (active == 1 ? " activo" : " activos");
    const std::string perfil = "Perfil: " + mm.fetchCurrentPreset().name;
    const float wAct = anchoTexto(vg, fuentes().mono, 13.f, activos) + 18.f;
    const float wPerf = anchoTexto(vg, fuentes().mono, 13.f, perfil) + 18.f;
    std::string busca;
    if( not _filter_.empty() ) busca = "«" + _filter_ + "» · " + std::to_string(_visible_.size()) + " de " + std::to_string(_rows_.size());
    const float wBusca = busca.empty() ? 0.f : anchoTexto(vg, fuentes().texto, 14.f, busca) + 18.f + 10.f;

    const float maxW = right - wEstado - 20.f - left;
    const bool conIcono = _icon_ != nullptr;
    const float fijo = (conIcono ? 8.f + 40.f + 12.f : 22.f) + 12.f + wAct + 10.f + wPerf + wBusca + 22.f;
    nvgFontFaceId(vg, fuentes().negrita);
    nvgFontSize(vg, 20.f);
    const std::string name = ajustar(vg, _title_, std::max(80.f, maxW - fijo));
    const float wName = anchoTexto(vg, fuentes().negrita, 20.f, name);
    const float w = std::min(maxW, fijo + wName);
    NVGcolor b = pal::borde;
    caja(vg, left, top, w, h, h / 2, pal::panel, al, &b);
    float cx = left + (conIcono ? 8.f : 22.f);
    if( conIcono ){
      if( _image_ == 0 ) _image_ = nvgCreateImageMem(vg, 0, _icon_, 0x20000);
      nvgBeginPath(vg);
      nvgCircle(vg, cx + 20.f, cy, 20.f);
      if( _image_ > 0 ) nvgFillPaint(vg, nvgImagePattern(vg, cx, cy - 20.f, 40.f, 40.f, 0, _image_, al));
      else nvgFillColor(vg, conAlfa(colorDe(_title_), al));
      nvgFill(vg);
      cx += 40.f + 12.f;
    }
    texto(vg, fuentes().negrita, 20.f, conAlfa(pal::texto, al), cx, cy + 1.f, name);
    cx += wName + 12.f;
    cx += etiqueta(vg, cx, cy, activos, active > 0 ? nvgRGBAf(pal::verde.r, pal::verde.g, pal::verde.b, 0.12f) : pal::chip, active > 0 ? pal::verde : pal::tenue, al) + 10.f;
    cx += etiqueta(vg, cx, cy, perfil, nvgRGBAf(pal::acento.r, pal::acento.g, pal::acento.b, 0.14f), pal::acento, al) + 10.f;
    if( not busca.empty() ) etiqueta(vg, cx, cy, busca, pal::chip, pal::cuerpo, al, false, 14.f);
  }

  const float gridTop = y + 100.f, gridH = height - 100.f - 84.f;
  const std::string contador = _visible_.empty() ? "" : std::to_string(_selected_ + 1) + " / " + std::to_string(_visible_.size());

  if( _rows_.empty() ){
    texto(vg, fuentes().negrita, 22.f, conAlfa(pal::texto, al), left, gridTop + 24.f, "Este juego todavía no tiene mods");
    nvgFontFaceId(vg, fuentes().texto);
    nvgFontSize(vg, 17.f);
    nvgTextLineHeight(vg, 1.4f);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, conAlfa(pal::tenue, al));
    nvgBeginPath(vg);
    const std::string help = "Cada mod va en su carpeta: " + mm.getGameFolderPath() + "/<nombre del mod>/<archivos como van en la consola>. SimpleModDownloader los deja ahí al descargarlos.";
    nvgTextBox(vg, left, gridTop + 52.f, right - left, help.c_str(), nullptr);
    pie(vg, left, right, y + height, "", {{"X", "Grupos y opciones"}, {"B", "Volver"}}, al);
    return;
  }
  if( _visible_.empty() ){
    texto(vg, fuentes().texto, 18.f, conAlfa(pal::tenue, al), left, gridTop + 24.f, "Ningún mod coincide con «" + _filter_ + "».");
    pie(vg, left, right, y + height, "", {{"Y", "Buscar otra cosa", true}, {"−", "Ver todos"}, {"B", "Volver"}}, al);
    return;
  }

  // Tarjetas.
  const float cw = (right - left - kGap * (kCols - 1)) / kCols, step = kCardH + kGap;
  const int rows = (int(_visible_.size()) + kCols - 1) / kCols;
  const int fullRows = std::max(1, int((gridH + kGap) / step));
  const int selRow = _selected_ / kCols;
  if( selRow < _first_ ) _first_ = selRow;
  if( selRow >= _first_ + fullRows ) _first_ = selRow - fullRows + 1;

  static const char* kKindText[] = {"romfs", "exefs", ".ips", "mixto", "otro"};
  const NVGcolor kindColor[] = {pal::acento, pal::oro, pal::verde, pal::cuerpo, pal::tenue};

  nvgSave(vg);
  nvgScissor(vg, left - 4.f, gridTop - 4.f, right - left + 8.f, gridH + 4.f);
  for( int r = _first_; r < rows and (r - _first_) * step < gridH; r++ ){
    for( int c = 0; c < kCols; c++ ){
      const int vi = r * kCols + c;
      if( vi >= int(_visible_.size()) ) break;
      const Row& row = _rows_[_visible_[vi]];
      const float cx = left + c * (cw + kGap), cy = gridTop + (r - _first_) * step;
      const bool sel = vi == _selected_;
      const double frac = this->fractionOf(row.name);
      const std::string status = this->statusOf(row.name);
      const bool on = frac > 0;

      NVGcolor borde = sel ? pal::acento : pal::bordeSuave;
      caja(vg, cx, cy, cw, kCardH, 14.f, sel ? pal::elegido : pal::panel, al, &borde, sel ? 3.f : 1.f);

      // El tipo de mod en un cuadro.
      const float tile = 72.f, tx = cx + 16.f, ty = cy + (kCardH - tile) / 2;
      const NVGcolor kc = kindColor[row.kind];
      Row& rw = _rows_[_visible_[vi]];
      if( not rw.thumb.empty() and rw.image < 0 ) rw.image = nvgCreateImage(vg, rw.thumb.c_str(), 0);
      if( rw.image > 0 ){
        // La miniatura del mod (de SimpleModDownloader), recortada al cuadro.
        int iw = 1, ih = 1;
        nvgImageSize(vg, rw.image, &iw, &ih);
        const float sc = std::max(tile / float(iw), tile / float(ih));
        const float dw = iw * sc, dh = ih * sc;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, tx, ty, tile, tile, 12.f);
        nvgFillPaint(vg, nvgImagePattern(vg, tx + (tile - dw) / 2, ty + (tile - dh) / 2, dw, dh, 0, rw.image, al));
        nvgFill(vg);
      }
      else{
        caja(vg, tx, ty, tile, tile, 12.f, nvgRGBAf(kc.r, kc.g, kc.b, 0.15f), al);
        texto(vg, fuentes().mono, 15.f, conAlfa(kc, al), tx + tile / 2, ty + tile / 2 + 1.f, kKindText[row.kind], NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      }

      // El estado a la derecha.
      std::string label;
      NVGcolor bg = mezcla(pal::fondo, pal::borde, 0.25f), fg = pal::tenue;
      if( status.empty() ) label = "SIN REVISAR";
      else if( status == "NO FILE" ) label = "SIN ARCHIVOS";
      else if( frac == 1 ){ label = "ACTIVO"; bg = nvgRGBAf(pal::verde.r, pal::verde.g, pal::verde.b, 0.14f); fg = pal::verde; }
      else if( frac > 0 ){
        const auto open = status.find('(');
        label = "A MEDIAS" + (open == std::string::npos ? std::string() : " " + status.substr(open));
        bg = nvgRGBAf(pal::oro.r, pal::oro.g, pal::oro.b, 0.14f); fg = pal::oro;
      }
      else label = "APAGADO";
      const float sw = anchoTexto(vg, fuentes().negrita, 14.f, label) + 24.f;
      const float sx = cx + cw - 16.f - sw;
      caja(vg, sx, cy + kCardH / 2 - 14.f, sw, 28.f, 14.f, bg, al);
      texto(vg, fuentes().negrita, 14.f, conAlfa(fg, al), sx + sw / 2, cy + kCardH / 2 + 1.f, label, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

      // Nombre y etiquetas.
      const float nx = tx + tile + 16.f, nw = sx - 16.f - nx;
      nvgFontFaceId(vg, fuentes().negrita);
      nvgFontSize(vg, 19.f);
      texto(vg, fuentes().negrita, 19.f, conAlfa(pal::texto, al), nx, cy + kCardH / 2 - 15.f, ajustar(vg, row.name, nw));
      float ex = nx;
      if( rw.image > 0 ) ex += etiqueta(vg, ex, cy + kCardH / 2 + 18.f, kKindText[row.kind], nvgRGBAf(kc.r, kc.g, kc.b, 0.15f), kc, al, true, 12.f) + 6.f;
      ex += etiqueta(vg, ex, cy + kCardH / 2 + 18.f, std::to_string(row.nFiles) + (row.nFiles == 1 ? " archivo" : " archivos"), pal::chip, pal::tenue, al, true, 12.f) + 6.f;
      if( not row.clashes.empty() ){
        // Si choca con uno activo, se nombra ese; si no, el primero.
        std::string con = row.clashes[0];
        int activos = 0;
        for( auto& o : row.clashes ){ if( this->fractionOf(o) > 0 ){ if( activos == 0 ) con = o; activos++; } }
        std::string t = "Choca con " + con + (row.clashes.size() > 1 ? " y " + std::to_string(row.clashes.size() - 1) + " más" : "");
        nvgFontFaceId(vg, fuentes().texto);
        nvgFontSize(vg, 13.f);
        t = ajustar(vg, t, std::max(40.f, nx + nw - ex - 18.f));
        const bool fuerte = not on and activos > 0;  // activarlo pisaria a otro
        etiqueta(vg, ex, cy + kCardH / 2 + 18.f, t, nvgRGBAf(pal::oro.r, pal::oro.g, pal::oro.b, fuerte ? 0.22f : 0.12f), pal::oro, al, false, 13.f);
      }
    }
  }
  nvgRestore(vg);

  if( rows > fullRows ){
    const float tx = right + 14.f;
    caja(vg, tx, gridTop, 4.f, gridH, 2.f, mezcla(pal::fondo, pal::borde, 0.25f), al);
    const float barH = std::max(40.f, gridH * fullRows / float(rows));
    const float barY = gridTop + (gridH - barH) * _first_ / float(std::max(1, rows - fullRows));
    caja(vg, tx, barY, 4.f, barH, 2.f, pal::borde, al);
  }

  const bool selOn = this->fractionOf(_rows_[_visible_[_selected_]].name) > 0;
  pie(vg, left, right, y + height, contador, {
      {"A", selOn ? "Apagar" : "Activar", true},
      {"Y", "Buscar"},
      {"X", "Más"},
      {"−", _filter_.empty() ? "" : "Quitar búsqueda"},
      {"B", "Volver"}}, al);
}

FrameMods::FrameMods(GuiModManager* manager_, const std::string& title_, const uint8_t* icon_) : FrameSinBarras() {
  // El icono viene de la lista de juegos (su cache); si no, se busca como antes.
  if( icon_ != nullptr ){
    _icon_ = new uint8_t[0x20000];
    std::memcpy(_icon_, icon_, 0x20000);
  }
  else{
    const std::string tid = GenericToolbox::Switch::Utils::lookForTidInSubFolders( manager_->getGameBrowser().getModManager().getGameFolderPath(), 5 );
    _icon_ = GenericToolbox::Switch::Utils::getIconFromTitleId( tid );
  }
  this->setContentView( new ModsView(manager_, _icon_, title_) );
}

FrameMods::~FrameMods(){
  delete[] _icon_;
}
