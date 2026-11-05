// OpenBFME. GPL-3.0.
//
// ScriptActions, notre version : les actions et conditions de script que l'IA d'escarmouche de BFME1 utilise pour bâtir et évaluer (ses bibliothèques
// Libraries\AI_*Skirmish, ai_economy_execution, ai_initialize ...) et que le port n'avait pas (comptées non portées).
// See ScriptActions.h.
//
// TARGET FACTS (RotWK game.dat, caveat S-001 ; le répartiteur des actions RW 0x7CAFA5, sa table RW 0x7CF857 ; celui des conditions RW 0x7EB7CD, sa table
// RW 0x7ED414 indexée par l'ordinal - 5) :
//   * BUILD_BASE_BUILDING_PER_TACTICAL_MARKER (type, mode, repère, unité, référence) 385 -> RW 0x7C718B : les tests de BUILD_BASE_BUILDING (RW 0x7BEE14) ;
//     le repère (RW 0x79993D) est l'objet dont le modèle porte ce nom, le plus proche du camp ; l'emplacement libre le plus proche (mode 0) ou le plus
//     loin (mode 1) de lui le bâtit (RW 0x79A688) ; sans repère, le premier libre (RW 0x79A5EC(type, -2, 0)) ;
//   * UNIT_THREAT_LEVEL (unité, op, valeur, rayon) 115 -> RW 0x7E5970 et TEAM_THREAT_LEVEL (équipe, ..) 116 -> RW 0x7E8A25 : la menace (RW 0x697B41 /
//     0x7A6A20, ScriptEngine::unitThreat / teamThreat) comparée à la valeur (op 0 <, 1 <=, 2 ==, 3 >=, 4 >, 5 !=) ;
//   * PLAYER_DESTROYED_N_BUILDINGS_PLAYER (joueur, N, joueur) 71 -> RW 0x7EC4A2 -> RW 0x7E53ED : toujours faux (les joueurs résolus, sans suite) ;
//   * SKIRMISH_SPECIAL_POWER_READY (joueurs, pouvoir) 85 -> RW 0x7E9C33 (ZH evaluateSkirmishSpecialPowerIsReady) : l'entier du paramètre du pouvoir
//     (+ 0x20) sert de prochaine frame de test : -1 ou une frame à venir -> faux ; le pouvoir introuvable (RW 0x69C146) -> -1 et faux ; sinon, pour
//     chaque joueur du paramètre (RW 0x75968D / 0x6A85EE), ses prototypes (+ 0x34C), leurs équipes (+ 0x334), leurs membres : ni UNDER_CONSTRUCTION
//     (+ 0x94 bit 2), ni désactivé (+ 0x1C8), son module du pouvoir (RW 0x68C26D) utilisable (RW 0x7B1D79) : prêt (vslot 0x04) -> vrai ; sinon sa
//     frame de disponibilité (vslot 0x10) abaisse la prochaine frame de test, qui part de maintenant + 10 s (RW 0xD9F608 * 10) ; elle est gardée, faux.
// INFERENCE : le test d'argent RW 0x6AAA2E est rendu comme BUILD_BASE_BUILDING (ScriptActionsUnits.cpp) ; la frame de test de SKIRMISH_SPECIAL_POWER_READY
// n'est pas sauvegardée (elle repart de 0 après un chargement).

#include "GameLogic/ScriptEngine/ScriptActions.h"

#include "Common/BuildAssistant.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/Team.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/CastleModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
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
		if (!castle || !castle->canBuildAtBase(tt))
		{
			return true;
		}
		if ((std::int32_t)BuildAssistant::calcCostToBuild(*tt, owner, nullptr, -1) > (std::int32_t)owner->getMoney()->countMoney()) // RW 0x6AAA2E
		{
			return true;
		}
		// le repère le plus proche du camp (RW 0x79993D) : l'emplacement selon le mode (RW 0x79A688) ; sans repère, le premier libre (RW 0x79A5EC(type, -2, 0))
		Object *marker = nearestMarker(logic, *castle->getObject(), param(a.params, 2).stringValue);
		Object *built = marker ? castle->preBuildNear(*tt, *marker->getPosition(), param(a.params, 1).intValue) : castle->buildAtBase(*tt);
		if (built && !param(a.params, 4).stringValue.empty())
		{
			engine.setReference(param(a.params, 4).stringValue, *built);
			engine.nameInCache(param(a.params, 4).stringValue, *built);
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
	if (name == "PLAYER_DESTROYED_N_BUILDINGS_PLAYER") // RW 0x7EC4A2 -> RW 0x7E53ED
	{
		result = false; // RotWK résout les deux joueurs (RW 0x75968D, 0x6A85EE) puis rend toujours faux
		return true;
	}
	if (name == "SKIRMISH_SPECIAL_POWER_READY") // RW 0x7E9C33
	{
		result = false;
		if (c.params.size() < 2)
		{
			return true;
		}
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
	return false;
}
