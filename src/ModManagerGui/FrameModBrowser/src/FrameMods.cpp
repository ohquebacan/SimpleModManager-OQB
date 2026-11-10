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

  const NVGcolor kGreenBg = nvgRGBA(52, 213, 131, 46);
  const NVGcolor kGreen = nvgRGB(46, 170, 110);
  const NVGcolor kAmberBg = nvgRGBA(230, 185, 92, 50);
  const NVGcolor kAmber = nvgRGB(196, 140, 40);

  float pillWidth(NVGcontext* vg, const std::string& text_, float size_ = 15.f){
    nvgFontSize(vg, size_);
    return nvgTextBounds(vg, 0, 0, text_.c_str(), nullptr, nullptr) + 22.f;
  }

  void pill(NVGcontext* vg, float x_, float cy_, const std::string& text_, NVGcolor bg_, NVGcolor fg_, float alpha_, float size_ = 15.f){
    const float w = pillWidth(vg, text_, size_), h = size_ + 11.f;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x_, cy_ - h / 2, w, h, h / 2);
    nvgFillColor(vg, nvgRGBAf(bg_.r, bg_.g, bg_.b, bg_.a * alpha_));
    nvgFill(vg);
    nvgFillColor(vg, nvgRGBAf(fg_.r, fg_.g, fg_.b, alpha_));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgBeginPath(vg);
    nvgText(vg, x_ + w / 2, cy_ + 1, text_.c_str(), nullptr);
  }

}

ModsView::ModsView(GuiModManager* manager_, uint8_t* icon_, std::string title_)
    : _manager_(manager_), _icon_(icon_), _title_(std::move(title_)) {
  this->scanMods();
  this->rebuild();

  this->registerAction("", brls::Key::DUP, [this]{ this->move(-1); return true; }, true);
  this->registerAction("", brls::Key::DDOWN, [this]{ this->move(1); return true; }, true);
  this->registerAction("", brls::Key::L, [this]{ this->move(-6); return true; }, true);
  this->registerAction("", brls::Key::R, [this]{ this->move(6); return true; }, true);
  this->registerAction("Activar / apagar", brls::Key::A, [this]{ this->toggleSelected(); return true; });
  this->registerAction("Grupos y opciones", brls::Key::X, [this]{ this->openMore(); return true; });
  this->registerAction("Buscar mod", brls::Key::Y, [this]{
    brls::Swkbd::openForText([this](std::string text){
      _filter_ = text;
      this->rebuild();
    }, "Buscar mod", "", 40, _filter_);
    return true;
  });
  this->registerAction("Quitar búsqueda", brls::Key::MINUS, [this]{
    if( _filter_.empty() ) return true;
    _filter_.clear();
    this->rebuild();
    return true;
  }, true);
}

ModsView::~ModsView(){
  if( _image_ > 0 ) nvgDeleteImage(brls::Application::getNVGContext(), _image_);
}

/* Los archivos de cada mod: cuantos son y que mods comparten alguno. Dos mods
   chocan si tienen un archivo con la misma ruta: el que se activa despues pisa
   al otro, y apagar uno borra ese archivo del otro. */
void ModsView::scanMods(){
  auto& mm = _manager_->getGameBrowser().getModManager();
  auto& modList = mm.getModList();
  _rows_.clear();
  std::map<std::string, std::vector<int>> owners;
  for( size_t i = 0; i < modList.size(); i++ ){
    Row r;
    r.name = modList[i].modName;
    const std::string modPath = GenericToolbox::joinPath(mm.getGameFolderPath(), r.name);
    for( auto& f : GenericToolbox::lsFilesRecursive(modPath) ){
      const std::string name = GenericToolbox::getFileName(f);
      if( name.empty() or name[0] == '.' ) continue;  // .DS_Store, ._* y similares
      r.nFiles++;
      owners[f].push_back(int(i));
    }
    _rows_.emplace_back(std::move(r));
  }
  std::map<int, std::set<int>> pairs;
  for( auto& o : owners ){
    if( o.second.size() < 2 ) continue;
    for( int a : o.second ) for( int b : o.second ) if( a != b ) pairs[a].insert(b);
  }
  for( auto& p : pairs ) for( int b : p.second ) _rows_[p.first].clashes.push_back(_rows_[b].name);
}

void ModsView::rebuild(){
  _visible_.clear();
  const std::string f = lowerAscii(_filter_);
  for( size_t i = 0; i < _rows_.size(); i++ ){
    if( f.empty() or lowerAscii(_rows_[i].name).find(f) != std::string::npos ) _visible_.push_back(int(i));
  }
  _selected_ = 0;
  _first_ = 0;
  this->updateActionHint(brls::Key::MINUS, _filter_.empty() ? "" : "Quitar búsqueda");
}

void ModsView::move(int delta_){
  if( _visible_.empty() ) return;
  _selected_ = std::clamp(_selected_ + delta_, 0, int(_visible_.size()) - 1);
}

