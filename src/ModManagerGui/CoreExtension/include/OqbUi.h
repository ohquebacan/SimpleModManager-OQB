//
// OQB: piezas comunes del rediseno. Un marco sin la cabecera ni la barra de
// atajos de borealis (cada pantalla dibuja las suyas), los colores Amatista,
// las letras de OQB y las piezas del estilo nuevo: pildoras arriba (pestanas,
// hora, Wi-Fi y bateria) y abajo (contador y botones).
//

#ifndef SIMPLEMODMANAGER_OQBUI_H
#define SIMPLEMODMANAGER_OQBUI_H

#include <borealis.hpp>
#include <switch.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

class FrameSinBarras : public brls::AppletFrame {
public:
  FrameSinBarras() : brls::AppletFrame(false, false) {}
  void setContentView(brls::View* view) override {
    _content_ = view;
    brls::AppletFrame::setContentView(view);
  }
  void layout(NVGcontext* vg, brls::Style* style, brls::FontStash* stash) override {
    if( _content_ ){
      _content_->setBoundaries(this->x, this->y, this->width, this->height);
      _content_->invalidate();
    }
  }
  void draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height, brls::Style* style, brls::FrameContext* ctx) override {
    if( _content_ ) _content_->frame(ctx);
  }
private:
  brls::View* _content_{nullptr};
};

namespace oqbui {

  /* Colores del tema. Las mismas gamas que OQB Updater y JKSV BACAN (fondo,
     panel, linea, acento); el resto se deriva de ellas. Cian neón por defecto. */
  namespace pal {
    inline NVGcolor fondo, panel, lateral, elegido, borde, bordeSuave, chip, tinta, acento;
    inline const NVGcolor texto     = nvgRGB(232, 237, 230);
    inline const NVGcolor cuerpo    = nvgRGB(201, 207, 198);
    inline const NVGcolor tenue     = nvgRGB(154, 163, 150);
    inline const NVGcolor verde     = nvgRGB(157, 234, 154);
    inline const NVGcolor oro       = nvgRGB(230, 185, 92);
  }

  struct Tema{ const char* nombre; uint32_t fondo, panel, linea, claro; };
  inline constexpr Tema kTemas[] = {
      {"Cian neón", 0x0E1518, 0x121C20, 0x304C58, 0x00D2FF},
      {"Grafito", 0x0E0F11, 0x131417, 0x373A41, 0xA9B8C8},
      {"Amatista", 0x0F0D12, 0x141118, 0x3A3243, 0xC0A8EE},
      {"Verde OQB", 0x0B0F0A, 0x121711, 0x2A3327, 0x9DEA9A},
      {"Ámbar", 0x100E08, 0x16130B, 0x3D3522, 0xFFE27A},
      {"Bosque", 0x0C0F0D, 0x101411, 0x323B35, 0x8FC9A0},
      {"Rubí", 0x110C0D, 0x171012, 0x423034, 0xEE9A9A},
      {"Neón", 0x0A0F08, 0x11170E, 0x2C3624, 0xCEF781},
      // Mas vivos, con los acentos de Bacán Shop sobre gris casi negro.
      {"Esmeralda", 0x0E1515, 0x121C1D, 0x304E4D, 0x00E0A5},
      {"Azul Switch", 0x0E1317, 0x12191F, 0x304655, 0x009BE6},
      {"Violeta", 0x131118, 0x1A1720, 0x473E58, 0xBE5FFF},
      {"Naranja", 0x151411, 0x1C1A17, 0x4F473D, 0xFFA51E},
      {"Rojo Switch", 0x151113, 0x1C1619, 0x4F3C42, 0xFF4B4B},
  };
  inline constexpr int kNumTemas = int(sizeof(kTemas) / sizeof(kTemas[0]));
  inline constexpr const char* kTemaPath = "/config/SimpleModManager/tema.txt";

