// OpenBFME. GPL-3.0.
//
// ScriptActions, notre version : les actions et conditions de script que l'IA d'escarmouche de BFME1 utilise pour bâtir (ses bibliothèques
// Libraries\AI_*Skirmish, ai_economy_execution, ai_initialize ...). RotWK les garde ; le port ne les avait pas (comptées non portées).
// See ScriptActions.h.
//
// TARGET FACTS (RotWK game.dat, caveat S-001 ; le répartiteur des actions RW 0x7CAFA5, sa table RW 0x7CF857 ; celui des conditions RW 0x7EB7CD, sa table
// RW 0x7ED414 indexée par l'ordinal - 5) :
//   * SET_UNIT_REFERENCE (référence, unité) 376 -> RW 0x7C2AAC(référence, unité, 0) : l'unité par nom (RW 0x75A3D8) ; la référence (RW 0x608397) puis le
//     cache des noms (RW 0x60A1D5) ; SET_UNIT_REFERENCE_TO_REFERENCE 378 -> le même (.., 1) : la référence copiée (RW 0x608504) ;
//   * NAMED_BASE_UNPACK (unité, référence) 381 / NAMED_BASE_UNPACK_FREE 493 -> RW 0x7BED64(unité, référence, gratuit) : l'unité, contrôlée par le joueur
//     courant (RW 0x6057EE) ; son CastleBehavior (RW 0x68BDA5) qui peut se déployer (RW 0x7983E3(1)), que ce joueur a le droit de déployer
//     (RW 0x79A472) et, sauf gratuit, peut payer (RW 0x79969F) ; initiateUnpack(gratuit) (RW 0x79C17D) ; la référence (RW 0x608397) ;
//   * BUILD_BASE_BUILDING (type, unité, référence) 383 -> RW 0x7BEE14 : l'unité, son joueur qui a le droit de bâtir (Player + 0x359) et qui est le
//     joueur courant ; le type (RW 0x6D1305) ; le CastleBehavior a un emplacement libre pour lui (RW 0x7996D3) ; le joueur peut le payer
//     (RW 0x6AAA2E(type, 0)) ; le premier emplacement qui le prend le bâtit (RW 0x79A5EC(type, -2, 0)) ; le bâtiment reçoit la référence (RW 0x608397)
//     et entre dans le cache des noms (RW 0x60A1D5) ;
//   * BUILD_BASE_BUILDING_PER_TACTICAL_MARKER (type, mode, repère, unité, référence) 385 -> RW 0x7C718B : les mêmes tests ; le repère (RW 0x79993D) est
//     l'objet dont le modèle porte ce nom, le plus proche du camp ; l'emplacement libre le plus proche (mode 0) ou le plus loin (mode 1) de lui le bâtit
//     (RW 0x79A688) ; sans repère, le premier libre (RW 0x79A5EC(type, -2, 0)) ;
//   * SET_PLAYER_MONEY_TO_COUNTER (joueurs, compteur) 404 -> RW 0x7C3655 : la somme de l'argent (Player + 0x90 + 4) des joueurs du masque ;
//     SET_PLAYER_COMMAND_POINTS_AVAILABLE_TO_COUNTER 408 -> RW 0x7C37C9 : les points de commandement disponibles (RW 0x6A7F6A(1)) du premier joueur ;
//   * UNIT_THREAT_LEVEL (unité, op, valeur, rayon) 115 -> RW 0x7E5970 et TEAM_THREAT_LEVEL (équipe, ..) 116 -> RW 0x7E8A25 : la menace (RW 0x697B41 /
//     0x7A6A20, ScriptEngine::unitThreat / teamThreat) comparée à la valeur (op 0 <, 1 <=, 2 ==, 3 >=, 4 >, 5 !=) ;
//   * TEAM_RECRUIT_UNITS (équipe, nombre, type) 397 -> RW 0x7C2BFB : l'équipe (RW 0x759FDA), le modèle (RW 0x6D1305) et la liste de types
//     (RW 0x759158) du même nom ; le rayon : MaxRecruitRadius (AIData + 0x5C) quand l'équipe a une unité (RW 0x7A0956(0)), sinon 1000000.0
//     (RW 0xBDCDC0) ; Team::recruitUnits RW 0x7A491C (modèle, liste, nombre, rayon) : autour du centre de l'équipe (RW 0x7A02E1), autant de fois que
//     le nombre, l'objet le plus proche (distance 2D au carré, une égalité prend le dernier) parmi tous les objets du joueur de l'équipe : du modèle
//     (RW 0x73D5C2, ou la liste RW 0x7A407A du modèle le nomme) ou de la liste (RW 0x778EA0) ; hors de cette équipe ; son équipe active (+ 0x5D),
//     de priorité de production (prototype + 0x21C) plus basse que celle de cette équipe, recrutable (l'équipe par défaut, ou teamIsAIRecruitable
//     + 0x210, sauf le réglage de l'équipe + 0x110 / + 0x111) ; son IA recrutable (+ 0x3BE) ; pas HELD (+ 0x1C8 bit 3) ; il rejoint l'équipe
//     (RW 0x69954A) et le rayon repart du sien ;
//   * TEAM_RECRUIT_UNITS_FROM_TEAM (équipe, nombre, type, équipe source) 398 -> RW 0x7C2C7A : l'équipe (RW 0x759FDA, créée au besoin), la source
//     (sans création), le modèle et la liste du type ; Team RW 0x7A449F (modèle, liste, nombre, source) : autour du centre de l'équipe (RW 0x7A02E1),
//     autant de fois que le nombre, le membre le plus proche (distance 2D au carré, une égalité prend le dernier) des équipes du prototype de la
//     source (+ 0x334) : du modèle ou de la liste, son IA recrutable (+ 0x3BE), pas HELD ; il rejoint l'équipe (RW 0x69954A) ;
//   * PLAYER_HAS_OBJECT_OF_VETERANCY (joueur, type, op, niveau) 146 -> RW 0x7E99F4 : le premier joueur du paramètre (RW 0x6A85B6, aucun : faux) ;
//     la liste de types du nom, sinon son modèle ; un membre d'une équipe d'un prototype du joueur, de la liste (RW 0x778EA0) ou du modèle
//     (RW 0x73D5C2), dont le rang (ExperienceTracker + 0x24) se compare au niveau (op 0 <, 1 <=, 2 ==, 3 >=, 4 >, 5 !=) -> vrai ;
//   * PLAYER_DESTROYED_N_BUILDINGS_PLAYER (joueur, N, joueur) 71 -> RW 0x7EC4A2 -> RW 0x7E53ED : toujours faux (les joueurs résolus, sans suite) ;
//   * COMPARE_NUM_PLAYERS_IN_GAME (op, nombre) 168 -> RW 0x7ED0FE -> RW 0x7E5FA5 : les joueurs jouables vivants (RW 0x6A8630(0)) comparés au nombre ;
//   * PLAYER_HAS_N_OR_FEWER_BASES (joueur, N) 131 -> RW 0x7EBDB5 -> RW 0x7E64CD : faux avant SecondsBeforeBaseCheckActive ; sinon vrai quand les
//     membres VITAL_FOR_BASE_SURVIVAL des joueurs du paramètre (RW 0x6ABB6A) ne dépassent pas N ;
//   * CAN_BUILD_OBJECTTYPE_AT_BASE (joueur, unité, type) 169 -> RW 0x7E5BF4 : le modèle, l'unité, le premier joueur du paramètre (aucun : faux) qui
//     doit la contrôler ; son CastleBehavior (RW 0x68BDA5) déployé avec un emplacement libre pour le modèle (RW 0x7996D3) -> vrai ;
//   * NAMED_BASE_UNPACKABLE_FOR_PLAYER (unité, joueur) 121 -> RW 0x7E5142 : l'unité contrôlée par le premier joueur du masque, son CastleBehavior qui peut
//     se déployer (RW 0x7983E3(1)), que ce joueur a le droit de déployer et peut payer.
//   * SKIRMISH_SPECIAL_POWER_READY (joueurs, pouvoir) 85 -> RW 0x7E9C33 (ZH evaluateSkirmishSpecialPowerIsReady) : l'entier du paramètre du pouvoir
//     (+ 0x20) sert de prochaine frame de test : -1 ou une frame à venir -> faux ; le pouvoir introuvable (RW 0x69C146) -> -1 et faux ; sinon, pour
//     chaque joueur du paramètre (RW 0x75968D / 0x6A85EE), ses prototypes (+ 0x34C), leurs équipes (+ 0x334), leurs membres : ni UNDER_CONSTRUCTION
//     (+ 0x94 bit 2), ni désactivé (+ 0x1C8), son module du pouvoir (RW 0x68C26D) utilisable (RW 0x7B1D79) : prêt (vslot 0x04) -> vrai ; sinon sa
//     frame de disponibilité (vslot 0x10) abaisse la prochaine frame de test, qui part de maintenant + 10 s (RW 0xD9F608 * 10) ; elle est gardée, faux.
// INFERENCE / NOT PORTED : RW 0x7E5142 essaie ensuite les boutons CASTLE_UNPACK_EXPLICIT_OBJECT (type 0x32) du jeu de commandes de l'unité (RW 0x7E51DB ..),
// non porté (faux) ; RW 0x6AAA2E est rendu par le prix (calcCostToBuild <= l'argent) comme BuildCommands ; la clé de la référence copiée par RW 0x608504
// (non lue) est prise comme la référence source du camp courant.