double ModsView::fractionOf(const std::string& modName_) const{
  auto& mm = _manager_->getGameBrowser().getModManager();
  const auto preset = mm.fetchCurrentPreset().name;
  for( auto& m : mm.getModList() ){ if( m.modName == modName_ ) return m.getStatusFraction(preset); }
  return 0;
}

std::string ModsView::statusOf(const std::string& modName_) const{
  auto& mm = _manager_->getGameBrowser().getModManager();
  const auto preset = mm.fetchCurrentPreset().name;
  for( auto& m : mm.getModList() ){ if( m.modName == modName_ ) return m.getStatus(preset); }
  return "";
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
  const NVGcolor text = this->a(ctx->theme->textColor);
  const NVGcolor faint = this->a(ctx->theme->descriptionColor);
  const NVGcolor accent = this->a(ctx->theme->highlightColor1);
  const NVGcolor veil = this->a(nvgRGBAf(ctx->theme->textColor.r, ctx->theme->textColor.g, ctx->theme->textColor.b, 0.07f));
  const NVGcolor knobOff = this->a(nvgRGBAf(ctx->theme->textColor.r, ctx->theme->textColor.g, ctx->theme->textColor.b, 0.35f));
  nvgFontFaceId(vg, ctx->fontStash->regular);

  const float pad = 40.f;
  const float left = x + pad, w = width - pad * 2;
  float top = y + 22.f;

  auto& mm = _manager_->getGameBrowser().getModManager();

  /* La primera vez que se abre un juego no hay estado guardado de sus mods:
     se revisan una vez (con su ventana de progreso), como hacia la app antes. */
  if( not _checked_ ){
    _checked_ = true;
    bool any = false;
    for( auto& r : _rows_ ){ if( not this->statusOf(r.name).empty() ){ any = true; break; } }
    if( not any and not _rows_.empty() ){ _manager_->startCheckAllModsThread(); }
  }

  // Resumen arriba: activos de cuantos, perfil y busqueda.
  {
    int active = 0;
    for( auto& r : _rows_ ){ if( this->fractionOf(r.name) == 1 ) active++; }
    const std::string summary = std::to_string(active) + " de " + std::to_string(_rows_.size()) + (active == 1 ? " activo" : " activos");
    pill(vg, left, top + 18.f, summary, active > 0 ? kGreenBg : nvgRGBAf(veil.r, veil.g, veil.b, veil.a), active > 0 ? kGreen : faint, this->alpha, 16.f);
    float cx = left + pillWidth(vg, summary, 16.f) + 12.f;

    const std::string profile = "Perfil: " + mm.fetchCurrentPreset().name;
    pill(vg, cx, top + 18.f, profile, nvgRGBAf(accent.r, accent.g, accent.b, 0.18f), accent, this->alpha, 16.f);
    cx += pillWidth(vg, profile, 16.f) + 18.f;

    nvgFontSize(vg, 16.f);
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, _filter_.empty() ? faint : text);
    nvgBeginPath(vg);
    const std::string search = _filter_.empty() ? "Buscar mod (Y)" : "Buscando: " + _filter_ + "  ·  " + std::to_string(_visible_.size()) + " de " + std::to_string(_rows_.size());
    nvgText(vg, left + w, top + 19.f, fit(vg, search, left + w - cx).c_str(), nullptr);
    top += 52.f;
  }

  if( _rows_.empty() ){
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, text);
    nvgFontSize(vg, 22.f);
    nvgBeginPath(vg);
    nvgText(vg, left, top + 10.f, "Este juego todavía no tiene mods", nullptr);
    nvgFontSize(vg, 17.f);
    nvgTextLineHeight(vg, 1.4f);
    nvgFillColor(vg, faint);
    nvgBeginPath(vg);
    const std::string help = "Cada mod va en su carpeta: " + mm.getGameFolderPath() + "/<nombre del mod>/<archivos como van en la consola>. SimpleModDownloader los deja ahí al descargarlos.";
    nvgTextBox(vg, left, top + 48.f, w, help.c_str(), nullptr);
    return;
  }
  if( _visible_.empty() ){
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, faint);
    nvgFontSize(vg, 18.f);
    nvgBeginPath(vg);
    nvgText(vg, left, top + 10.f, ("Ningún mod coincide con «" + _filter_ + "». Pulsa Y para buscar otra cosa o − para ver todos.").c_str(), nullptr);
    return;
  }

  const float rowH = 74.f;
  const int fits = std::max(1, int((y + height - 10.f - top) / rowH));
  if( _selected_ < _first_ ) _first_ = _selected_;
  if( _selected_ >= _first_ + fits ) _first_ = _selected_ - fits + 1;

  for( int k = 0; k < fits and _first_ + k < int(_visible_.size()); k++ ){
    const int vi = _first_ + k;
    const Row& r = _rows_[_visible_[vi]];
    const float ry = top + k * rowH;
    const double frac = this->fractionOf(r.name);
    const std::string status = this->statusOf(r.name);

    // Separador y seleccion.
    if( vi == _selected_ ){
      nvgBeginPath(vg);
      nvgRoundedRect(vg, left - 12.f, ry + 2, w + 24.f, rowH - 4, 12.f);
      nvgFillColor(vg, nvgRGBAf(accent.r, accent.g, accent.b, 0.13f * this->alpha));
      nvgFill(vg);
    }
    else{
      nvgBeginPath(vg);
      nvgRect(vg, left, ry + rowH - 1, w, 1);
      nvgFillColor(vg, veil);
      nvgFill(vg);
    }

    // Interruptor.
    const float sw = 52.f, sh = 28.f, sx = left, sy = ry + (rowH - sh) / 2;
    const bool on = frac > 0;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, sx, sy, sw, sh, sh / 2);
    nvgFillColor(vg, on ? (frac == 1 ? this->a(kGreen) : this->a(kAmber)) : veil);
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, on ? sx + sw - sh / 2 : sx + sh / 2, sy + sh / 2, sh / 2 - 4);
    nvgFillColor(vg, on ? this->a(nvgRGB(255, 255, 255)) : knobOff);
    nvgFill(vg);

    // Pildora de estado (el choque manda si el mod esta apagado y choca con uno activo).
    std::string label;
    NVGcolor bg = veil, fg = faint;
    std::vector<std::string> activeClashes;
    for( auto& c : r.clashes ){ if( this->fractionOf(c) > 0 ) activeClashes.push_back(c); }
    if( status.empty() ){ label = "Sin revisar"; }
    else if( status == "NO FILE" ){ label = "Sin archivos"; }
    else if( frac == 1 ){ label = "Activo"; bg = kGreenBg; fg = kGreen; }
    else if( frac > 0 ){
      const auto open = status.find('(');
      label = "A medias " + (open == std::string::npos ? std::string() : status.substr(open));
      bg = kAmberBg; fg = kAmber;
    }
    else{ label = "Apagado"; }
    if( not on and not activeClashes.empty() ){
      label = "Choca con " + activeClashes[0] + (activeClashes.size() > 1 ? " y " + std::to_string(activeClashes.size() - 1) + " más" : "");
      bg = kAmberBg; fg = kAmber;
    }
    nvgFontFaceId(vg, ctx->fontStash->regular);
    const float maxPill = w * 0.42f;
    nvgFontSize(vg, 15.f);
    label = fit(vg, label, maxPill - 22.f);
    const float pw = pillWidth(vg, label);
    pill(vg, left + w - pw, ry + rowH / 2, label, bg, fg, this->alpha);

    // Nombre y detalle.
    const float tx = sx + sw + 18.f, tw = left + w - pw - 18.f - tx;
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFontSize(vg, 20.f);
    nvgFillColor(vg, text);
    nvgBeginPath(vg);
    nvgText(vg, tx, ry + 14.f, fit(vg, r.name, tw).c_str(), nullptr);
    std::string sub = std::to_string(r.nFiles) + (r.nFiles == 1 ? " archivo" : " archivos");
    if( not r.clashes.empty() and (on or activeClashes.empty()) ){
      sub += "  ·  comparte archivos con " + r.clashes[0] + (r.clashes.size() > 1 ? " y " + std::to_string(r.clashes.size() - 1) + " más" : "");
    }
    nvgFontSize(vg, 15.f);
    nvgFillColor(vg, faint);
    nvgBeginPath(vg);
    nvgText(vg, tx, ry + 42.f, fit(vg, sub, tw).c_str(), nullptr);
  }

  if( int(_visible_.size()) > fits ){
    const float trackH = fits * rowH - 8.f;
    const float barH = std::max(30.f, trackH * fits / float(_visible_.size()));
    const float barY = top + 4.f + (trackH - barH) * _first_ / float(_visible_.size() - fits);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, left + w + 18.f, barY, 4.f, barH, 2.f);
    nvgFillColor(vg, veil);
    nvgFill(vg);
  }
}


FrameMods::FrameMods(GuiModManager* manager_, const std::string& title_) : brls::AppletFrame(false, false) {
  this->setTitle( title_ );
  this->setFooterText( "SimpleModManager" );
  const std::string tid = GenericToolbox::Switch::Utils::lookForTidInSubFolders( manager_->getGameBrowser().getModManager().getGameFolderPath(), 5 );
  _icon_ = GenericToolbox::Switch::Utils::getIconFromTitleId( tid );
  if( _icon_ != nullptr ){ this->setIcon(_icon_, 0x20000); }
  else{ this->setIcon("romfs:/images/icon_corner.png"); }
  this->setContentView( new ModsView(manager_, _icon_, title_) );
}

FrameMods::~FrameMods(){
  delete[] _icon_;
}