  inline NVGcolor hex(uint32_t c_){ return nvgRGB((c_ >> 16) & 0xFF, (c_ >> 8) & 0xFF, c_ & 0xFF); }
  inline NVGcolor mezcla(NVGcolor a_, NVGcolor b_, float p_){
    return nvgRGBf(a_.r + (b_.r - a_.r) * p_, a_.g + (b_.g - a_.g) * p_, a_.b + (b_.b - a_.b) * p_);
  }

  inline int& temaActual(){ static int t = 0; return t; }

  inline void usarPaleta(int i_){
    if( i_ < 0 or i_ >= kNumTemas ) i_ = 0;
    temaActual() = i_;
    const Tema& t = kTemas[i_];
    pal::fondo = hex(t.fondo);
    pal::lateral = hex(t.panel);
    pal::borde = hex(t.linea);
    pal::acento = hex(t.claro);
    pal::panel = mezcla(pal::lateral, pal::borde, 0.12f);
    pal::bordeSuave = mezcla(pal::lateral, pal::borde, 0.6f);
    pal::chip = mezcla(pal::lateral, pal::borde, 0.55f);
    pal::elegido = mezcla(pal::fondo, pal::acento, 0.10f);
    pal::tinta = pal::lateral;
  }

  /* El tema se guarda por nombre. Un archivo viejo trae el numero del orden
     anterior (Amatista, Verde OQB, Ambar, Bosque, Grafito, Rubi, Neon). */
  inline int leerTema() {
      static const char* viejos[] = {"Amatista", "Verde OQB", "Ámbar", "Bosque", "Grafito", "Rubí", "Neón"};
      char linea[64] = {0};
      if (FILE* f = std::fopen(kTemaPath, "r")) {
          if (!std::fgets(linea, sizeof(linea), f)) linea[0] = 0;
          std::fclose(f);
      }
      std::string nombre(linea);
      while (!nombre.empty() && (nombre.back() == '\n' || nombre.back() == '\r' || nombre.back() == ' ')) nombre.pop_back();
      if (nombre.size() == 1 && nombre[0] >= '0' && nombre[0] <= '6') nombre = viejos[nombre[0] - '0'];
      for (int i = 0; i < kNumTemas; ++i)
          if (nombre == kTemas[i].nombre) return i;
      return 0;
  }

  inline void guardarTema(int i) {
      if (i < 0 || i >= kNumTemas) return;
      if (FILE* f = std::fopen(kTemaPath, "w")) {
          std::fprintf(f, "%s\n", kTemas[i].nombre);
          std::fclose(f);
      }
  }

  inline NVGcolor conAlfa(NVGcolor c_, float a_){ return nvgRGBAf(c_.r, c_.g, c_.b, c_.a * a_); }

  // Letras de OQB. Se cargan una vez, despues de iniciar borealis.
  struct Fuentes{ int texto{-1}; int negrita{-1}; int mono{-1}; int marca{-1}; int simbolos{-1}; };
  inline Fuentes& fuentes(){ static Fuentes f; return f; }

  /* Instrument Sans pasa a ser la letra normal de toda la app; los iconos de
     botones, el cirilico y el CJK siguen saliendo de las letras de la consola. */
  inline void cargarFuentes(){
    NVGcontext* vg = brls::Application::getNVGContext();
    brls::FontStash* st = brls::Application::getFontStash();
    const int sistema = st->regular;
    const int respaldos[] = {sistema, st->standard, st->sharedSymbols, st->material, st->korean, st->schinese, st->extSchinese, st->tchinese};
    auto cargar = [&](const char* nombre_, const char* ruta_){
      const int id = brls::Application::loadFont(nombre_, ruta_);
      if( id < 0 ) return -1;
      std::vector<int> puestos;
      for( int r : respaldos ){
        if( r <= 0 and r != sistema ) continue;
        if( r < 0 or r == id or std::find(puestos.begin(), puestos.end(), r) != puestos.end() ) continue;
        nvgAddFallbackFontId(vg, id, r);
        puestos.push_back(r);
      }
      return id;
    };
    auto& f = fuentes();
    f.texto = cargar("oqb-texto", "romfs:/fonts/instrument_sans_medium.ttf");
    f.negrita = cargar("oqb-negrita", "romfs:/fonts/instrument_sans_semibold.ttf");
    f.mono = cargar("oqb-mono", "romfs:/fonts/jetbrains_mono_medium.ttf");
    f.marca = cargar("oqb-marca", "romfs:/fonts/upheaval.ttf");
    if( f.negrita < 0 ) f.negrita = f.texto;
    if( f.texto >= 0 ) st->regular = f.texto;
    else f.texto = f.negrita = sistema;
    if( f.mono < 0 ) f.mono = f.texto;
    if( f.marca < 0 ) f.marca = f.negrita;
    f.simbolos = st->sharedSymbols > 0 ? st->sharedSymbols : -1;
  }