#include "GameLogic/ScriptEngine/ScriptActions.h"

#include <algorithm>

#include "Common/BuildAssistant.h"
#include "Common/Player.h"
#include "Common/Team.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/AI/AIPlayerTeams.h"
#include "Common/Thing/KindOfTokens.h"
#include "GameLogic/Economy.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/CastleModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "Common/SpecialPower.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ScriptEngine/ScriptConditions.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"
#include "GameLogic/ScriptEngine/Scripts.h"
#include "GameLogic/SimMath.h"

namespace
{
const ScriptParameter &param(const std::vector<ScriptParameter> &params, size_t i)
{
	static const ScriptParameter kEmpty;
	return i < params.size() ? params[i] : kEmpty;
}

bool compareThreat(int op, float threat, float value)
{
	switch (op)
	{
	case 0: return threat < value;
	case 1: return threat <= value;
	case 2: return threat == value;
	case 3: return threat >= value;
	case 4: return threat > value;
	case 5: return threat != value;
	default: return false;
	}
}

// RW 0x4035AC
float lengthEstimate(float x, float y)
{
	const float ax = x < 0.0f ? SimMath::subf32(0.0f, x) : x, ay = y < 0.0f ? SimMath::subf32(0.0f, y) : y;
	return ax > ay ? SimMath::addf32(ax, SimMath::mulf32(ay, 0.25f)) : SimMath::addf32(ay, SimMath::mulf32(ax, 0.25f));
}

// RW 0x79993D : parmi tous les objets, celui dont le modèle porte le nom du repère, le plus proche (Coord3D::lengthEstimate) de l'objet du camp
Object *nearestMarker(GameLogic &logic, const Object &castle, const std::string &marker)
{
	Object *best = nullptr;
	float bestDist = 0.0f;
	for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
	{
		if (!o->getTemplate() || o->getTemplate()->getName() != marker)
		{
			continue;
		}
		const float d = lengthEstimate(SimMath::subf32(o->getPosition()->x, castle.getPosition()->x), SimMath::subf32(o->getPosition()->y, castle.getPosition()->y));
		if (!best || d < bestDist)
		{
			best = o;
			bestDist = d;
		}
	}
	return best;
}
// RW 0x7A02E1 : la moyenne des positions des membres vivants (et pas DESTROYED) de l'équipe, (0, 0, 0) sans membre
Coord3D teamCentre(const Team &t)
{
	float x = 0.0f, y = 0.0f, z = 0.0f;
	int n = 0;
	for (const Object *o = t.getFirstMember(); o; o = o->friend_teamNext())
	{
		if (!o->isEffectivelyDead() && !o->isDestroyed())
		{
			x = SimMath::addf32(x, o->getPosition()->x);
			y = SimMath::addf32(o->getPosition()->y, y);
			z = SimMath::addf32(o->getPosition()->z, z);
			++n;
		}
	}
	if (n > 0)
	{
		const float f = SimMath::divf32(1.0f, (float)n);
		x = SimMath::mulf32(f, x);
		y = SimMath::mulf32(f, y);
		z = SimMath::mulf32(f, z);
	}
	return Coord3D{ x, y, z };
}

// RW 0x7A491C Team::recruitUnits : le nombre d'unités recrutées
int recruitUnits(GameLogic &logic, Team &team, const ThingTemplate *tt, const std::vector<std::string> *types, int count, float radius)
{
	Player *owner = team.getControllingPlayer();
	TeamPrototype *mine = team.getPrototype();
	const float radiusSq = SimMath::mulf32(radius, radius);
	const Coord3D pos = teamCentre(team);
	int recruited = 0;
	while (recruited < count)
	{
		Object *best = nullptr;
		float bestDist = radiusSq;
		for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
		{
			if (o->getControllingPlayer() != owner)
			{
				continue;
			}
			bool match = tt && BuildAssistant::isEquivalentTo(o->getTemplate(), tt); // RW 0x73D5C2 ; RW 0x7A407A en est un cas
			if (types)
			{
				for (const std::string &n : *types)
				{
					match = match || n == o->getTemplate()->getName(); // RW 0x778EA0
				}
			}
			Team *from = o->getTeam();
			if (!match || !from || from == &team || !from->isActive())
			{
				continue;
			}
			TeamPrototype *theirs = from->getPrototype();
			if (!theirs || !mine || logic.aiTeams().priority(*theirs) >= logic.aiTeams().priority(*mine))
			{
				continue;
			}
			// + 0x210 teamIsAIRecruitable, ou l'équipe par défaut. INFERENCE : le réglage propre à l'équipe (+ 0x110 / + 0x111) n'a pas d'écrivain
			// porté, il n'est jamais posé
			const bool recruitable = from == owner->getDefaultTeam() || theirs->getDict().getBool("teamIsAIRecruitable");
			// l'IA recrutable (+ 0x3BE) : son seul écrivain, UNIT_AFFECT_OBJECT_PANEL_FLAGS AI_Recruitable, n'est pas porté (S-1186) : toujours vraie
			if (!recruitable || (o->getDisabledMask() & (1u << DISABLED_HELD)) != 0)
			{
				continue;
			}
			const float dx = SimMath::subf32(pos.x, o->getPosition()->x), dy = SimMath::subf32(pos.y, o->getPosition()->y);
			const float d = SimMath::addf32(SimMath::mulf32(dy, dy), SimMath::mulf32(dx, dx));
			if (d > bestDist)
			{
				continue;
			}
			bestDist = d;
			best = o;
		}
		if (!best)
		{
			break;
		}
		best->setTeam(&team); // RW 0x69954A
		++recruited;
	}
	return recruited;
}

// RW 0x7A449F Team::recruitUnitsFromTeam : le nombre d'unités recrutées
int recruitUnitsFromTeam(Team &team, const ThingTemplate *tt, const std::vector<std::string> *types, int count, Team &source)
{
	const Coord3D pos = teamCentre(team); // RW 0x7A02E1
	TeamPrototype *proto = source.getPrototype();
	int recruited = 0;
	while (proto && recruited < count)
	{
		Object *best = nullptr;
		float bestDist = 0.0f;
		// toutes les équipes du prototype de la source (+ 0x334, depuis la tête : la plus récente), pas seulement la source ; ni l'activité ni la
		// priorité de l'équipe ne sont lues, et un membre de cette équipe-ci peut être « recruté » (il y reste, et compte)
		for (auto it = proto->teams().rbegin(); it != proto->teams().rend(); ++it)
		{
			for (Object *o : ScriptConditions::members(**it))
			{
				bool match = tt && BuildAssistant::isEquivalentTo(o->getTemplate(), tt); // RW 0x73D5C2 ; RW 0x7A407A en est un cas
				if (types)
				{
					for (const std::string &n : *types)
					{
						match = match || n == o->getTemplate()->getName(); // RW 0x778EA0
					}
				}
				// l'IA recrutable (+ 0x3BE) : toujours vraie, comme dans recruitUnits (S-1186)
				if (!match || (o->getDisabledMask() & (1u << DISABLED_HELD)) != 0)
				{
					continue;
				}
				const float dx = SimMath::subf32(pos.x, o->getPosition()->x), dy = SimMath::subf32(pos.y, o->getPosition()->y);
				const float d = SimMath::addf32(SimMath::mulf32(dy, dy), SimMath::mulf32(dx, dx));
				if (best && d > bestDist)
				{
					continue;
				}
				bestDist = d;
				best = o;
			}
		}
		if (!best)
		{
			break;
		}
		++recruited;
		best->setTeam(&team); // RW 0x69954A
	}
	return recruited;
}
} // namespace

