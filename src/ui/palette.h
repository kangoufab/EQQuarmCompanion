#pragma once

// Central color token system for EqQuarmCompanion UI.
// All inline const char* so they compose naturally with Qt stylesheet strings.
// Include this header (or ui_helpers.h which re-exports it) in UI files.

// ── Backgrounds (darkest to lightest) ────────────────────────────────────────
inline const char* kBgBase        = "#0b1120";  // inventory row base
inline const char* kBgScrollTrack = "#0a0f1a";  // horizontal scrollbar track
inline const char* kBgMain        = "#0f1624";  // main window
inline const char* kBgCard        = "#1a2236";  // cards, combo dropdowns
inline const char* kBgTileNoLimit = "#1e2a1e";  // uncapped stat tile (ATK, Haste)
inline const char* kBgTileAtCap   = "#1e3a5f";  // stat tile when value = cap
inline const char* kBgTileAtk     = "#2a1e1e";  // uncapped stat tile, red accent (ATK in combat panel)
inline const char* kBgActionEquip  = "#1e3a1e";  // "Équiper" button background (green action)
inline const char* kBgActionSource = "#1e2a3e";  // "Source" button background (blue action)
inline const char* kBgTile        = "#252540";  // capped stat tile (attr/resist)

// ── Surfaces ──────────────────────────────────────────────────────────────────
inline const char* kSurfaceSection = "#141f35";
inline const char* kSurfaceMid     = "#16161e";
inline const char* kSurfaceMidAlt  = "#1e1e2c";  // alternating row stripe, pairs with kSurfaceMid
inline const char* kBorderSep      = "#333333";  // thin horizontal separator lines
inline const char* kSurfaceDark    = "#1a1a2e";
inline const char* kSurfaceDialog  = "#141428";  // Sources dialog results scroll area

// ── Borders ───────────────────────────────────────────────────────────────────
inline const char* kBorderSub    = "#131d2e";  // subtle row separator
inline const char* kBorderCardSep = "#1e2a3a"; // item card section separator (HLine)
inline const char* kBorderMid    = "#2a3a5a";  // selection / mid border
inline const char* kBorderCard   = "#3a4a6a";  // card / combo border
inline const char* kBorderAccent = "#4a6a9a";  // hover / focus highlight
inline const char* kBorderDisabled = "#2a3045"; // disabled control border

// ── Text ──────────────────────────────────────────────────────────────────────
inline const char* kTextPrimary   = "#e0e0e0";  // headings, key values
inline const char* kTextBase      = "#c0c0c0";  // body text, item names
inline const char* kTextSecondary = "#8a8a8a";  // secondary labels, grid keys — ≥4.5:1 on kBgCard/kBgBase/kBgMain (computed)
inline const char* kTextMuted     = "#828282";  // cap dividers, very dim — ≥4.5:1 on kBgMain/kBgBase (computed)
inline const char* kTextDim       = "#666677";  // disabled state — ≥4.5:1 on kBgMain/kBgBase (WCAG AA)

// P1 contrast fixes — these replace colors that failed WCAG AA (< 4.5:1):
inline const char* kTextEmptySlot = "#717e9f";  // "— vide —" in inventory (4.65:1 on kBgBase, computed)
inline const char* kTextSlotLabel = "#6686a6";  // slot abbreviations Charm/Head (4.95:1 on kBgBase, computed)
inline const char* kTextDeltaNeg  = "#f07070";  // negative stat delta on tile bg (≥4.5:1)
inline const char* kTextTileKey   = "#909090";  // stat tile key labels — ≥4.5:1 on all tile bgs

// ── Semantic colors ───────────────────────────────────────────────────────────
// These carry strict meaning — never use decoratively.
inline const char* kGreen  = "#81c784";  // upgrade / positive / safe
inline const char* kOrange = "#ffb74d";  // caution / marginal / warning
inline const char* kRed    = "#ef5350";  // danger / downgrade / immune

// ── Category / accent colors ──────────────────────────────────────────────────
inline const char* kAccentBlue   = "#64b5f6";  // Defense category / info / hover
inline const char* kAccentPurple = "#ba68c8";  // Sorts / spell category
inline const char* kAccentMelee  = "#e57373";  // Melee category (lighter than kRed)
inline const char* kAccentAtCap  = "#4fc3f7";  // stat value displayed at cap
inline const char* kAccentGold   = "#ffc947";  // AA bonuses / formula highlight
inline const char* kAccentBlocked = "#cc6666"; // blocked spell (stacking conflict loser)
inline const char* kAccentSong    = "#ffd54f"; // Bard "Songs" section accent
inline const char* kAccentBard      = "#9cbe9c"; // Bard-only debuff legend/marker (muted green)
inline const char* kAccentBardLight = "#b8d8b8"; // Bard spell name in resist-debuff table (lighter, on row stripe)

// ── Spell effect type colors ──────────────────────────────────────────────────
// Used by item_card.cpp and spell_tooltip.cpp — keep in sync.
inline const char* kAccentWorn   = "#8888ff";  // Worn effect prefix
inline const char* kAccentFocus  = "#88cc88";  // Focus effect prefix
inline const char* kAccentProc   = "#ffaa44";  // Proc effect prefix
inline const char* kAccentBuff   = "#80b0e0";  // Active buff label / tooltip accent

// ── BIS / state variants ──────────────────────────────────────────────────
inline const char* kAccentGoldHover = "#ffd97d";  // BIS gold cell hover / focus

// ── Section tints — used by sectionTheme() in ui_helpers.h ───────────────
inline const char* kBgTintOrange      = "#2a241a";
inline const char* kBorderTintOrange  = "#5a4a3a";
inline const char* kBgTintGreen       = "#1a2a1e";
inline const char* kBorderTintGreen   = "#3a5a4a";
inline const char* kBgTintRed         = "#2a1a1a";
inline const char* kBorderTintRed     = "#5a3a3a";
inline const char* kBgTintPurple      = "#241a2a";
inline const char* kBorderTintPurple  = "#4a3a5a";
inline const char* kBorderTintDefault = "#3a3a5a";  // neutral-cool fallback border

// ── HTML tooltip colors ───────────────────────────────────────────────────
// Used in Qt HTML tooltip strings (can't use QSS tokens in HTML attributes).
inline const char* kHtmlLabel          = "#aaaaaa";  // stat label in effect rows
inline const char* kHtmlCondHeader     = "#445566";  // ─ CONDITIONS header
inline const char* kHtmlCondLabel      = "#6a8399";  // CONDITIONS label
inline const char* kHtmlCondValue      = "#8aabb8";  // CONDITIONS value
inline const char* kHtmlConflictHeader = "#4a2020";  // ─ CONFLIT header
inline const char* kHtmlConflictLabel  = "#7a4040";  // CONFLIT label
inline const char* kHtmlSortHeader     = "#2d4258";  // ─ SORT header
inline const char* kHtmlSortLabel      = "#4a6070";  // SORT label
inline const char* kHtmlSortValue      = "#6a8090";  // SORT value
