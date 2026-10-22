// OpenBFME. GPL-3.0.
// Derived from Command & Conquer Generals Zero Hour, (c) 2001-2003 Electronic Arts Inc., GPL-3.0 (AIPlayer / AISkirmishPlayer : TeamInQueue, WorkOrder).
//
// AIPlayerTeams, notre version (IA d'escarmouche de BFME1) : la production d'équipes de l'AIPlayer de ZH, que RotWK garde (AIPlayer.cpp,
// AISkirmishPlayer.cpp ; vtables RW 0xC79D98 / 0xC79788). BFME1 n'a pas de SkirmishAIData : un joueur ordinateur y passe par cette branche
// d'AIPlayer::update (RW 0x8F9705 : TheSkirmishAIManager ne le connaît pas), ses bibliothèques de scripts choisissant les équipes par leurs
// conditions de production.
//
// TARGET FACTS (RotWK game.dat, caveat S-001) :
//   * update RW 0x8F9705 : vslot 0x40 checkReadyTeams RW 0x8F7949, 0x44 checkQueuedTeams RW 0x8F5FD8, 0x48 doTeamBuilding (AISkirmishPlayer
//     RW 0x8F3EFE), 0x4C les achats de compétences RW 0x8F7B3B, puis RW 0x8F8F68 (les ponts) ;
//   * doTeamBuilding RW 0x8F3EFE : rien si le joueur ne peut pas bâtir d'unités (Player + 0x358, mis à 1 par le constructeur RW 0x8F3E5C) ; sans
//     équipe à lancer (+ 0x14) le compte (+ 0x18) baisse, à 0 le délai (+ 0x28) repart de 0 et une équipe peut être lancée, le compte est plafonné à
//     3 s ; le délai baisse, sous 1 : processTeamBuilding RW 0x8F9487, puis selectTeamToBuild (vslot 0x68 -> 0x58, RW 0x8F805D) quand une équipe
//     peut être lancée, et le délai repart à 2 s ;
//   * selectTeamToBuild RW 0x8F805D : parmi les prototypes du joueur (Player + 0x34C) jugés bons (isAGoodIdeaToBuildTeam, AISkirmishPlayer
//     RW 0x8F417A) la priorité la plus haute (+ 0x21C, départ -99999) ; selectTeamToReinforce (vslot 0x5C) d'abord ; un tirage
//     GetGameLogicRandomValue(0, n - 1) (AIPlayer.cpp ligne 0x737) parmi ceux de cette priorité ; buildSpecificAITeam(proto, false) ; plus d'équipe
//     à lancer, le compte = TeamSeconds (+ 0x20) x 5, divisé par TeamsPoorRate sous Poor ou par TeamsWealthyRate au-dessus de Wealthy (AIData) ;
//   * isAGoodIdeaToBuildTeam RW 0x8F417A : la condition de production (RW 0x7A3C49) ; moins d'instances que MaxInstances (+ 0x218) ; aucune équipe
//     de ce prototype en file ; puis isPossibleToRecruitTeam RW 0x8F5D5D ou isPossibleToBuildTeam(proto, 1) RW 0x8F54BA ;
//   * isPossibleToBuildTeam RW 0x8F54BA : chaque type doit avoir une usine (RW 0x8F5347, occupée ou non) ; le coût (min + max) x 0.5 x le prix
//     (RW 0x73C25F) est sommé ; argent (Player + 0x94) < coût x TeamResourcesToStart : manque d'argent ; vrai quand une usine libre existe, sinon
//     vrai seulement sans l'exigence d'usine libre ;
//   * buildSpecificAITeam RW 0x8F6DE9 : une entrée d'unité (prototype + 0x130, pas 0x18 : min, max, type) donne un ordre facultatif (max - min,
//     + 0x29) puis un ordre requis (min, + 0x18), chacun mis en tête de liste ; l'équipe naît inactive (RW 0x7A6E8E), la file la reçoit
//     (prioritaire en tête), son début est la frame courante (+ 0x24) ;
//   * processTeamBuilding RW 0x8F9487 : pour chaque ordre incomplet, recruter d'abord (Team RW 0x7A41D4 autour du domicile, rayon MaxRecruitRadius
//     quand l'équipe en a un, sinon 100000) puis, s'il en manque et qu'il n'est pas facultatif, startTraining RW 0x8F69E8 : une usine libre
//     (RW 0x8F5347), canQueueCreateUnit (vslot 0x44) = 0, un ProductionID, queueCreateUnit(type, -1, id, -1, 0, "", 0), l'usine notée (+ 0x8) ;
//   * checkQueuedTeams RW 0x8F5FD8 : le délai de construction (prototype + 0x1F8 ; < 1 : jamais) passé, une équipe au minimum construite et sans
//     unité en production passe prête (RW 0x79FE29(0)), sinon elle est rendue à l'équipe par défaut (RW 0x7A12F4) ; puis une équipe entièrement
//     construite (RW 0x8F4FF0 : les ordres non facultatifs complets, au moins une unité) passe prête ;
//   * checkReadyTeams RW 0x8F7949 : prête quand ses membres sont au repos (RW 0x7A090C) ou 60 s après son début ; elle est activée (+ 0x5D / + 0x5E).
//   * acquireEnemy RW 0x8F457D : l'ennemi courant (+ 0xA0) vivant et actif est gardé ; sinon parmi les joueurs ENNEMIS (RW 0x6ADBEB) qui ont des
//     objets (RW 0x6ABDF7) et ne sont pas PlyrCreeps : le carré de la distance entre le centre de la base (+ 0x38) et le milieu du rectangle de
//     leurs structures (RW 0x8F6511, KindOf + 0x108 & 0x80), 499999997952 pour un joueur sans unités ou inactif, + 250000 par autre IA qui le vise,
//     - 625 (plancher 0) par IA qui vise ce joueur ; le plus petit sous 999999995904 l'emporte.
// INFERENCE / NOT PORTED : selectTeamToReinforce, les renforts, executeActions (+ 0x240) et la vslot 0x4C ; le déplacement d'une recrue vers le
// domicile ; le compte de priorité (succès / échec) est appliqué au passage prêt / à l'abandon comme ZH ; l'usine de RW 0x8F5347 est un objet du
// joueur avec une ProductionUpdate dont le CommandSet offre le type (BuildAssistant::isInProducersCommandSet) ; la sauvegarde de l'état.

