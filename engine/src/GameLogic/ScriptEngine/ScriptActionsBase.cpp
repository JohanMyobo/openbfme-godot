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
//   * NAMED_BASE_UNPACKABLE_FOR_PLAYER (unité, joueur) 121 -> RW 0x7E5142 : l'unité contrôlée par le premier joueur du masque, son CastleBehavior qui peut
//     se déployer (RW 0x7983E3(1)), que ce joueur a le droit de déployer et peut payer.
// INFERENCE / NOT PORTED : RW 0x7E5142 essaie ensuite les boutons CASTLE_UNPACK_EXPLICIT_OBJECT (type 0x32) du jeu de commandes de l'unité (RW 0x7E51DB ..),
// non porté (faux) ; RW 0x6AAA2E est rendu par le prix (calcCostToBuild <= l'argent) comme BuildCommands ; la clé de la référence copiée par RW 0x608504
// (non lue) est prise comme la référence source du camp courant.

#include "GameLogic/ScriptEngine/ScriptActions.h"

#include "Common/BuildAssistant.h"
#include "Common/Player.h"
#include "Common/Team.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/CastleModules.h"
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
