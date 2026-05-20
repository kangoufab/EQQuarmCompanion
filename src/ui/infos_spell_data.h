#pragma once
// Données statiques des sorts de debuff résistance — portées depuis infos_tab.py
// Aucune requête SQL au runtime. Mis à jour manuellement depuis la BDD si nécessaire.
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <vector>

// ── SPA constants (resist debuff SPAs) ──────────────────────────────────────
inline constexpr int SPA_FR    = 46;
inline constexpr int SPA_CR    = 47;
inline constexpr int SPA_PR    = 48;
inline constexpr int SPA_DR    = 49;
inline constexpr int SPA_MR    = 50;
inline constexpr int SPA_BLANK = 254;

// SPA → resist key
inline int resistSpa(const char* r) {
    if (!r) return 0;
    if (r[0]=='M') return SPA_MR;
    if (r[0]=='F') return SPA_FR;
    if (r[0]=='C') return SPA_CR;
    if (r[0]=='P') return SPA_PR;
    if (r[0]=='D') return SPA_DR;
    return 0;
}

// ── Spell effect value formula (CalcSpellEffectValue_formula) ────────────────
inline int calcSpellEffect(int base, int max, int formula, int level) {
    int ubase = std::abs(base);
    int updown = (max != 0 && max < base) ? -1 : 1;
    int r;
    if      (formula == 0 || formula == 100)  r = ubase;
    else if (formula >= 1  && formula <= 99)  r = ubase + level * formula;
    else if (formula == 101) r = updown * (ubase + level / 2);
    else if (formula == 102) r = updown * (ubase + level);
    else if (formula == 103) r = updown * (ubase + level * 2);
    else if (formula == 104) r = updown * (ubase + level * 3);
    else if (formula == 105) r = updown * (ubase + level * 4);
    else if (formula == 109) r = updown * (ubase + level / 4);
    else if (formula == 110) r = updown * (ubase + level / 6);
    else if (formula == 119) r = ubase + level / 8;
    else if (formula == 121) r = ubase + level / 3;
    else                     r = ubase;
    if (max != 0) {
        if (updown == 1) r = std::min(r, max);
        else             r = std::max(r, max);
    }
    if (base < 0 && r > 0) r = -r;
    return r;
}

// ── Spell data types ─────────────────────────────────────────────────────────
struct InfoEffect { int spa{254}, base{0}, max{0}, formula{100}; };

struct InfoSpell {
    int         id{};
    const char* name{};
    int         targettype{5};
    int cls_enc{255}, cls_shm{255}, cls_mag{255}, cls_nec{255};
    int cls_dru{255}, cls_brd{255}, cls_clr{255}, cls_bst{255};
    std::array<InfoEffect,7> fx{};
};

inline InfoSpell mkSpell(int id, const char* name, int tt,
                          int enc, int shm, int mag, int nec,
                          int dru, int brd, int clr, int bst,
                          std::initializer_list<InfoEffect> fxList) {
    InfoSpell s;
    s.id=id; s.name=name; s.targettype=tt;
    s.cls_enc=enc; s.cls_shm=shm; s.cls_mag=mag; s.cls_nec=nec;
    s.cls_dru=dru; s.cls_brd=brd; s.cls_clr=clr; s.cls_bst=bst;
    int i=0; for (auto& f : fxList) { if(i<7) s.fx[i++]=f; }
    return s;
}

