#include "core/spell_stacking.h"

static bool isBardSong(const SpellData& sp) {
    // Bard = class 8 (index 7). Si classes[7] != 255 -> bard song.
    return sp.classes[7] != 255;
}

bool spellsStack(const SpellData& a, const SpellData& b) {
    bool aBard = isBardSong(a);
    bool bBard = isBardSong(b);
    // Bard + non-bard -> toujours compatible (goto STACK_OK dans buffstacking.cpp)
    if (aBard != bBard) return true;
    // Deux sorts du même type : comparer SPA slot par slot
    for (int i = 0; i < 12; ++i) {
        if (a.spa[i] == 0 && b.spa[i] == 0) continue;
        if (a.spa[i] == b.spa[i]) return false; // même SPA = conflit
    }
    return true;
}
