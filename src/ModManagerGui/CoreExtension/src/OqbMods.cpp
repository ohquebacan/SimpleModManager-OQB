// SPDX-License-Identifier: GPL-3.0-or-later
// OH! QUÉ BACÁN: preparar los mods de un juego. Ver OqbMods.h.
#include "OqbMods.h"
#include "OqbPchtxt.h"

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace oqb::mods {
namespace {

const std::string ConfigBase = "/config/SimpleModManager";

std::string Min(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool EsTid(const std::string& s) {
    if (s.size() != 16) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isxdigit(c); });
}

bool TerminaEn(const std::string& s, const std::string& fin) {
    return s.size() >= fin.size() && Min(s.substr(s.size() - fin.size())) == fin;
}

bool EsArchivo(const std::string& nombre) {
    return TerminaEn(nombre, ".zip") || TerminaEn(nombre, ".7z") || TerminaEn(nombre, ".rar");
}

std::vector<std::string> Partes(const std::string& ruta) {
    std::vector<std::string> v;
    std::string p;
    std::istringstream in(ruta);
    while (std::getline(in, p, '/')) if (!p.empty()) v.push_back(p);
    return v;
}

std::string Unir(const std::vector<std::string>& v, size_t desde) {
    std::string s;
    for (size_t i = desde; i < v.size(); ++i) { if (!s.empty()) s += '/'; s += v[i]; }
    return s;
}

std::string CarpetaSegura(std::string nombre) {
    for (auto& c : nombre)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != ' ' && c != '-' && c != '_') c = '_';
    while (!nombre.empty() && nombre.back() == ' ') nombre.pop_back();
    return nombre.empty() ? std::string("mod") : nombre;
}

// Archivos del mod, con la ruta relativa a la carpeta del mod.
std::vector<std::string> Archivos(const fs::path& mod) {
    std::vector<std::string> out;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(mod, ec), fin; !ec && it != fin; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        out.push_back(fs::relative(it->path(), mod, ec).generic_string());
    }
    return out;
}

// Dónde debería ir un archivo del mod (relativo a la carpeta del mod), o vacío
// si no tiene forma reconocible. `tid` hace falta para romfs/ y exefs/ sueltos.
std::string Destino(const std::string& rel, const std::string& tid) {
    const auto p = Partes(rel);
    for (size_t i = 0; i + 1 < p.size(); ++i) {
        if (Min(p[i]) == "contents" && EsTid(p[i + 1])) {
            std::string resto = Unir(p, i + 2);
            const auto r = Partes(resto);
            if (!r.empty() && (Min(r[0]) == "romfs" || Min(r[0]) == "exefs")) resto = Min(r[0]) + (r.size() > 1 ? "/" + Unir(r, 1) : "");
            return "contents/" + p[i + 1] + "/" + resto;
        }
    }
    for (size_t i = 0; i + 1 < p.size(); ++i)
        if (Min(p[i]) == "exefs_patches") return "exefs_patches/" + Unir(p, i + 1);
    if (!tid.empty())
        for (size_t i = 0; i + 1 < p.size(); ++i)
            if (Min(p[i]) == "romfs" || Min(p[i]) == "exefs") return "contents/" + tid + "/" + Min(p[i]) + "/" + Unir(p, i + 1);
    return {};
}

bool Oculto(const std::string& rel) {
    for (const auto& parte : Partes(rel)) if (!parte.empty() && parte[0] == '.') return true;
    return false;
}

// Si hay que tocar algo en este mod.
bool ModPorPreparar(const fs::path& mod, const std::string& tid) {
    for (const auto& rel : Archivos(mod)) {
        if (Oculto(rel)) continue;
        if (TerminaEn(rel, ".pchtxt")) return true;
        const std::string d = Destino(rel, tid);
        if (!d.empty() && Min(d) != Min(rel)) return true;
    }
    return false;
}

// Busca un ID de juego en las carpetas contents/<ID> de los mods.
std::string TidDeLosMods(const fs::path& juego) {
    std::error_code ec;
    for (fs::recursive_directory_iterator it(juego, ec), fin; !ec && it != fin; it.increment(ec)) {
        if (it.depth() > 4) { it.disable_recursion_pending(); continue; }
        if (!it->is_directory(ec)) continue;
        const std::string nombre = it->path().filename().string();
        if (EsTid(nombre) && Min(it->path().parent_path().filename().string()) == "contents") return nombre;
    }
    return {};
}

void BorrarVacias(const fs::path& raiz) {
    std::error_code ec;
    std::vector<fs::path> dirs;
    for (fs::recursive_directory_iterator it(raiz, ec), fin; !ec && it != fin; it.increment(ec))
        if (it->is_directory(ec)) dirs.push_back(it->path());
    std::sort(dirs.begin(), dirs.end(), [](const fs::path& a, const fs::path& b) { return a.string().size() > b.string().size(); });
    for (const auto& d : dirs) if (fs::is_empty(d, ec)) fs::remove(d, ec);
}

