// SPDX-License-Identifier: GPL-3.0-or-later
// OH! QUÉ BACÁN: deja los mods de un juego con la forma que espera la app
// (contents/<ID>/romfs|exefs y exefs_patches/), antes de mostrarlos.
//  - .zip/.7z/.rar en la carpeta del juego: se descomprimen en una carpeta por
//    mod y el archivo original se guarda en /config/SimpleModManager/oqb_archivos.
//  - .pchtxt: se convierten a .ips (exefs_patches/<mod>/<ID>.ips); el original se
//    guarda en /config/SimpleModManager/oqb_pchtxt, para que no se instale.
//  - Archivos con forma de mod pero en otro nivel (una carpeta de más,
//    atmosphere/contents/..., romfs/ o exefs/ sueltos): se mueven a su lugar.
// Lo que no se reconoce (un léeme, imágenes) no se toca.
#pragma once
#include <atomic>
#include <string>

namespace oqb::mods {

struct Progreso {
    double fraccion{0};
    std::string actual;
};

// Rápido (solo lista carpetas): si hay algo que preparar.
bool NecesitaPreparar(const std::string& carpetaJuego, const std::string& tid, const std::string& baseInstalacion);

// Hace el trabajo. `tid` puede venir vacío: entonces se busca en los otros mods.
// Devuelve un resumen corto para el registro.
std::string Preparar(const std::string& carpetaJuego, std::string tid, const std::string& baseInstalacion,
                     Progreso& progreso, const std::atomic<bool>& cancelar);

} // namespace oqb::mods