// ── Static spell data (id, name, tt, enc,shm,mag,nec,dru,brd,clr,bst, {effects}) ──
inline const std::vector<InfoSpell>& getInfoSpells() {
    static const std::vector<InfoSpell> kSpells = {
        // ── Druid FR debuffs ─────────────────────────────────────────────────
        mkSpell(29,   "Ice",                        5, 255,255,255,255, 49,255,255,255, {{79,-612,612,100},{46,-50,0,100}}),
        mkSpell(1437, "Ro`s Fiery Sundering",        5, 255,255,255,255, 39,255,255,255, {{10,0,0,100},{46,-30,0,110},{1,-10,0,109}}),
        mkSpell(2518, "Ro's Smoldering Disjunction", 5, 255,255,255,255, 56,255,255,255, {{1,-60,0,101},{46,-55,0,110},{79,-150,0,100},{2,-30,0,101}}),
        mkSpell(1600, "Breath of Ro",                5, 255,255,255,255, 52,255,255,255, {{10,0,0,100},{0,-92,0,100},{46,-1,0,101},{10,0,0,100},{10,0,0,100},{10,0,0,100},{10,0,0,100}}),
        // ── Druid CR AoE ─────────────────────────────────────────────────────
        mkSpell(433,  "Fire",                        8, 255,255,255,255, 49,255,255,255, {{79,-392,392,100},{47,-50,0,100}}),
        mkSpell(3695, "Frost Zephyr",                5, 255,255,255,255, 44,255,255,255, {{1,-30,0,101},{10,0,0,100},{10,0,0,100},{10,0,0,100},{47,-30,30,100}}),
        // ── Shaman Malo line (CR/MR/PR/FR) ───────────────────────────────────
        mkSpell(110,  "Malaise",    5,  24, 19,255,255,255,255,255,255, {{10,0,0,100},{47,-6,20,101},{50,-6,20,101},{48,-6,20,101},{46,-6,20,101}}),
        mkSpell(111,  "Malaisement",5,  44, 34,255,255,255,255,255,255, {{10,0,0,100},{47,-20,40,101},{50,-20,40,101},{48,-20,40,101},{46,-20,40,101}}),
        mkSpell(112,  "Malosi",     5, 255, 49, 51, 51,255,255,255,255, {{10,0,0,100},{47,-10,60,102},{50,-10,60,102},{48,-10,60,102},{46,-10,60,102}}),
        mkSpell(1577, "Malosini",   5,  58, 57,255, 58,255,255,255,255, {{46,0,0,100},{47,-10,60,102},{50,-10,60,102},{48,-10,60,102},{46,-10,60,102}}),
        mkSpell(1578, "Malo",       5, 255, 60,255,255,255,255,255,255, {{10,0,0,100},{47,-10,45,102},{50,-10,45,102},{48,-10,45,102},{46,-10,45,102}}),
        mkSpell(1772, "Mala",       5, 255,255, 60,255,255,255,255,255, {{10,0,0,100},{47,-10,35,102},{50,-10,35,102},{48,-10,35,102},{46,-10,35,102}}),
        // ── Enchanter Tash line (MR) ──────────────────────────────────────────
        mkSpell(676,  "Tashan",             5,  4,255,255,255,255,255,255,255, {{36,1,0,100},{50,-5,13,102}}),
        mkSpell(677,  "Tashani",            5, 20,255,255,255,255,255,255,255, {{36,1,0,100},{50,-10,23,101}}),
        mkSpell(678,  "Tashania",           5, 44,255,255,255,255,255,255,255, {{36,1,0,100},{50,-9,33,101}}),
        mkSpell(1699, "Wind of Tashani",    8, 55,255,255,255,255,255,255,255, {{36,1,0,100},{50,-10,23,101}}),
        mkSpell(1702, "Tashanian",          5, 57,255,255,255,255,255,255,255, {{36,1,0,100},{50,-9,43,101}}),
        mkSpell(1704, "Wind of Tashanian",  8, 60,255,255,255,255,255,255,255, {{36,1,0,100},{50,-10,40,101}}),
        // ── Necro Scent line (FR/PR/DR) ───────────────────────────────────────
        mkSpell(1511, "Scent of Dusk",     5, 255,255,255, 12,255,255,255,255, {{36,1,0,100},{46,-1,9,101},{48,-1,9,101},{49,-1,9,101}}),
        mkSpell(1512, "Scent of Shadow",   5, 255,255,255, 24,255,255,255,255, {{36,4,0,100},{46,-2,18,101},{48,-2,18,101},{49,-2,18,101}}),
        mkSpell(1513, "Scent of Darkness", 5, 255,255,255, 39,255,255,255,255, {{36,9,0,100},{46,-5,27,101},{48,-5,27,101},{49,-5,27,101}}),
        mkSpell(1716, "Scent of Terris",   5, 255,255,255, 52,255,255,255,255, {{36,9,0,100},{46,-7,36,101},{48,-7,36,101},{49,-7,36,101}}),
        // ── Shaman Insidious line (DR) ────────────────────────────────────────
        mkSpell(526,  "Insidious Fever",  5, 255, 19,255,255,255,255,255,255, {{35,4,0,100},{49,-10,35,101}}),
        mkSpell(527,  "Insidious Malady", 5, 255, 39,255,255,255,255,255,255, {{35,9,0,100},{49,-10,60,102}}),
        mkSpell(1573, "Insidious Decay",  5, 255, 52,255,255,255,255,255,255, {{35,9,0,100},{49,-10,0,101}}),
        // ── Bard songs ────────────────────────────────────────────────────────
        mkSpell(1451, "Occlusion of Sound",          5, 255,255,255,255,255, 55,255,255, {{47,-10,20,100},{46,-10,20,100},{10,0,0,100},{50,-10,20,100}}),
        mkSpell(3375, "Harmony of Sound",            5, 255,255,255,255,255, 65,255,255, {{47,-15,15,100},{46,-15,15,100},{10,0,0,100},{50,-15,15,100}}),
        mkSpell(743,  "Tuyen`s Chant of Flame",      5, 255,255,255,255,255, 38,255,255, {{0,-1,0,101},{10,0,0,100},{10,0,0,100},{46,-2,0,109}}),
        mkSpell(3367, "Tuyen`s Chant of Fire",       5, 255,255,255,255,255, 65,255,255, {{0,-10,0,101},{10,0,0,100},{10,0,0,100},{46,-1,0,121}}),
        mkSpell(744,  "Tuyen`s Chant of Frost",      5, 255,255,255,255,255, 46,255,255, {{0,-1,0,101},{10,0,0,100},{10,0,0,100},{47,-2,0,109}}),
        mkSpell(3373, "Tuyen`s Chant of Ice",        5, 255,255,255,255,255, 63,255,255, {{0,-10,0,101},{10,0,0,100},{10,0,0,100},{47,-1,0,121}}),
        mkSpell(3566, "Tuyen`s Chant of Poison",     5, 255,255,255,255,255, 50,255,255, {{0,-1,0,101},{10,0,0,100},{10,0,0,100},{48,-2,0,109}}),
        mkSpell(3370, "Tuyen`s Chant of Venom",      5, 255,255,255,255,255, 63,255,255, {{0,-10,0,101},{10,0,0,100},{10,0,0,100},{48,-1,0,121}}),
        mkSpell(3567, "Tuyen`s Chant of Disease",    5, 255,255,255,255,255, 42,255,255, {{0,-1,0,101},{10,0,0,100},{10,0,0,100},{49,-2,0,109}}),
        mkSpell(3363, "Tuyen`s Chant of the Plague", 5, 255,255,255,255,255, 61,255,255, {{0,-10,0,101},{10,0,0,100},{10,0,0,100},{49,-1,0,121}}),
        mkSpell(725,  "Solon's Song of the Sirens",  5, 255,255,255,255,255, 27,255,255, {{22,1,37,100},{50,-1,0,119}}),
        mkSpell(741,  "Crission`s Pixie Strike",     5, 255,255,255,255,255, 28,255,255, {{31,2,45,100},{50,-1,0,110}}),
        mkSpell(750,  "Solon's Bewitching Bravura",  5, 255,255,255,255,255, 39,255,255, {{22,1,51,100},{50,-1,0,119}}),
        mkSpell(868,  "Sionachie`s Dreams",           5, 255,255,255,255,255, 40,255,255, {{31,2,53,100},{50,-1,0,110}}),
        mkSpell(1753, "Song of Twilight",             5, 255,255,255,255,255, 53,255,255, {{31,2,55,100},{10,0,0,100},{10,0,0,100},{10,0,0,100},{50,-1,0,109}}),
        mkSpell(1764, "Denon`s Bereavement",          4, 255,255,255,255,255, 59,255,255, {{36,4,0,100},{0,-30,0,100},{21,1,0,100},{50,-15,0,100}}),
        mkSpell(707,  "Fufil`s Curtailing Chant",     5, 255,255,255,255,255, 30,255,255, {{0,-1,0,121},{10,0,0,100},{10,0,0,100},{10,0,0,100},{10,0,0,100},{10,0,0,100},{50,-2,0,109}}),
    };
    return kSpells;
}