bool ScriptActions::executeBaseAction(ScriptEngine &engine, const ScriptActionRec &a, const std::string &name)
{
	GameLogic &logic = engine.logic();
	if (name == "SET_UNIT_REFERENCE") // RW 0x7C2AAC(.., 0)
	{
		if (Object *o = engine.getUnitNamed(param(a.params, 1).stringValue))
		{
			engine.setReference(param(a.params, 0).stringValue, *o);
			engine.nameInCache(param(a.params, 0).stringValue, *o);
		}
		return true;
	}
	if (name == "SET_UNIT_REFERENCE_TO_REFERENCE") // RW 0x7C2AAC(.., 1) -> RW 0x608504
	{
		engine.copyReference(param(a.params, 0).stringValue, param(a.params, 1).stringValue);
		return true;
	}
	if (name == "NAMED_BASE_UNPACK" || name == "NAMED_BASE_UNPACK_FREE") // RW 0x7BED64
	{
		const bool free = name == "NAMED_BASE_UNPACK_FREE";
		Object *o = engine.getUnitNamed(param(a.params, 0).stringValue);
		Player *owner = o ? o->getControllingPlayer() : nullptr;
		if (!o || owner != engine.currentPlayer())
		{
			return true;
		}
		CastleBehavior *castle = ScriptConditions::castleOf(logic, *o);
		if (!castle || !castle->canUnpack(true) || !castle->isPlayerAllowedToPackOrUnpack(owner) || (!free && !castle->canAffordUnpack(owner)))
		{
			return true;
		}
		castle->initiateUnpack(free, nullptr);
		engine.setReference(param(a.params, 1).stringValue, *o);
		return true;
	}
	if (name == "BUILD_BASE_BUILDING") // RW 0x7BEE14
	{
		Object *o = engine.getUnitNamed(param(a.params, 1).stringValue);
		Player *owner = o ? o->getControllingPlayer() : nullptr;
		if (!owner || !owner->canBuildBase() || owner != engine.currentPlayer())
		{
			return true;
		}
		const ThingTemplate *tt = logic.things().findTemplate(param(a.params, 0).stringValue);
		CastleBehavior *castle = tt ? ScriptConditions::castleOf(logic, *o) : nullptr;
		if (!castle || castle->state() != CastleBehavior::STATE_UNPACKED)
		{
			return true;
		}
		if ((std::uint32_t)BuildAssistant::calcCostToBuild(*tt, owner, nullptr, -1) > owner->getMoney()->countMoney()) // RW 0x6AAA2E
		{
			return true;
		}
		// RW 0x79A5EC(type, -2, 0) : les emplacements dans l'ordre, le premier libre le bâtit (le test RW 0x7996D3 est le même parcours)
		if (Object *built = castle->preBuildForScript(*tt))
		{
			engine.setReference(param(a.params, 2).stringValue, *built);
			engine.nameInCache(param(a.params, 2).stringValue, *built);
		}
		return true;
	}
	if (name == "BUILD_BASE_BUILDING_PER_TACTICAL_MARKER") // RW 0x7C718B
	{
		Object *o = engine.getUnitNamed(param(a.params, 3).stringValue);
		Player *owner = o ? o->getControllingPlayer() : nullptr;
		if (!owner || !owner->canBuildBase() || owner != engine.currentPlayer())
		{
			return true;
		}
		const ThingTemplate *tt = logic.things().findTemplate(param(a.params, 0).stringValue);
		CastleBehavior *castle = tt ? ScriptConditions::castleOf(logic, *o) : nullptr;
		if (!castle || castle->state() != CastleBehavior::STATE_UNPACKED)
		{
			return true;
		}
		if ((std::uint32_t)BuildAssistant::calcCostToBuild(*tt, owner, nullptr, -1) > owner->getMoney()->countMoney()) // RW 0x6AAA2E
		{
			return true;
		}
		// le repère le plus proche du camp (RW 0x79993D) : l'emplacement selon le mode (RW 0x79A688) ; sans repère, le premier libre (RW 0x79A5EC(type, -2, 0))
		Object *marker = nearestMarker(logic, *castle->getObject(), param(a.params, 2).stringValue);
		Object *built = marker ? castle->preBuildNear(*tt, *marker->getPosition(), param(a.params, 1).intValue) : castle->preBuildForScript(*tt);
		if (built)
		{
			engine.setReference(param(a.params, 4).stringValue, *built);
			engine.nameInCache(param(a.params, 4).stringValue, *built);
		}
		return true;
	}
	if (name == "TEAM_RECRUIT_UNITS") // RW 0x7C2BFB
	{
		Team *team = ScriptConditions::team(engine, param(a.params, 0).stringValue);
		if (!team)
		{
			return true;
		}
		const std::string &type = param(a.params, 2).stringValue;
		const ThingTemplate *tt = logic.things().findTemplate(type);
		const std::vector<std::string> *types = engine.findObjectList(type);
		const float radius = ScriptConditions::teamHasAnyUnits(*team) ? logic.settings().maxRecruitRadius : 1000000.0f; // RW 0xBDCDC0
		recruitUnits(logic, *team, tt, types, param(a.params, 1).intValue, radius);
		return true;
	}
	if (name == "TEAM_RECRUIT_UNITS_FROM_TEAM") // RW 0x7C2C7A
	{
		// l'équipe est créée au besoin (RW 0x759FDA(.., 1)), la source non
		Team *team = ScriptConditions::team(engine, param(a.params, 0).stringValue, true);
		Team *source = team ? ScriptConditions::team(engine, param(a.params, 3).stringValue) : nullptr;
		if (!source)
		{
			return true;
		}
		const std::string &type = param(a.params, 2).stringValue;
		recruitUnitsFromTeam(*team, logic.things().findTemplate(type), engine.findObjectList(type), param(a.params, 1).intValue, *source);
		return true;
	}
	if (name == "SET_PLAYER_MONEY_TO_COUNTER") // RW 0x7C3655
	{
		std::uint32_t sum = 0;
		for (Player *p : ScriptConditions::players(engine, param(a.params, 0).stringValue))
		{
			sum += p->getMoney()->countMoney();
		}
		engine.counter(param(a.params, 1).stringValue).value = (std::int32_t)sum;
		return true;
	}
	if (name == "SET_PLAYER_COMMAND_POINTS_AVAILABLE_TO_COUNTER") // RW 0x7C37C9
	{
		if (Player *p = ScriptConditions::firstPlayer(engine, param(a.params, 0).stringValue))
		{
			engine.counter(param(a.params, 1).stringValue).value = p->commandPointsAvailable();
		}
		return true;
	}
	return false;
}