  /* Los botones con los iconos de la propia consola (letra de simbolos del
     sistema): se ven nitidos a cualquier tamano y bien centrados, a diferencia
     de una letra dibujada dentro de un circulo. */
  inline const char* glifo(const std::string& t_){
    if( t_ == "A" ) return "\uE0E0";
    if( t_ == "B" ) return "\uE0E1";
    if( t_ == "X" ) return "\uE0E2";
    if( t_ == "Y" ) return "\uE0E3";
    if( t_ == "L" ) return "\uE0E4";
    if( t_ == "R" ) return "\uE0E5";
    if( t_ == "ZL" ) return "\uE0E6";
    if( t_ == "ZR" ) return "\uE0E7";
    if( t_ == "+" ) return "\uE0EF";
    if( t_ == "−" or t_ == "-" ) return "\uE0F0";
    return nullptr;
  }

  // Los colores de borealis (dialogos, listas, progreso) pasan a Amatista.
  inline void aplicarTema(){
    auto* w = brls::Application::getThemeVariantsWrapper();
    if( w == nullptr ) return;
    for( auto v : {brls::ThemeVariant::LIGHT, brls::ThemeVariant::DARK} ){
      brls::Theme* t = w->getTheme(v);
      if( t == nullptr ) continue;
      t->backgroundColor[0] = pal::fondo.r; t->backgroundColor[1] = pal::fondo.g; t->backgroundColor[2] = pal::fondo.b;
      t->backgroundColorRGB = pal::fondo;
      t->textColor = pal::texto;
      t->descriptionColor = pal::tenue;
      t->notificationTextColor = pal::texto;
      t->backdropColor = nvgRGBA(0, 0, 0, 178);
      t->separatorColor = pal::bordeSuave;
      t->sidebarColor = pal::panel;
      t->activeTabColor = pal::acento;
      t->sidebarSeparatorColor = pal::bordeSuave;
      t->highlightBackgroundColor = pal::elegido;
      t->highlightColor1 = pal::acento;
      t->highlightColor2 = nvgRGB(222, 210, 248);
      t->listItemSeparatorColor = pal::bordeSuave;
      t->listItemValueColor = pal::verde;
      t->listItemFaintValueColor = pal::tenue;
      t->tableEvenBackgroundColor = pal::panel;
      t->tableBodyTextColor = pal::cuerpo;
      t->dropdownBackgroundColor = nvgRGBA(0, 0, 0, 178);
      t->nextStageBulletColor = pal::acento;
      t->spinnerBarColor = conAlfa(pal::tenue, 0.4f);
      t->scrollBarColor = pal::borde;
      t->headerRectangleColor = pal::acento;
      t->buttonPrimaryEnabledBackgroundColor = pal::acento;
      t->buttonPrimaryDisabledBackgroundColor = pal::chip;
      t->buttonPrimaryEnabledTextColor = pal::tinta;
      t->buttonPrimaryDisabledTextColor = pal::tenue;
      t->buttonBorderedBorderColor = pal::borde;
      t->buttonBorderedTextColor = pal::texto;
      t->buttonRegularBackgroundColor = pal::chip;
      t->buttonRegularTextColor = pal::texto;
      t->buttonRegularBorderColor = pal::borde;
      t->dialogColor = pal::panel;
      t->dialogBackdrop = nvgRGBA(0, 0, 0, 150);
      t->dialogButtonColor = pal::acento;
      t->dialogButtonSeparatorColor = pal::bordeSuave;
    }
  }