// Guarda un archivo fuera de la carpeta del mod (sin pisar uno anterior).
bool Guardar(const fs::path& origen, const fs::path& destino) {
    std::error_code ec;
    fs::create_directories(destino.parent_path(), ec);
    fs::path final = destino;
    for (int n = 2; fs::exists(final, ec) && n < 100; ++n)
        final = destino.parent_path() / (destino.stem().string() + " (" + std::to_string(n) + ")" + destino.extension().string());
    fs::rename(origen, final, ec);
    if (!ec) return true;
    // rename entre carpetas siempre funciona en la misma SD; por si acaso, copia.
    fs::copy_file(origen, final, ec);
    if (ec) return false;
    fs::remove(origen, ec);
    return true;
}

// Descomprime `archivo` dentro de `destino`. Rechaza rutas absolutas o con "..".
bool Descomprimir(const fs::path& archivo, const fs::path& destino, Progreso& progreso, const std::atomic<bool>& cancelar, std::string& error) {
    struct archive* a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    if (archive_read_open_filename(a, archivo.string().c_str(), 64 * 1024) != ARCHIVE_OK) {
        error = archive_error_string(a) ? archive_error_string(a) : "no se pudo abrir";
        archive_read_free(a);
        return false;
    }
    bool ok = true;
    std::error_code ec;
    struct archive_entry* e;
    std::vector<char> buffer(256 * 1024);
    int r;
    while ((r = archive_read_next_header(a, &e)) == ARCHIVE_OK) {
        if (cancelar) { ok = false; error = "cancelado"; break; }
        const std::string nombre = archive_entry_pathname(e) ? archive_entry_pathname(e) : "";
        const auto partes = Partes(nombre);
        if (partes.empty() || nombre[0] == '/' || std::any_of(partes.begin(), partes.end(), [](const std::string& s) { return s == ".." || s.find(':') != std::string::npos; }))
            continue;
        const fs::path salida = destino / Unir(partes, 0);
        if (archive_entry_filetype(e) == AE_IFDIR) { fs::create_directories(salida, ec); continue; }
        if (archive_entry_filetype(e) != AE_IFREG) continue;
        progreso.actual = partes.back();
        fs::create_directories(salida.parent_path(), ec);
        std::ofstream f(salida, std::ios::binary | std::ios::trunc);
        if (!f) { ok = false; error = "no se pudo escribir " + salida.string(); break; }
        la_ssize_t n;
        while ((n = archive_read_data(a, buffer.data(), buffer.size())) > 0) f.write(buffer.data(), n);
        if (n < 0) { ok = false; error = archive_error_string(a) ? archive_error_string(a) : "archivo dañado"; break; }
    }
    if (r != ARCHIVE_EOF && r != ARCHIVE_OK && ok) { ok = false; error = archive_error_string(a) ? archive_error_string(a) : "archivo dañado"; }
    archive_read_close(a);
    archive_read_free(a);
    return ok;
}

// Convierte los .pchtxt de un mod y guarda los originales afuera.
size_t ConvertirPchtxt(const fs::path& mod, const std::string& juego, std::string& log) {
    size_t hechos = 0;
    std::error_code ec;
    for (const auto& rel : Archivos(mod)) {
        if (Oculto(rel) || !TerminaEn(rel, ".pchtxt")) continue;
        const fs::path origen = mod / rel;
        std::ifstream f(origen, std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        f.close();
        std::string carpeta;
        const auto p = Partes(rel);
        for (size_t i = 0; i + 2 < p.size(); ++i) if (Min(p[i]) == "exefs_patches") carpeta = p[i + 1];
        if (carpeta.empty()) carpeta = mod.filename().string();
        carpeta = CarpetaSegura(carpeta);
        std::vector<oqb::pchtxt::Ips> parches;
        std::string error;
        if (oqb::pchtxt::Convertir(ss.str(), parches, error)) {
            for (const auto& ips : parches) {
                const fs::path destino = mod / "exefs_patches" / carpeta / oqb::pchtxt::NombreIps(ips);
                fs::create_directories(destino.parent_path(), ec);
                std::ofstream o(destino, std::ios::binary | std::ios::trunc);
                o.write(reinterpret_cast<const char*>(ips.data.data()), static_cast<std::streamsize>(ips.data.size()));
                ++hechos;
            }
            log += rel + " -> exefs_patches/" + carpeta + "; ";
        } else {
            log += rel + ": " + error + "; ";
        }
        // El original se guarda afuera: si no, se copiaría a /atmosphere al activar.
        Guardar(origen, fs::path(ConfigBase) / "oqb_pchtxt" / juego / mod.filename() / rel);
    }
    return hechos;
}

// Mueve a su lugar los archivos con forma de mod que están en otro nivel.
size_t Reubicar(const fs::path& mod, const std::string& tid, std::string& log) {
    size_t movidos = 0;
    std::error_code ec;
    for (const auto& rel : Archivos(mod)) {
        if (Oculto(rel)) continue;
        const std::string d = Destino(rel, tid);
        if (d.empty() || Min(d) == Min(rel)) continue;
        const fs::path nuevo = mod / d;
        if (fs::exists(nuevo, ec)) { log += rel + ": ya existe " + d + "; "; continue; }
        fs::create_directories(nuevo.parent_path(), ec);
        fs::rename(mod / rel, nuevo, ec);
        if (ec) { log += rel + ": " + ec.message() + "; "; continue; }
        ++movidos;
    }
    return movidos;
}

// En un mod que hubo que reorganizar, lo que no es del mod (léeme, imágenes) se
// guarda afuera: si no, se copiaría suelto dentro de /atmosphere al activarlo.
void ApartarSobrantes(const fs::path& mod, const std::string& juego) {
    for (const auto& rel : Archivos(mod)) {
        if (Oculto(rel)) continue;
        const auto p = Partes(rel);
        const std::string primero = p.empty() ? "" : Min(p[0]);
        if (primero == "contents" || primero == "exefs_patches") continue;
        Guardar(mod / rel, fs::path(ConfigBase) / "oqb_extra" / juego / mod.filename() / rel);
    }
    BorrarVacias(mod);
}

} // namespace