bool ScriptActions::evaluateBaseCondition(ScriptEngine &engine, const ScriptCondition &c, const std::string &name, bool &result)
{
	GameLogic &logic = engine.logic();
	if (name == "UNIT_THREAT_LEVEL") // RW 0x7E5970
	{
		Object *o = engine.getUnitNamed(param(c.params, 0).stringValue);
		result = o && compareThreat(param(c.params, 1).intValue, engine.unitThreat(*o, param(c.params, 3).realValue), param(c.params, 2).realValue);
		return true;
	}
	if (name == "TEAM_THREAT_LEVEL") // RW 0x7E8A25
	{
		Team *t = ScriptConditions::team(engine, param(c.params, 0).stringValue);
		result = t && compareThreat(param(c.params, 1).intValue, engine.teamThreat(*t, param(c.params, 3).realValue), param(c.params, 2).realValue);
		return true;
	}
	if (name == "PLAYER_HAS_OBJECT_OF_VETERANCY") // RW 0x7E99F4
	{
		result = false;
		Player *p = ScriptConditions::firstPlayer(engine, param(c.params, 0).stringValue);
		if (!p)
		{
			return true;
		}
		const std::string &type = param(c.params, 1).stringValue;
		const std::vector<std::string> *types = engine.findObjectList(type);
		const ThingTemplate *tt = types ? nullptr : logic.things().findTemplate(type);
		const int op = param(c.params, 2).intValue, level = param(c.params, 3).intValue;
		for (const auto &proto : logic.players().teams().prototypes())
		{
			if (proto->getControllingPlayer() != p)
			{
				continue;
			}
			for (Team *t : proto->teams())
			{
				for (Object *o = t->getFirstMember(); o; o = o->friend_teamNext())
				{
					bool match = false;
					if (types)
					{
						match = std::find(types->begin(), types->end(), o->getTemplate()->getName()) != types->end(); // RW 0x778EA0
					}
					else
					{
						match = BuildAssistant::isEquivalentTo(o->getTemplate(), tt); // RW 0x73D5C2
					}
					const ExperienceTracker *xp = match ? o->getExperienceTracker() : nullptr; // + 0x26C
					if (xp && compareThreat(op, (float)xp->getRank(), (float)level))   // le rang (+ 0x24), comparaison entière
					{
						result = true;
						return true;
					}
				}
			}
		}
		return true;
	}
	if (name == "PLAYER_DESTROYED_N_BUILDINGS_PLAYER") // RW 0x7EC4A2 -> RW 0x7E53ED
	{
		result = false; // RotWK résout les deux joueurs (RW 0x75968D, 0x6A85EE) puis rend toujours faux
		return true;
	}
	if (name == "PLAYER_HAS_N_OR_FEWER_BASES") // RW 0x7EBDB5 -> RW 0x7E64CD
	{
		// faux avant SecondsBeforeBaseCheckActive (GlobalData + 0x110C) x 5 frames ; sinon les objets VITAL_FOR_BASE_SURVIVAL (KindOf 177, sans
		// exclusion) des membres des équipes des joueurs du paramètre (Player RW 0x6ABB6A -> TeamPrototype RW 0x7A184C -> Team RW 0x7A0555) : vrai
		// quand ce compte ne dépasse pas N
		const unsigned delay = (unsigned)SimMath::truncToInt32(SimMath::mulf32(SimMath::sseFromInt32(5), logic.settings().secondsBeforeBaseCheckActive));
		result = false;
		if (logic.getFrame() < delay)
		{
			return true;
		}
		static const int vital = KindOfTokens::indexOf("VITAL_FOR_BASE_SURVIVAL");
		std::int32_t count = 0;
		for (Player *p : ScriptConditions::players(engine, param(c.params, 0).stringValue))
		{
			for (const auto &proto : logic.players().teams().prototypes())
			{
				if (proto->getControllingPlayer() != p)
				{
					continue;
				}
				for (Team *t : proto->teams())
				{
					for (Object *o = t->getFirstMember(); o; o = o->friend_teamNext())
					{
						count += o->isKindOf((unsigned)vital) ? 1 : 0;
					}
				}
			}
		}
		result = param(c.params, 1).intValue >= count;
		return true;
	}
	if (name == "COMPARE_NUM_PLAYERS_IN_GAME") // RW 0x7ED0FE -> RW 0x7E5FA5
	{
		// le nombre de joueurs jouables vivants (RW 0x6A8630(0)) comparé à la valeur (op 0 <, 1 <=, 2 ==, 3 >=, 4 >, 5 !=, autre : faux)
		result = compareThreat(param(c.params, 0).intValue, (float)logic.economy().livePlayableCount(false), (float)param(c.params, 1).intValue);
		return true;
	}
	if (name == "CAN_BUILD_OBJECTTYPE_AT_BASE") // RW 0x7E5BF4
	{
		result = false;
		const ThingTemplate *tt = logic.things().findTemplate(param(c.params, 2).stringValue);
		Object *o = tt ? engine.getUnitNamed(param(c.params, 1).stringValue) : nullptr;
		Player *p = o ? ScriptConditions::firstPlayer(engine, param(c.params, 0).stringValue) : nullptr;
		if (!p || o->getControllingPlayer() != p)
		{
			return true;
		}
		CastleBehavior *castle = ScriptConditions::castleOf(logic, *o);
		result = castle && castle->hasFreePadFor(*tt); // RW 0x7996D3
		return true;
	}
	if (name == "SKIRMISH_SPECIAL_POWER_READY") // RW 0x7E9C33
	{
		result = false;
		// l'entier du paramètre est l'état du test (ZH Parameter::friend_setInt), propre à la condition chargée
		std::int32_t &nextFrame = const_cast<ScriptCondition &>(c).params[1].intValue;
		const UnsignedInt now = logic.getFrame();
		if (nextFrame == -1 || (nextFrame > 0 && (UnsignedInt)nextFrame > now))
		{
			return true;
		}
		UnsignedInt next = now + 10u * LOGICFRAMES_PER_SECOND;
		const SpecialPowerTemplate *power = TheSpecialPowerStore ? TheSpecialPowerStore->findSpecialPowerTemplate(param(c.params, 1).stringValue) : nullptr;
		if (!power)
		{
			nextFrame = -1;
			return true;
		}
		for (Player *p : ScriptConditions::players(engine, param(c.params, 0).stringValue))
		{
			for (const auto &proto : logic.players().teams().prototypes())
			{
				if (proto->getControllingPlayer() != p)
				{
					continue;
				}
				for (Team *t : proto->teams())
				{
					for (Object *o = t->getFirstMember(); o; o = o->friend_teamNext())
					{
						if (o->isUnderConstruction() || o->getDisabledMask() != 0)
						{
							continue;
						}
						SpecialPowerModuleInterface *mod = SpecialPowerModules::findModule(*o, power);
						if (!mod || !SpecialPowerModules::canUseSpecialPower(*o, power))
						{
							continue;
						}
						if (mod->isReady())
						{
							result = true;
							return true;
						}
						if (mod->getReadyFrame() < next)
						{
							next = mod->getReadyFrame();
						}
					}
				}
			}
		}
		nextFrame = (std::int32_t)next;
		return true;
	}
	if (name == "NAMED_BASE_UNPACKABLE_FOR_PLAYER") // RW 0x7E5142
	{
		result = false;
		Object *o = engine.getUnitNamed(param(c.params, 0).stringValue);
		Player *p = o ? ScriptConditions::firstPlayer(engine, param(c.params, 1).stringValue) : nullptr;
		if (!p || o->getControllingPlayer() != p)
		{
			return true;
		}
		CastleBehavior *castle = ScriptConditions::castleOf(logic, *o);
		result = castle && castle->canUnpack(true) && castle->isPlayerAllowedToPackOrUnpack(p) && castle->canAffordUnpack(p);
		return true;
	}
	return false;
}