  // Texto que entra en un ancho; si no, se corta con "…" (sin partir un caracter UTF-8).
  inline std::string ajustar(NVGcontext* vg, const std::string& text_, float width_){
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

  inline float anchoTexto(NVGcontext* vg, int fuente_, float size_, const std::string& t_){
    nvgFontFaceId(vg, fuente_);
    nvgFontSize(vg, size_);
    return nvgTextBounds(vg, 0, 0, t_.c_str(), nullptr, nullptr);
  }

  inline void texto(NVGcontext* vg, int fuente_, float size_, NVGcolor c_, float x_, float y_, const std::string& t_, int align_ = NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE){
    nvgFontFaceId(vg, fuente_);
    nvgFontSize(vg, size_);
    nvgTextAlign(vg, align_);
    nvgFillColor(vg, c_);
    nvgBeginPath(vg);
    nvgText(vg, x_, y_, t_.c_str(), nullptr);
  }

  // Rectangulo redondeado con relleno y, si se pide, borde por dentro.
  inline void caja(NVGcontext* vg, float x_, float y_, float w_, float h_, float r_, NVGcolor fill_, float a_, NVGcolor* stroke_ = nullptr, float sw_ = 1.f){
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x_, y_, w_, h_, r_);
    nvgFillColor(vg, conAlfa(fill_, a_));
    nvgFill(vg);
    if( stroke_ ){
      nvgBeginPath(vg);
      nvgRoundedRect(vg, x_ + sw_ / 2, y_ + sw_ / 2, w_ - sw_, h_ - sw_, std::max(0.f, r_ - sw_ / 2));
      nvgStrokeColor(vg, conAlfa(*stroke_, a_));
      nvgStrokeWidth(vg, sw_);
      nvgStroke(vg);
    }
  }

  // Etiqueta chica; devuelve su ancho.
  inline float etiqueta(NVGcontext* vg, float x_, float cy_, const std::string& t_, NVGcolor bg_, NVGcolor fg_, float a_, bool mono_ = true, float size_ = 13.f){
    const int f = mono_ ? fuentes().mono : fuentes().texto;
    const float w = anchoTexto(vg, f, size_, t_) + 18.f, h = size_ + 10.f;
    caja(vg, x_, cy_ - h / 2, w, h, 9.f, bg_, a_);
    texto(vg, f, size_, conAlfa(fg_, a_), x_ + 9.f, cy_ + 1.f, t_);
    return w;
  }

