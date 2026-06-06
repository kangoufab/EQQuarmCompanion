# Product

## Register

product

## Users

EverQuest players active on the Project Quarm emulator. Used during or between game sessions — typically on a second screen or in alt-tab — by a solo player who needs fast access to combat and gear analysis without leaving the game context. The user is often mid-session: they have limited attention, are in a specific mental context (fighting a boss, looting a mob, theorcrafting a raid setup), and need answers in seconds, not minutes.

## Product Purpose

EqQuarmCompanion is a desktop companion tool for Project Quarm (an EverQuest emulator running Planes of Power content). It parses a character export file and connects to the Quarm game database to provide:

- **Item upgrade evaluation** (Stuff tab): compare equipped vs candidate items across weighted stat categories, score UPGRADE/DOWNGRADE, view bag and equipped inventory.
- **NPC combat analysis** (Fight tab): incoming DPS at multiple slow percentages, loot table, special abilities, resist ratings, CH rotation planning.
- **Buff/spell planning** (Buffs tab): class-based spell selection, stacking validation, Clickies from equipped items, active buff stat contribution.
- **Resist debuff reference** (Infos tab): best debuffs available by expansion and level cap, stacking conflicts, total debuff per resist type.

Success means a player can open the app, load their character, and get a confident answer to "should I equip this?", "can I tank this NPC?", or "what slows does the shaman need?" in under 30 seconds.

## Brand Personality

Dense. Precise. Utilitarian.

The app is a professional instrument, not a game cosmetic. It earns trust through accuracy and density, not visual flair. The tone is that of a terminal or an IDE: dark, structured, functional. Colors communicate data state (good/warning/danger), not mood.

## References

- **VS Code / WezTerm**: near-black backgrounds, monospace-friendly type, syntax-color conventions for semantic meaning, minimal chrome, high information density.
- **TradingView**: dark dense data surfaces, functional color for directional signals, tabular data legible at small sizes.

## Anti-references

- SaaS cream/white ("startup web look"): light backgrounds, rounded cards, generic blue primary, generous whitespace. Wrong register entirely.
- Game-kitch (glow effects, gold borders, particle animations): decorative excess. The app serves the game; it doesn't try to look like it.
- Over-minimal/empty: excessive whitespace that wastes screen real estate while the player is alt-tabbing under time pressure.
- Flat / no visual hierarchy: undifferentiated sections, same visual weight for primary data and secondary labels.

## Design Principles

1. **Data is the UI.** Stats, numbers, and ratings are first-class elements. Typography hierarchy, color, and layout exist to make the numbers scannable at a glance, not to decorate around them.
2. **Color carries meaning, not mood.** Green = good/upgrade/safe. Orange = caution/marginal/warning. Red = danger/downgrade/immune. These conventions are non-negotiable; decorative use of these colors would corrupt the signal system.
3. **Dark-native, not dark-mode.** The app runs in a gaming environment (low ambient light, full-screen game behind it). Dark is the only register. It should feel like a tool that belongs in that environment.
4. **Density without crowding.** The goal is maximum relevant information per pixel, achieved through tight spacing and deliberate typographic hierarchy — not by eliminating breathing room entirely. Section boundaries must be visually clear.
5. **Glanceable over discoverable.** A player alt-tabbing mid-combat needs to read a result in 2 seconds. Critical outputs (DPS estimate, UPGRADE/DOWNGRADE score, resist rating) must be the most visually dominant element on each tab.

## Accessibility & Inclusion

- WCAG AA as a floor: body text ≥ 4.5:1 contrast, large/bold text ≥ 3:1.
- Reduced motion: the app uses Qt Widgets (no CSS animations), so this is handled at the OS level; no special consideration needed.
- Color-blind safety: green/orange/red signals should not rely on hue alone; pair with distinct lightness values and labels so deuteranopia users can distinguish ratings.
