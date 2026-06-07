#pragma once
#include "core/types.h"
#include "ui/palette.h"
#include <QString>
#include <map>

// HTML tooltip for a spell effect (SPA-decoded effects).
// level       = 0   → use base/max values without level scaling.
// spellNames  = optional map of spell_id → name for SE_LimitSpell (139) display.
// accentColor = hex color for the spell header and effect values (e.g. kAccentFocus).
QString formatSpellTooltip(const SpellData& spell, int level = 0,
                            const std::map<int,QString>& spellNames = {},
                            const QString& accentColor = kTextBase);