  // Pildora de hora, Wi-Fi y bateria, pegada a la derecha. Devuelve su ancho.
  inline float estado(NVGcontext* vg, float right_, float y_, float a_){
    static time_t visto = 0;
    static std::string hora;
    static u32 bateria = 0;
    static bool hayBateria = false, conectado = false;
    const time_t ahora = time(nullptr);
    if( ahora != visto ){
      visto = ahora;
      char b[8];
      const struct tm* t = localtime(&ahora);
      strftime(b, sizeof(b), "%H:%M", t);
      hora = b;
      hayBateria = R_SUCCEEDED(psmGetBatteryChargePercentage(&bateria));
      NifmInternetConnectionType tipo{};
      u32 senal{0};
      NifmInternetConnectionStatus st{};
      conectado = R_SUCCEEDED(nifmGetInternetConnectionStatus(&tipo, &senal, &st)) and st == NifmInternetConnectionStatus_Connected;
    }
    const float h = 56.f;
    const std::string pct = hayBateria ? std::to_string(bateria) + "%" : "";
    const float wHora = anchoTexto(vg, fuentes().texto, 20.f, hora);
    const float wPct = pct.empty() ? 0.f : anchoTexto(vg, fuentes().texto, 20.f, pct);
    const float w = 22.f + wHora + 18.f + 22.f + (pct.empty() ? 0.f : 18.f + wPct + 8.f + 30.f) + 22.f;
    const float x = right_ - w, cy = y_ + h / 2;
    NVGcolor b = pal::borde;
    caja(vg, x, y_, w, h, h / 2, pal::panel, a_, &b);
    const NVGcolor tc = conAlfa(pal::texto, a_);
    float cx = x + 22.f;
    texto(vg, fuentes().texto, 20.f, tc, cx, cy + 1.f, hora);
    cx += wHora + 18.f;
    // Wi-Fi: tres arcos y un punto; apagado si no hay conexion.
    {
      const NVGcolor wc = conectado ? tc : conAlfa(pal::tenue, 0.6f * a_);
      const float ox = cx + 11.f, oy = cy + 7.f;
      nvgStrokeColor(vg, wc);
      nvgStrokeWidth(vg, 2.f);
      nvgLineCap(vg, NVG_ROUND);
      for( int i = 1; i <= 3; i++ ){
        nvgBeginPath(vg);
        nvgArc(vg, ox, oy, 4.f * i, -NVG_PI * 0.78f, -NVG_PI * 0.22f, NVG_CW);
        nvgStroke(vg);
      }
      nvgBeginPath(vg);
      nvgCircle(vg, ox, oy, 1.6f);
      nvgFillColor(vg, wc);
      nvgFill(vg);
      cx += 22.f;
    }
    if( not pct.empty() ){
      cx += 18.f;
      texto(vg, fuentes().texto, 20.f, tc, cx, cy + 1.f, pct);
      cx += wPct + 8.f;
      nvgBeginPath(vg);
      nvgRoundedRect(vg, cx + 0.8f, cy - 7.f, 25.f, 14.f, 3.f);
      nvgStrokeColor(vg, tc);
      nvgStrokeWidth(vg, 1.6f);
      nvgStroke(vg);
      const float lleno = 19.f * std::clamp<u32>(bateria, 0, 100) / 100.f;
      caja(vg, cx + 3.8f, cy - 4.f, std::max(2.f, lleno), 8.f, 1.5f, bateria <= 15 ? pal::oro : pal::verde, a_);
      caja(vg, cx + 27.f, cy - 3.f, 2.f, 6.f, 1.f, pal::texto, a_);
    }
    return w;
  }

  // Un boton de abajo: la tecla en un circulo y su texto. Devuelve su ancho.
  struct Boton{ std::string tecla; std::string texto; bool principal{false}; };

  inline float anchoBoton(NVGcontext* vg, const Boton& b_){
    return 18.f + 22.f + 8.f + anchoTexto(vg, b_.principal ? fuentes().negrita : fuentes().texto, 18.f, b_.texto) + 18.f;
  }

