#pragma once
#include "core/types.h"

// Retourne true si les deux sorts peuvent coexister dans le buff bar.
// Implémente buffstacking.cpp EQMacEmu :
// - Bard songs (classes[7] != 255) stackent toujours avec les non-bard
// - Entre deux non-bard (ou deux bard) : comparaison SPA slot par slot
[[nodiscard]] bool spellsStack(const SpellData& a, const SpellData& b);
