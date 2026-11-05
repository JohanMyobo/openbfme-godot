// OpenBFME. GPL-3.0.
//
// The start of a skirmish in the logic (lane START-1): the consumer of the NewGameMessage the lobby's StartGame queued (closes the consumer half of S-178).
//
// WHEN: the message is consumed at logic frame 0, before any logic frame ran: prepareNewGame is called with the message and seeds the logic RNG from GameInfo::seed
// (RotWK InitGameLogicRandom at the lobby's StartGame, RW 0x928736; the generator is the caller's), resolves every random choice in retail's order (SkirmishRandom)
// and returns the resolved GameInfo and the RNG with its state after the draws; LiveGame::load then builds the sides, the players, the map's objects and every
// player's starting base from that (it takes the RNG over, so the draws of the resolution and of the object creation are one stream).  The load screen of the host is
// initialised from the resolved info between the two calls, as retail does (RW 0x62DC3E: resolve, then the load screen).

#pragma once

#include "Common/PlayerTemplate.h"
#include "Common/RandomValue.h"
#include "GameClient/MapCache.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/GameInfo.h"

#include <string>
#include <vector>

struct NewGameStart
{
	NewGameMessage message;      ///< the message with every random choice resolved
	GameLogicRandom random;      ///< seeded from message.game.seed, advanced by the resolution
	int localSlot = 0;           ///< GameInfo::getLocalSlotNum: the first human slot
	bool observer = false;       ///< lane ONLINE-3: a spectator (no slot; localSlot -1): the ReplayObserver side is the local player, as in retail's replay mode
	std::string mapName;         ///< LiveGame's map name ("map mp evendim") of the cache key
	int mapPlayerCount = 0;      ///< validated cache count, checked against the loaded map before creating players
	std::vector<std::string> notes;
	// notre version, BFME1 (SinglePlayerSkirmishGameInfo) : le camp de la carte que chaque emplacement reprend ; vide : une escarmouche ordinaire (Player_<n>)
	std::vector<std::string> namedSlots;
	explicit NewGameStart(RandomAlgorithm algorithm) : random(algorithm) {}
};

// notre version, BFME1 : un emplacement d'une carte SPS de la campagne (ActCampaignLogic::skirmishSlotsOf)
struct NamedSlot
{
	int slot = 0;
	std::string name, faction;
	int startPos = 0, team = -1;
	bool human = false;
};

namespace NewGame
{
// The map name of a map cache key ("maps/map mp evendim/map mp evendim.map" -> "map mp evendim"); false when the key is not that shape.
bool mapNameOfKey(const std::string &key, std::string &out);
// Consumes `in`: false + *error when the mode is not a skirmish, the map is not in the cache, the game has no player, or the random choices cannot be resolved.
bool prepareNewGame(const NewGameMessage &in, const PlayerTemplateStore &templates, const GameLogicSettings &settings, const std::vector<MapCacheEntry> &maps,
	RandomAlgorithm algorithm, NewGameStart &out, std::string *error);
// notre version, BFME1. FAITS CIBLE (lotrbfme.exe) : le message de nouvelle partie du mode 6 (0xA13EF9 ff : SinglePlayerSkirmishGameInfo, carte "maps\\<nom>\\<nom>.map") ; le mode 6
// compte comme une escarmouche (0x75F0A0 : mode 2 ou 6) ; les bases de départ sont posées par emplacement comme en escarmouche (0x79555B -> 0x78A9F0). Pas de cache de cartes ni de choix
// au hasard : tout vient du fichier de campagne. La graine vient de timeGetTime (0xA13E1C). Faux + *error pour un camp inconnu, un emplacement doublé, ou aucun joueur humain.
// FAIT DONNEUR (ZH GameInfo::reset) : l'argent de départ est celui de GameData.
bool prepareNamedGame(const std::string &mapName, const std::vector<NamedSlot> &slots, const PlayerTemplateStore &templates, RandomAlgorithm algorithm, std::uint32_t seed, int difficulty,
	int startingCash, NewGameStart &out, std::string *error);
std::vector<std::string> stopLines();
} // namespace NewGame