bool NecesitaPreparar(const std::string& carpetaJuego, const std::string& tid, const std::string& baseInstalacion) {
    std::error_code ec;
    const fs::path juego(carpetaJuego);
    const bool atmosphere = Min(baseInstalacion) == "/atmosphere";
    std::string id = tid;
    for (const auto& e : fs::directory_iterator(juego, ec)) {
        const std::string nombre = e.path().filename().string();
        if (nombre.empty() || nombre[0] == '.') continue;
        if (e.is_regular_file(ec) && EsArchivo(nombre)) return true;
    }
    if (!atmosphere) return false;
    if (id.empty()) id = TidDeLosMods(juego);
    for (const auto& e : fs::directory_iterator(juego, ec)) {
        const std::string nombre = e.path().filename().string();
        if (nombre.empty() || nombre[0] == '.' || !e.is_directory(ec)) continue;
        if (ModPorPreparar(e.path(), id)) return true;
    }
    return false;
}

std::string Preparar(const std::string& carpetaJuego, std::string tid, const std::string& baseInstalacion,
                     Progreso& progreso, const std::atomic<bool>& cancelar) {
    std::error_code ec;
    const fs::path juego(carpetaJuego);
    const std::string nombreJuego = juego.filename().string();
    const bool atmosphere = Min(baseInstalacion) == "/atmosphere";
    std::string log;

    // 1. Archivos comprimidos -> una carpeta por mod.
    std::vector<fs::path> comprimidos;
    for (const auto& e : fs::directory_iterator(juego, ec)) {
        const std::string nombre = e.path().filename().string();
        if (!nombre.empty() && nombre[0] != '.' && e.is_regular_file(ec) && EsArchivo(nombre)) comprimidos.push_back(e.path());
    }
    std::vector<fs::path> mods;
    for (const auto& e : fs::directory_iterator(juego, ec))
        if (e.is_directory(ec) && e.path().filename().string()[0] != '.') mods.push_back(e.path());
    const double total = double(comprimidos.size() + mods.size() + 1);
    double hecho = 0;
    for (const auto& c : comprimidos) {
        if (cancelar) return log + "cancelado";
        progreso.fraccion = hecho++ / total;
        fs::path destino = juego / c.stem();
        for (int n = 2; fs::exists(destino, ec) && n < 100; ++n) destino = juego / (c.stem().string() + " (" + std::to_string(n) + ")");
        std::string error;
        if (Descomprimir(c, destino, progreso, cancelar, error)) {
            Guardar(c, fs::path(ConfigBase) / "oqb_archivos" / nombreJuego / c.filename());
            mods.push_back(destino);
            log += c.filename().string() + " descomprimido; ";
        } else {
            fs::remove_all(destino, ec);
            log += c.filename().string() + ": " + error + "; ";
            // Se aparta para no intentarlo en cada apertura; el archivo no se pierde.
            Guardar(c, fs::path(ConfigBase) / "oqb_archivos" / nombreJuego / ("NO-SE-PUDO-" + c.filename().string()));
        }
    }
    if (!atmosphere) { progreso.fraccion = 1; return log; }

    // 2. Cada mod: .pchtxt a .ips y archivos a su lugar.
    if (tid.empty()) tid = TidDeLosMods(juego);
    for (const auto& m : mods) {
        if (cancelar) return log + "cancelado";
        progreso.fraccion = hecho++ / total;
        progreso.actual = m.filename().string();
        const size_t ips = ConvertirPchtxt(m, nombreJuego, log);
        const size_t movidos = Reubicar(m, tid, log);
        if (movidos) ApartarSobrantes(m, nombreJuego);
        if (ips || movidos)
            log += m.filename().string() + ": " + std::to_string(ips) + " .ips, " + std::to_string(movidos) + " movidos; ";
    }
    progreso.fraccion = 1;
    return log;
}

} // namespace oqb::mods
