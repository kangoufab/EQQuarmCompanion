#pragma once
#include "core/types.h"
#include <QFrame>
#include <string>

// Bandeau de stats joueur (tuiles par catégorie, sans tooltips).
// className : filtre les catégories affichées selon la classe.
// expansion : "Classic" | "Kunark" | "Velious" | "Luclin" | "Planes of Power"
QFrame* makePlayerStatsBar(const PlayerTotals& totals,
                            const std::string& className,
                            const std::string& expansion);