inline const std::map<int,const InfoSpell*>& getInfoSpellMap() {
    static std::map<int,const InfoSpell*> m;
    if (m.empty())
        for (auto& s : getInfoSpells()) m[s.id] = &s;
    return m;
}

// ── Zone expansion availability (from _SPELL_ZONE_EXP) ──────────────────────
inline const std::map<int,int>& getSpellZoneExp() {
    static const std::map<int,int> m = {
        {29,0},{110,0},{111,0},{112,0},{433,0},{526,0},{527,0},
        {676,0},{677,0},{1511,0},{1512,0},{1513,0},
        {1699,0},{1704,0},{1772,0},
        {1577,1},{1578,1},{1600,1},{1702,1},
        {1437,2},{2518,3},
    };
    return m;
}

// ── Group / stacking data ────────────────────────────────────────────────────
struct InfoGroup {
    const char* id;
    const char* label;
    std::vector<std::string> resists;
    bool is_bard;
    std::vector<int> spell_ids;
};

inline const std::vector<InfoGroup>& getInfoGroups() {
    static const std::vector<InfoGroup> g = {
        {"malo",      "Malo ♦",           {"MR","FR","CR","PR"}, false, {110,111,112,1577,1578,1772}},
        {"tash",      "Tash",                  {"MR"},                false, {676,677,678,1699,1702,1704}},
        {"scent",     "Scent ♦",           {"FR","PR","DR"},      false, {1511,1512,1513,1716}},
        {"insidious", "Insidious",              {"DR"},                false, {526,527,1573}},
        {"druid_fr",  "Druide FR",              {"FR"},                false, {1437,29,2518}},
        {"fire_aoe",  "Fire AoE ⚠",         {"CR"},                false, {433}},
        {"frost_z",   "Frost Zephyr",           {"CR"},                false, {3695}},
        {"breath_ro", "Breath of Ro",           {"FR"},                false, {1600}},
        {"occl_multi","Occl./Harmony ♦",   {"FR","CR","MR"},       true,  {1451,3375}},
        {"tuyen_fr",  "Tuyen Flamme/Feu",       {"FR"},                true,  {743,3367}},
        {"tuyen_cr",  "Tuyen Gel/Glace",        {"CR"},                true,  {744,3373}},
        {"tuyen_pr",  "Tuyen Venin",            {"PR"},                true,  {3566,3370}},
        {"tuyen_dr",  "Tuyen Fléau",        {"DR"},                true,  {3567,3363}},
        {"bard_mr2",  "Bard MR (slows)",        {"MR"},                true,  {725,741,750,868}},
        {"bard_mr4",  "Denon ⚠",            {"MR"},                true,  {1764}},
        {"bard_mr5",  "Song of Twilight",       {"MR"},                true,  {1753}},
        {"bard_mr6",  "Fufil's Chant",          {"MR"},                true,  {707}},
    };
    return g;
}

