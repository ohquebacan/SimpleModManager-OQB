//
// OQB: piezas comunes del rediseno. Un marco sin la cabecera ni la barra de
// atajos de borealis (cada pantalla dibuja las suyas), y la barra de atajos
// del mockup: cada tecla en un recuadro redondeado y su texto al lado.
//

#ifndef SIMPLEMODMANAGER_OQBUI_H
#define SIMPLEMODMANAGER_OQBUI_H

#include <borealis.hpp>

#include <string>
#include <utility>
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

  // Barra de atajos abajo; devuelve su alto.
  inline float hintBar(NVGcontext* vg, float x_, float bottom_, float width_, const std::vector<std::pair<std::string, std::string>>& hints_, NVGcolor text_, float alpha_){
    const float h = 56.f, key = 30.f;
    const float y = bottom_ - h;
    nvgBeginPath(vg);
    nvgRect(vg, x_ + 30.f, y, width_ - 60.f, 1.f);
    nvgFillColor(vg, nvgRGBAf(text_.r, text_.g, text_.b, 0.15f * alpha_));
    nvgFill(vg);
    float cx = x_ + width_ - 40.f;
    // De derecha a izquierda, como borealis, para que A quede a la derecha.
    for( auto it = hints_.rbegin(); it != hints_.rend(); ++it ){
      if( it->second.empty() ) continue;
      nvgFontSize(vg, 17.f);
      const float tw = nvgTextBounds(vg, 0, 0, it->second.c_str(), nullptr, nullptr);
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgFillColor(vg, nvgRGBAf(text_.r, text_.g, text_.b, alpha_));
      nvgBeginPath(vg);
      nvgText(vg, cx - tw, y + h / 2 + 1, it->second.c_str(), nullptr);
      cx -= tw + 10.f;
      nvgFontSize(vg, 15.f);
      const float kw = std::max(key, nvgTextBounds(vg, 0, 0, it->first.c_str(), nullptr, nullptr) + 16.f);
      nvgBeginPath(vg);
      nvgRoundedRect(vg, cx - kw, y + (h - key) / 2, kw, key, 8.f);
      nvgFillColor(vg, nvgRGBAf(text_.r, text_.g, text_.b, 0.18f * alpha_));
      nvgFill(vg);
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(vg, nvgRGBAf(text_.r, text_.g, text_.b, alpha_));
      nvgBeginPath(vg);
      nvgText(vg, cx - kw / 2, y + h / 2 + 1, it->first.c_str(), nullptr);
      cx -= kw + 26.f;
    }
    return h;
  }

  // Chip seleccionable; devuelve su ancho.
  inline float chip(NVGcontext* vg, float x_, float y_, float h_, const std::string& text_, bool on_, bool focus_, NVGcolor accent_, NVGcolor text_c, NVGcolor faint_, float alpha_){
    nvgFontSize(vg, 16.f);
    const float w = nvgTextBounds(vg, 0, 0, text_.c_str(), nullptr, nullptr) + 32.f;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x_, y_, w, h_, 10.f);
    nvgFillColor(vg, on_ ? nvgRGBAf(accent_.r, accent_.g, accent_.b, 0.28f * alpha_) : nvgRGBAf(text_c.r, text_c.g, text_c.b, 0.08f * alpha_));
    nvgFill(vg);
    if( focus_ ){
      nvgBeginPath(vg);
      nvgRoundedRect(vg, x_ - 3, y_ - 3, w + 6, h_ + 6, 13.f);
      nvgStrokeColor(vg, accent_);
      nvgStrokeWidth(vg, 3.f);
      nvgStroke(vg);
    }
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, on_ ? accent_ : faint_);
    nvgBeginPath(vg);
    nvgText(vg, x_ + w / 2, y_ + h_ / 2 + 1, text_.c_str(), nullptr);
    return w;
  }

}

#endif //SIMPLEMODMANAGER_OQBUI_H
