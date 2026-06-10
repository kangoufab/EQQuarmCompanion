#pragma once
// Helpers internes partagés entre les unités de compilation de CharacterTab
// (character_tab.cpp et character_tab_tooltip.cpp). Pas de QObject ici : aucun
// passage par MOC. `inline` (pas `static`) pour une définition unique conforme
// à l'ODR, partagée entre les deux TU.
#include "core/types.h"
#include <string>

// Lit la valeur d'un stat depuis un ItemData via sa clé ASCII neutre.
inline int getItemStat(const std::string& stat, const ItemData& item) {
    if (stat == "hp")        return item.hp;
    if (stat == "mana")      return item.mana;
    if (stat == "ac")        return item.ac;
    if (stat == "atk")       return item.atk;
    if (stat == "damage")    return item.damage;
    if (stat == "delay")     return item.delay;
    if (stat == "astr")      return item.astr;
    if (stat == "asta")      return item.asta;
    if (stat == "adex")      return item.adex;
    if (stat == "aagi")      return item.aagi;
    if (stat == "awis")      return item.awis;
    if (stat == "aint")      return item.aint;
    if (stat == "acha")      return item.acha;
    if (stat == "mr")        return item.mr;
    if (stat == "fr")        return item.fr;
    if (stat == "cr")        return item.cr;
    if (stat == "dr")        return item.dr;
    if (stat == "pr")        return item.pr;
    if (stat == "haste")     return item.haste;
    if (stat == "hp_regen")  return item.hp_regen;
    if (stat == "mana_regen") return item.mana_regen;
    return 0;
}

inline bool isAttrStat(const std::string& s) {
    return s=="astr"||s=="asta"||s=="adex"||s=="aagi"||s=="awis"||s=="aint"||s=="acha";
}
inline bool isResistStat(const std::string& s) {
    return s=="mr"||s=="fr"||s=="cr"||s=="dr"||s=="pr";
}