// Conflicts: group_id → {conflicts_with_id, warning_text}
inline const std::map<std::string,std::pair<std::string,std::string>>& getCrossConflicts() {
    static const std::map<std::string,std::pair<std::string,std::string>> m = {
        {"druid_fr", {"scent",      "FR@slot2 — conflit avec Scent"}},
        {"fire_aoe", {"malo",       "CR@slot2 — remplace Malo!"}},
        {"bard_mr4", {"occl_multi", "MR@slot3 — conflit avec Occl./Harmony (invalide les -CR/-FR)"}},
    };
    return m;
}

// Order in which groups contribute to each resist total
inline const std::map<std::string,std::vector<std::string>>& getResistGroupOrder() {
    static const std::map<std::string,std::vector<std::string>> m = {
        {"MR", {"tash","malo"}},
        {"FR", {"druid_fr","scent","breath_ro","malo"}},
        {"CR", {"malo","fire_aoe","frost_z"}},
        {"PR", {"scent","malo"}},
        {"DR", {"insidious","scent"}},
    };
    return m;
}
inline const std::map<std::string,std::vector<std::string>>& getResistBardGroups() {
    static const std::map<std::string,std::vector<std::string>> m = {
        {"MR", {"occl_multi","bard_mr2","bard_mr4","bard_mr5","bard_mr6"}},
        {"FR", {"occl_multi","tuyen_fr"}},
        {"CR", {"occl_multi","tuyen_cr"}},
        {"PR", {"tuyen_pr"}},
        {"DR", {"tuyen_dr"}},
    };
    return m;
}