  inline float boton(NVGcontext* vg, float x_, float y_, const Boton& b_, float a_){
    const float h = 44.f, w = anchoBoton(vg, b_), cy = y_ + h / 2;
    NVGcolor borde = b_.principal ? pal::acento : pal::borde;
    caja(vg, x_, y_, w, h, h / 2, b_.principal ? pal::chip : pal::panel, a_, &borde);
    const NVGcolor kc = conAlfa(b_.principal ? pal::acento : pal::texto, a_);
    if( const char* g = glifo(b_.tecla); g and fuentes().simbolos >= 0 ){
      texto(vg, fuentes().simbolos, 26.f, kc, x_ + 29.f, cy, g, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
    else{
      nvgBeginPath(vg);
      nvgCircle(vg, x_ + 18.f + 11.f, cy, 11.f);
      nvgFillColor(vg, kc);
      nvgFill(vg);
      texto(vg, fuentes().negrita, 14.f, conAlfa(pal::tinta, a_), x_ + 29.f, cy + 1.f, b_.tecla, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
    texto(vg, b_.principal ? fuentes().negrita : fuentes().texto, 18.f, conAlfa(pal::texto, a_), x_ + 18.f + 22.f + 8.f, cy + 1.f, b_.texto);
    return w;
  }

  // Contador (si hay) a la izquierda y botones a la derecha, a 20 px del borde de abajo.
  inline void pie(NVGcontext* vg, float left_, float right_, float bottom_, const std::string& contador_, const std::vector<Boton>& botones_, float a_){
    const float h = 44.f, y = bottom_ - 20.f - h;
    if( not contador_.empty() ){
      const float w = anchoTexto(vg, fuentes().mono, 17.f, contador_) + 36.f;
      NVGcolor b = pal::borde;
      caja(vg, left_, y, w, h, h / 2, pal::fondo, a_, &b);
      texto(vg, fuentes().mono, 17.f, conAlfa(pal::texto, a_), left_ + 18.f, y + h / 2 + 1.f, contador_);
    }
    float total = 0.f;
    int n = 0;
    for( auto& b : botones_ ){ if( b.texto.empty() ) continue; total += anchoBoton(vg, b); n++; }
    if( n == 0 ) return;
    float x = right_ - total - 10.f * (n - 1);
    for( auto& b : botones_ ){
      if( b.texto.empty() ) continue;
      x += boton(vg, x, y, b, a_) + 10.f;
    }
  }

  // Iconos de las pestanas, dibujados (sin imagenes).
  // Pestana de juegos: una tarjeta de juego de Switch (esquina cortada y etiqueta).
  inline void iconoJuegos(NVGcontext* vg, float cx_, float cy_, NVGcolor c_){
    const float w = 15.f, h = 19.f, x = cx_ - w / 2, y = cy_ - h / 2, corte = 4.5f, r = 2.f;
    nvgStrokeColor(vg, c_);
    nvgStrokeWidth(vg, 1.9f);
    nvgLineJoin(vg, NVG_ROUND);
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + r, y);
    nvgLineTo(vg, x + w - corte, y);
    nvgLineTo(vg, x + w, y + corte);
    nvgLineTo(vg, x + w, y + h - r);
    nvgArcTo(vg, x + w, y + h, x + w - r, y + h, r);
    nvgLineTo(vg, x + r, y + h);
    nvgArcTo(vg, x, y + h, x, y + h - r, r);
    nvgLineTo(vg, x, y + r);
    nvgArcTo(vg, x, y, x + r, y, r);
    nvgClosePath(vg);
    nvgStroke(vg);
    // La etiqueta del juego, llena.
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 3.5f, y + 5.f, w - 7.f, 7.5f, 1.2f);
    nvgFillColor(vg, c_);
    nvgFill(vg);
  }
  inline void iconoAjustes(NVGcontext* vg, float cx_, float cy_, NVGcolor c_){
    nvgStrokeColor(vg, c_);
    nvgStrokeWidth(vg, 1.8f);
    nvgLineCap(vg, NVG_ROUND);
    const float l = cx_ - 9.f, r = cx_ + 9.f;
    nvgBeginPath(vg); nvgMoveTo(vg, l, cy_ - 5.f); nvgLineTo(vg, cx_ + 1.f, cy_ - 5.f); nvgMoveTo(vg, cx_ + 7.f, cy_ - 5.f); nvgLineTo(vg, r, cy_ - 5.f); nvgStroke(vg);
    nvgBeginPath(vg); nvgMoveTo(vg, l, cy_ + 5.f); nvgLineTo(vg, cx_ - 5.f, cy_ + 5.f); nvgMoveTo(vg, cx_ + 1.f, cy_ + 5.f); nvgLineTo(vg, r, cy_ + 5.f); nvgStroke(vg);
    nvgBeginPath(vg); nvgCircle(vg, cx_ + 4.f, cy_ - 5.f, 2.5f); nvgStroke(vg);
    nvgBeginPath(vg); nvgCircle(vg, cx_ - 2.f, cy_ + 5.f, 2.5f); nvgStroke(vg);
  }

  inline float tecla(NVGcontext* vg, float x_, float cy_, const std::string& t_, float a_){
    if( const char* g = glifo(t_); g and fuentes().simbolos >= 0 ){
      texto(vg, fuentes().simbolos, 24.f, conAlfa(pal::tenue, a_), x_ + 12.f, cy_, g, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      return 24.f;
    }
    const float w = anchoTexto(vg, fuentes().mono, 12.f, t_) + 12.f;
    NVGcolor b = pal::borde;
    caja(vg, x_, cy_ - 11.f, w, 22.f, 6.f, pal::panel, a_, &b);
    texto(vg, fuentes().mono, 12.f, conAlfa(pal::tenue, a_), x_ + w / 2, cy_ + 1.f, t_, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    return w;
  }

  /* Pestanas de arriba en una pildora: L, juegos y ajustes (la elegida en un
     circulo lleno) y R. Devuelve su ancho. */
  inline float pestanas(NVGcontext* vg, float x_, float y_, int elegida_, float a_){
    const float h = 56.f, cy = y_ + h / 2;
    const float wL = fuentes().simbolos >= 0 ? 24.f : anchoTexto(vg, fuentes().mono, 12.f, "L") + 12.f;
    const float w = 10.f + wL + 6.f + 44.f + 6.f + 44.f + 6.f + wL + 10.f;
    NVGcolor b = pal::borde;
    caja(vg, x_, y_, w, h, h / 2, pal::panel, a_, &b);
    float cx = x_ + 10.f;
    cx += tecla(vg, cx, cy, "L", a_) + 6.f;
    for( int i = 0; i < 2; i++ ){
      const bool on = i == elegida_;
      if( on ){
        nvgBeginPath(vg);
        nvgCircle(vg, cx + 22.f, cy, 22.f);
        nvgFillColor(vg, conAlfa(pal::acento, a_));
        nvgFill(vg);
      }
      const NVGcolor c = conAlfa(on ? pal::tinta : pal::texto, a_);
      if( i == 0 ) iconoJuegos(vg, cx + 22.f, cy, c);
      else iconoAjustes(vg, cx + 22.f, cy, c);
      cx += 44.f + 6.f;
    }
    tecla(vg, cx, cy, "R", a_);
    return w;
  }

  // Titulo en Upheaval (multiplos de 20 px).
  inline float titulo(NVGcontext* vg, float x_, float cy_, const std::string& t_, float a_){
    texto(vg, fuentes().marca, 40.f, conAlfa(pal::texto, a_), x_, cy_ + 2.f, t_);
    return anchoTexto(vg, fuentes().marca, 40.f, t_);
  }

  // Un color de relleno estable por nombre, para portadas sin icono.
  inline NVGcolor colorDe(const std::string& s_){
    static const NVGcolor colores[] = {nvgRGB(122,43,43), nvgRGB(69,52,92), nvgRGB(44,74,94), nvgRGB(92,42,34), nvgRGB(47,82,70),
                                       nvgRGB(45,79,110), nvgRGB(74,45,85), nvgRGB(62,58,99), nvgRGB(58,77,52), nvgRGB(90,58,36)};
    unsigned h = 2166136261u;
    for( unsigned char c : s_ ) h = (h ^ c) * 16777619u;
    return colores[h % 10];
  }

  // Siglas para una portada sin icono: la primera letra de hasta 4 palabras.
  inline std::string siglas(const std::string& s_){
    std::string out;
    bool inicio = true;
    for( unsigned char c : s_ ){
      if( std::isalnum(c) ){ if( inicio and out.size() < 4 ) out += char(std::toupper(c)); inicio = false; }
      else if( c == ' ' or c == '-' or c == ':' or c == '_' ) inicio = true;
    }
    return out.empty() ? "?" : out;
  }

}

#endif //SIMPLEMODMANAGER_OQBUI_H