#pragma once

#include "Common/INIDataTypes.h"
#include "Common/Team.h"
#include "GameLogic/ObjectTypes.h"

#include <cstdint>
#include <map>
#include <vector>

class GameLogic;
class Object;
class Player;
class Team;
class TeamPrototype;
class ThingTemplate;

class AIPlayerTeams
{
public:
	explicit AIPlayerTeams(GameLogic &logic) : m_logic(logic) {}

	// les joueurs ordinateur de la partie reçoivent leur état (BFME1 seulement : `enabled` faux, rien ne tourne)
	void newGame(bool enabled);
	void reset();
	// RW 0x8F9705, chaque frame (phase 5)
	void update();
	// Player::onUnitCreated (RW 0x6AA688) -> AIPlayer::onUnitProduced : l'unité sortie d'une usine rejoint l'équipe dont l'ordre l'attendait
	void onUnitProduced(Object &factory, Object &unit);
	bool enabled() const { return m_enabled; }
	// RW 0x758AB1 « <This Player's Enemy> » -> Player::getCurrentEnemy (RW 0x6AA5F5) -> AISkirmishPlayer::getAiEnemy (RW 0x8F47EB) : toutes les
	// 5 s acquireEnemy (RW 0x8F457D) ; null pour un joueur sans IA
	Player *currentEnemy(Player &p);

private:
	struct WorkOrder
	{
		const ThingTemplate *thing = nullptr; ///< + 0x4
		ObjectID factory = INVALID_ID;        ///< + 0x8
		int numCompleted = 0;                 ///< + 0x10
		int numRequired = 0;                  ///< + 0x14
		bool required = false;                ///< + 0x18
		bool optional = false;                ///< + 0x29
	};
	struct TeamInQueue
	{
		std::vector<WorkOrder> orders; ///< + 0x14, dans l'ordre de la liste
		TeamID team = 0;               ///< + 0x1C
		UnsignedInt frameStarted = 0;  ///< + 0x24
		bool priorityBuild = false;    ///< + 0x18
	};
	struct PlayerAI
	{
		int playerIndex = -1;
		bool readyToBuildTeam = false; ///< + 0x14
		int teamTimer = 0;             ///< + 0x18
		int teamDelay = 0;             ///< + 0x28
		Coord3D baseCenter{};          ///< + 0x38
		int enemyIndex = -1;           ///< + 0xA0
		UnsignedInt nextEnemyFrame = 0;///< + 0x9C
		std::vector<TeamInQueue> buildQueue, readyQueue; ///< + 0x8, + 0xC
	};

	void update(PlayerAI &ai, Player &p);
	void checkReadyTeams(PlayerAI &ai, Player &p);
	void checkQueuedTeams(PlayerAI &ai, Player &p);
	void processTeamBuilding(PlayerAI &ai, Player &p);
	bool selectTeamToBuild(PlayerAI &ai, Player &p);
	bool isAGoodIdeaToBuildTeam(PlayerAI &ai, Player &p, TeamPrototype &proto);
	bool isPossibleToBuildTeam(Player &p, TeamPrototype &proto, bool requireIdleFactory, bool &needMoney);
	bool isPossibleToRecruitTeam(Player &p, TeamPrototype &proto);
	void buildSpecificAITeam(PlayerAI &ai, Player &p, TeamPrototype &proto, bool priorityBuild);
	bool startTraining(Player &p, WorkOrder &order);
	Object *findFactory(Player &p, const ThingTemplate &thing, bool busyOK);
	Object *tryToRecruit(Team &team, const ThingTemplate &thing, const Coord3D &pos, float radius);
	int &priority(TeamPrototype &proto);
	void endTeam(TeamInQueue &t, bool success);
	void acquireEnemy(PlayerAI &ai, Player &p);
	PlayerAI *find(const Player &p);

	GameLogic &m_logic;
	bool m_enabled = false;
	std::vector<PlayerAI> m_players;
	std::map<int, int> m_priority; ///< prototype + 0x21C, par id de prototype (le dict ne donne que la valeur de départ)
};