// ── Per-spell resist value computation ──────────────────────────────────────
inline int spellResistVal(const InfoSpell& sp, const char* resist, int level) {
    int spa = resistSpa(resist);
    int best = 0;
    for (auto& fx : sp.fx) {
        if (fx.spa == SPA_BLANK) break;
        if (fx.spa != spa) continue;
        if (fx.base >= 0) continue;
        int v = calcSpellEffect(fx.base, fx.max, fx.formula, level);
        if (v < best) best = v;
    }
    return best;
}

// ── Min class level for the display classes ──────────────────────────────────
inline int minClassLevel(const InfoSpell& sp) {
    int levels[] = {sp.cls_enc, sp.cls_shm, sp.cls_mag, sp.cls_nec,
                    sp.cls_dru, sp.cls_brd, sp.cls_clr, sp.cls_bst};
    int best = 255;
    for (int l : levels) if (l > 0 && l < best) best = l;
    return best;
}

// ── Classes display string ───────────────────────────────────────────────────
inline std::string classesStr(const InfoSpell& sp) {
    struct Col { const char* label; int level; };
    Col cols[] = {
        {"Enc", sp.cls_enc}, {"Shm", sp.cls_shm}, {"Mag", sp.cls_mag},
        {"Nec", sp.cls_nec}, {"Dru", sp.cls_dru}, {"Brd", sp.cls_brd},
        {"Clr", sp.cls_clr}, {"Bst", sp.cls_bst},
    };
    std::string out;
    for (auto& c : cols) {
        if (c.level > 0 && c.level < 255) {
            if (!out.empty()) out += '/';
            out += c.label + std::to_string(c.level);
        }
    }
    return out;
}

inline const char* targetLabel(int tt) {
    switch(tt) {
        case 5: return "single"; case 8: return "AoE";
        case 4: return "PB AoE"; case 9: return "Animal"; case 2: return "group";
        default: return "?";
    }
}

// ── Best spell selection (mirrors _best_in_group) ────────────────────────────
inline const InfoSpell* bestInGroup(const std::vector<int>& sids, const char* resist,
                                     int cap, bool is_bard, int expIdx)
{
    auto& smap    = getInfoSpellMap();
    auto& zoneExp = getSpellZoneExp();
    int best_val  = 0;
    const InfoSpell* best = nullptr;
    for (int sid : sids) {
        auto it = smap.find(sid);
        if (it == smap.end()) continue;
        const InfoSpell& sp = *it->second;
        if (minClassLevel(sp) > cap) continue;
        if (!is_bard) {
            auto ze = zoneExp.find(sid);
            if (ze != zoneExp.end() && ze->second > expIdx) continue;
        }
        int val = spellResistVal(sp, resist, cap);
        if (val < best_val) { best_val = val; best = &sp; }
    }
    return best;
}
