#pragma once

// Central color token system for EqQuarmCompanion UI.
// All inline const char* so they compose naturally with Qt stylesheet strings.
// Include this header (or ui_helpers.h which re-exports it) in UI files.

// ── Backgrounds (darkest to lightest) ────────────────────────────────────────
inline const char* kBgBase        = "#0b1120";  // inventory row base
inline const char* kBgMain        = "#0f1624";  // main window
inline const char* kBgCard        = "#1a2236";  // cards, combo dropdowns
inline const char* kBgTileNoLimit = "#1e2a1e";  // uncapped stat tile (ATK, Haste)
inline const char* kBgTileAtCap   = "#1e3a5f";  // stat tile when value = cap
inline const char* kBgTile        = "#252540";  // capped stat tile (attr/resist)

// ── Surfaces ──────────────────────────────────────────────────────────────────
inline const char* kSurfaceSection = "#141f35";
inline const char* kSurfaceMid     = "#16161e";
inline const char* kSurfaceDark    = "#1a1a2e";

// ── Borders ───────────────────────────────────────────────────────────────────
inline const char* kBorderSub    = "#131d2e";  // subtle row separator
inline const char* kBorderMid    = "#2a3a5a";  // selection / mid border
inline const char* kBorderCard   = "#3a4a6a";  // card / combo border
inline const char* kBorderAccent = "#4a6a9a";  // hover / focus highlight

// ── Text ──────────────────────────────────────────────────────────────────────
inline const char* kTextPrimary   = "#e0e0e0";  // headings, key values
inline const char* kTextBase      = "#c0c0c0";  // body text, item names
inline const char* kTextSecondary = "#888888";  // secondary labels, grid keys
inline const char* kTextMuted     = "#7a7a7a";  // cap dividers, very dim — ≥4.5:1 on kBgMain
inline const char* kTextDim       = "#555555";  // disabled state

// P1 contrast fixes — these replace colors that failed WCAG AA (< 4.5:1):
inline const char* kTextEmptySlot = "#6b7899";  // "— vide —" in inventory (~4.5:1 on kBgBase)
inline const char* kTextSlotLabel = "#5a7a9a";  // slot abbreviations Charm/Head (≥4.5:1)
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
