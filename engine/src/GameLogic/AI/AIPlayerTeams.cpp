// OpenBFME. GPL-3.0.
// See GameLogic/AI/AIPlayerTeams.h.

#include "GameLogic/AI/AIPlayerTeams.h"

#include "Common/BuildAssistant.h"
#include "Common/Dict.h"
#include "Common/GameCommon.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RandomValue.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/AI/AIMove.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"
#include "GameLogic/SimMath.h"

#include <algorithm>
#include <string>

namespace
{
const int kPriorityNone = -99999; // RW 0x8F8076

struct UnitEntry
{
	int minCount = 0, maxCount = 0;
	const ThingTemplate *thing = nullptr;
};

// prototype + 0x130 (pas 0x18) : les entrées teamUnitMinCount / MaxCount / Type 1 .. 7 du dict (lane CAMP-1H les lit de même)
std::vector<UnitEntry> unitEntries(GameLogic &logic, const TeamPrototype &proto)
{
	std::vector<UnitEntry> out;
	const Dict &d = proto.getDict();
	for (int i = 1; i <= 7; ++i)
	{
		const std::string n = std::to_string(i);
		bool hasType = false;
		const std::string type = d.getAsciiString("teamUnitType" + n, &hasType);
		if (!hasType || type.empty())
		{
			continue;
		}
		UnitEntry e;
		e.minCount = d.getInt("teamUnitMinCount" + n);
		e.maxCount = d.getInt("teamUnitMaxCount" + n);
		e.thing = logic.things().findTemplate(type); // RW 0x6D1305 : un type inconnu est sauté
		out.push_back(e);
	}
	return out;
}

// prototype + 0x1DC / + 0x1E8 : le point de passage teamHome
bool homeLocation(GameLogic &logic, const TeamPrototype &proto, Coord3D &out)
{
	const std::string name = proto.getDict().getAsciiString("teamHome");
	const TerrainLogic *terrain = logic.terrain();
	const Waypoint *w = terrain && !name.empty() ? terrain->findWaypointByName(name) : nullptr;
	if (!w)
	{
		return false;
	}
	out = w->location;
	return true;
}

bool aiIdle(const Object &o)
{
	const AIUpdateInterface *ai = o.getAIUpdateInterface();
	return ai && ai->isIdle();
}
} // namespace

void AIPlayerTeams::reset()
{
	m_enabled = false;
	m_players.clear();
	m_priority.clear();
}

void AIPlayerTeams::newGame(bool enabled)
{
	reset();
	m_enabled = enabled;
	if (!enabled)
	{
		return;
	}
	PlayerList &players = m_logic.players();
	const TerrainLogic *terrain = m_logic.terrain();
	for (int i = 0; i < players.getPlayerCount(); ++i)
	{
		Player *p = players.getNthPlayer(i);
		if (!p || p->getPlayerType() != PLAYER_COMPUTER)
		{
			continue;
		}
		PlayerAI ai;
		ai.playerIndex = i;
		// + 0x38 : le centre de la base d'escarmouche ; ici le point de départ du joueur (Player_N_Start)
		const int start = p->getMultiplayerStartIndex();
		if (const Waypoint *w = terrain && start >= 0 ? terrain->findWaypointByName("Player_" + std::to_string(start + 1) + "_Start") : nullptr)
		{
			ai.baseCenter = w->location;
		}
		m_players.push_back(ai);
	}
}

void AIPlayerTeams::update()
{
	if (!m_enabled)
	{
		return;
	}
	for (PlayerAI &ai : m_players)
	{
		if (Player *p = m_logic.players().getNthPlayer(ai.playerIndex))
		{
			update(ai, *p);
		}
	}
}

// RW 0x8F9705 (la branche sans cerveau d'escarmouche) : 0x40, 0x44, 0x48 ; 0x4C et les ponts ne sont pas portés
void AIPlayerTeams::update(PlayerAI &ai, Player &p)
{
	checkReadyTeams(ai, p);
	checkQueuedTeams(ai, p);
	// RW 0x8F3EFE : AISkirmishPlayer::doTeamBuilding
	if (!ai.readyToBuildTeam)
	{
		--ai.teamTimer;
		if (ai.teamTimer <= 0)
		{
			ai.teamDelay = 0;
			ai.readyToBuildTeam = true;
		}
		if (ai.teamTimer > 3 * (int)LOGICFRAMES_PER_SECOND)
		{
			ai.teamTimer = 3 * (int)LOGICFRAMES_PER_SECOND;
		}
	}
	--ai.teamDelay;
	if (ai.teamDelay < 1)
	{
		processTeamBuilding(ai, p);
		if (ai.readyToBuildTeam)
		{
			selectTeamToBuild(ai, p); // vslot 0x68 -> 0x58
		}
		ai.teamDelay = 2 * (int)LOGICFRAMES_PER_SECOND;
	}
}

int &AIPlayerTeams::priority(TeamPrototype &proto)
{
	auto it = m_priority.find(proto.getID());
	if (it == m_priority.end())
	{
		it = m_priority.emplace(proto.getID(), proto.getDict().getInt("teamProductionPriority")).first;
	}
	return it->second;
}

// ZH : increaseAIPriorityForSuccess / decreaseAIPriorityForFailure
void AIPlayerTeams::endTeam(TeamInQueue &t, bool success)
{
	Team *team = m_logic.players().teams().findTeamByID(t.team);
	TeamPrototype *proto = team ? team->getPrototype() : nullptr;
	if (!proto)
	{
		return;
	}
	if (success)
	{
		priority(*proto) += proto->getDict().getInt("teamProductionPrioritySuccessIncrease");
	}
	else
	{
		priority(*proto) -= proto->getDict().getInt("teamProductionPriorityFailureDecrease");
	}
}

// RW 0x8F7949
void AIPlayerTeams::checkReadyTeams(PlayerAI &ai, Player &p)
{
	(void)p;
	const UnsignedInt now = m_logic.getFrame();
	for (size_t i = 0; i < ai.readyQueue.size();)
	{
		TeamInQueue &t = ai.readyQueue[i];
		Team *team = m_logic.players().teams().findTeamByID(t.team);
		bool ready = t.frameStarted + 60u * LOGICFRAMES_PER_SECOND < now; // RW 0x8F796E
		if (team && !ready)
		{
			// RW 0x7A090C : tous les membres vivants au repos
			ready = true;
			for (Object *o = team->getFirstMember(); o; o = o->friend_teamNext())
			{
				if (!o->isEffectivelyDead() && o->getAIUpdateInterface() && !aiIdle(*o))
				{
					ready = false;
					break;
				}
			}
		}
		if (!ready)
		{
			++i;
			continue;
		}
		if (team)
		{
			team->scriptState().beingBuilt = false; // RW 0x79FE29(0)
			team->setActive();                      // + 0x5D / + 0x5E
		}
		ai.readyQueue.erase(ai.readyQueue.begin() + (std::ptrdiff_t)i);
	}
}

// RW 0x8F5FD8
void AIPlayerTeams::checkQueuedTeams(PlayerAI &ai, Player &p)
{
	(void)p;
	const UnsignedInt now = m_logic.getFrame();
	TeamFactory &teams = m_logic.players().teams();
	for (size_t i = 0; i < ai.buildQueue.size();)
	{
		TeamInQueue &t = ai.buildQueue[i];
		Team *team = teams.findTeamByID(t.team);
		const int timeout = team && team->getPrototype() ? team->getPrototype()->templateScripts().initialIdleFrames : 0; // prototype + 0x1F8
		bool minimum = true, inProduction = false, all = true;
		int total = 0;
		for (const WorkOrder &o : t.orders)
		{
			const int counted = o.numCompleted + (o.factory != INVALID_ID ? 1 : 0); // RW 0x8F5040
			if (o.required && o.numRequired > counted)
			{
				minimum = false;
			}
			inProduction = inProduction || o.factory != INVALID_ID; // RW 0x8F5067
			if (!o.optional && o.numRequired > o.numCompleted)        // RW 0x8F4FF0
			{
				all = false;
			}
			total += o.numCompleted;
		}
		all = all && total > 0;
		const bool expired = timeout >= 1 && now > t.frameStarted + (UnsignedInt)timeout; // RW 0x8F501B
		if (!team || (expired && !minimum))
		{
			// RW 0x7A12F4 : les membres vont à l'équipe par défaut
			if (team)
			{
				if (Team *def = p.getDefaultTeam(); def && def != team)
				{
					while (Object *o = team->getFirstMember())
					{
						o->setTeam(def);
					}
				}
				team->scriptState().beingBuilt = false;
			}
			endTeam(t, false);
			ai.buildQueue.erase(ai.buildQueue.begin() + (std::ptrdiff_t)i);
			continue;
		}
		if ((expired && !inProduction) || all)
		{
			team->scriptState().beingBuilt = false; // RW 0x79FE29(0)
			endTeam(t, true);
			TeamInQueue moved = std::move(t);
			ai.buildQueue.erase(ai.buildQueue.begin() + (std::ptrdiff_t)i);
			ai.readyQueue.insert(ai.readyQueue.begin(), std::move(moved)); // RW 0x8F3DD3 : en tête
			continue;
		}
		++i;
	}
}

// RW 0x8F9487
void AIPlayerTeams::processTeamBuilding(PlayerAI &ai, Player &p)
{
	TeamFactory &teams = m_logic.players().teams();
	for (TeamInQueue &t : ai.buildQueue)
	{
		Team *team = teams.findTeamByID(t.team);
		if (!team || !team->getPrototype())
		{
			continue;
		}
		Coord3D home = ai.baseCenter;
		const bool hasHome = homeLocation(m_logic, *team->getPrototype(), home);
		const float radius = hasHome ? m_logic.settings().maxRecruitRadius : 100000.0f; // AIData + 0x5C, RW 0xBDCF20
		for (WorkOrder &o : t.orders)
		{
			// INFERENCE (RW 0x8F7D42 non lu) : une usine morte ou vidée sans avoir livré libère l'ordre
			if (o.factory != INVALID_ID)
			{
				Object *f = m_logic.findObjectByID(o.factory);
				ProductionUpdateInterface *fpu = f && !f->isEffectivelyDead() ? f->getProductionUpdate() : nullptr;
				if (!fpu || fpu->getProductionCount() == 0)
				{
					o.factory = INVALID_ID;
				}
			}
			while (o.thing && o.numCompleted < o.numRequired)
			{
				Object *unit = tryToRecruit(*team, *o.thing, home, radius); // RW 0x7A41D4
				if (!unit)
				{
					break;
				}
				++o.numCompleted;
				unit->setTeam(team); // RW 0x69954A
				team->setActive();   // RW 0x8F9608
			}
			if (o.thing && o.numCompleted < o.numRequired && !o.optional && o.factory == INVALID_ID)
			{
				startTraining(p, o); // vslot 0x60
			}
		}
	}
}

// RW 0x7A41D4 (ZH Team::tryToRecruit) : l'objet de ce type du joueur, hors de cette équipe, dans l'équipe par défaut ou une équipe active qui se
// laisse recruter (teamIsAIRecruitable), vivant et doté d'une IA, le plus proche de `pos` dans le rayon
Object *AIPlayerTeams::tryToRecruit(Team &team, const ThingTemplate &thing, const Coord3D &pos, float radius)
{
	Player *owner = team.getControllingPlayer();
	Object *best = nullptr;
	float bestDist = SimMath::mulf32(radius, radius);
	for (Object *o = m_logic.getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getControllingPlayer() != owner || o->getTeam() == &team || o->isEffectivelyDead() || !o->getAIUpdateInterface())
		{
			continue;
		}
		if (!BuildAssistant::isEquivalentTo(o->getTemplate(), &thing))
		{
			continue;
		}
		Team *from = o->getTeam();
		const bool isDefault = owner && from == owner->getDefaultTeam();
		if (!isDefault && !(from && from->isActive() && from->getPrototype() && from->getPrototype()->getDict().getBool("teamIsAIRecruitable")))
		{
			continue;
		}
		const float dx = SimMath::subf32(o->getPosition()->x, pos.x), dy = SimMath::subf32(o->getPosition()->y, pos.y);
		const float d = SimMath::addf32(SimMath::mulf32(dx, dx), SimMath::mulf32(dy, dy));
		if (d > bestDist)
		{
			continue;
		}
		bestDist = d;
		best = o;
	}
	return best;
}

// RW 0x8F5347 : une usine du joueur qui offre ce type ; `busyOK` faux : ni désactivée ni occupée
Object *AIPlayerTeams::findFactory(Player &p, const ThingTemplate &thing, bool busyOK)
{
	for (Object *o = m_logic.getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getControllingPlayer() != &p || o->isEffectivelyDead())
		{
			continue;
		}
		ProductionUpdateInterface *pu = o->getProductionUpdate();
		if (!pu || !BuildAssistant::isInProducersCommandSet(*o, &thing, -1))
		{
			continue;
		}
		if (!busyOK && (pu->isFactoryDisabled() || pu->getProductionCount() != 0))
		{
			continue;
		}
		return o;
	}
	return nullptr;
}

// RW 0x8F69E8
bool AIPlayerTeams::startTraining(Player &p, WorkOrder &order)
{
	Object *factory = findFactory(p, *order.thing, false);
	ProductionUpdateInterface *pu = factory ? factory->getProductionUpdate() : nullptr;
	if (!pu || pu->canQueueCreateUnit() != 0) // vslot 0x44
	{
		return false;
	}
	const ProductionID id = pu->requestUniqueUnitID(); // vslot 8
	if (!pu->queueCreateUnit(order.thing->getFinalOverride(), -1, id, -1, false, std::string(), false)) // vslot 0x20
	{
		return false;
	}
	order.factory = factory->getID();
	return true;
}

// RW 0x8F54BA
bool AIPlayerTeams::isPossibleToBuildTeam(Player &p, TeamPrototype &proto, bool requireIdleFactory, bool &needMoney)
{
	needMoney = false;
	bool anyIdle = false;
	int cost = 0;
	for (const UnitEntry &e : unitEntries(m_logic, proto))
	{
		if (!e.thing)
		{
			continue;
		}
		if (!findFactory(p, *e.thing, true))
		{
			return false;
		}
		anyIdle = anyIdle || findFactory(p, *e.thing, false) != nullptr;
		const int price = BuildAssistant::calcCostToBuild(*e.thing, &p, nullptr, -1); // RW 0x73C25F
		cost = (int)SimMath::addf32(SimMath::mulf32(SimMath::mulf32((float)(e.minCount + e.maxCount), 0.5f), (float)price), (float)cost);
	}
	const int needed = (int)SimMath::mulf32((float)cost, m_logic.settings().teamResourcesToStart); // AIData + 0x28
	if ((int)p.getMoney()->countMoney() < needed)
	{
		needMoney = true;
		return false;
	}
	return anyIdle || !requireIdleFactory;
}

// RW 0x8F5D5D : chaque entrée trouve au moins son minimum d'unités à recruter
bool AIPlayerTeams::isPossibleToRecruitTeam(Player &p, TeamPrototype &proto)
{
	const std::vector<UnitEntry> entries = unitEntries(m_logic, proto);
	if (entries.empty())
	{
		return false;
	}
	for (const UnitEntry &e : entries)
	{
		if (!e.thing || e.minCount <= 0)
		{
			continue;
		}
		int count = 0;
		for (Object *o = m_logic.getFirstObject(); o && count < e.minCount; o = o->getNextObject())
		{
			if (o->getControllingPlayer() != &p || o->isEffectivelyDead() || !o->getAIUpdateInterface() || !BuildAssistant::isEquivalentTo(o->getTemplate(), e.thing))
			{
				continue;
			}
			Team *from = o->getTeam();
			if (from == p.getDefaultTeam() || (from && from->isActive() && from->getPrototype() && from->getPrototype()->getDict().getBool("teamIsAIRecruitable")))
			{
				++count;
			}
		}
		if (count < e.minCount)
		{
			return false;
		}
	}
	return true;
}

// RW 0x8F417A (AISkirmishPlayer)
bool AIPlayerTeams::isAGoodIdeaToBuildTeam(PlayerAI &ai, Player &p, TeamPrototype &proto)
{
	const std::string condition = proto.getDict().getAsciiString("teamProductionCondition");
	if (condition.empty() || !m_logic.scriptEngine().evaluateProductionCondition(p, condition)) // RW 0x7A3C49
	{
		return false;
	}
	if ((int)proto.teams().size() >= proto.getDict().getInt("teamMaxInstances")) // RW 0x7A00B0 >= + 0x218
	{
		return false;
	}
	for (const TeamInQueue &t : ai.buildQueue)
	{
		Team *team = m_logic.players().teams().findTeamByID(t.team);
		if (team && team->getPrototype() == &proto)
		{
			return false;
		}
	}
	bool needMoney = false;
	return isPossibleToRecruitTeam(p, proto) || isPossibleToBuildTeam(p, proto, true, needMoney);
}

// RW 0x8F805D
bool AIPlayerTeams::selectTeamToBuild(PlayerAI &ai, Player &p)
{
	int hi = kPriorityNone;
	std::vector<TeamPrototype *> good;
	for (const std::unique_ptr<TeamPrototype> &proto : m_logic.players().teams().prototypes())
	{
		if (proto->getControllingPlayer() != &p || !isAGoodIdeaToBuildTeam(ai, p, *proto))
		{
			continue;
		}
		good.push_back(proto.get());
		hi = std::max(hi, priority(*proto));
	}
	if (hi == kPriorityNone)
	{
		return false;
	}
	std::vector<TeamPrototype *> best;
	for (TeamPrototype *proto : good)
	{
		if (priority(*proto) == hi)
		{
			best.push_back(proto);
		}
	}
	const int pick = m_logic.random().getValue(0, (int)best.size() - 1, "AIPlayer.cpp", 0x737);
	if (pick < 0 || pick >= (int)best.size())
	{
		return false;
	}
	buildSpecificAITeam(ai, p, *best[(size_t)pick], false); // vslot 0x24
	ai.readyToBuildTeam = false;
	int timer = (int)m_logic.settings().teamSeconds * (int)LOGICFRAMES_PER_SECOND; // + 0x20 x 5
	const int money = (int)p.getMoney()->countMoney();
	if (money < m_logic.settings().aiPoor)
	{
		timer = (int)SimMath::divf32((float)timer, m_logic.settings().teamsPoorRate);
	}
	else if (money > m_logic.settings().aiWealthy)
	{
		timer = (int)SimMath::divf32((float)timer, m_logic.settings().teamsWealthyRate);
	}
	ai.teamTimer = timer;
	return true;
}

// RW 0x8F6DE9
void AIPlayerTeams::buildSpecificAITeam(PlayerAI &ai, Player &p, TeamPrototype &proto, bool priorityBuild)
{
	bool needMoney = false;
	if (!isPossibleToRecruitTeam(p, proto) && !isPossibleToBuildTeam(p, proto, false, needMoney) && !needMoney)
	{
		return; // RW 0x8F7068 : les usines ou la technologie manquent
	}
	// deux passes, chaque ordre mis en tête de liste : les facultatifs (max - min) puis les requis (min)
	std::vector<WorkOrder> orders;
	const std::vector<UnitEntry> entries = unitEntries(m_logic, proto);
	for (const UnitEntry &e : entries)
	{
		if (e.thing && e.maxCount - e.minCount > 0)
		{
			WorkOrder o;
			o.thing = e.thing;
			o.numRequired = e.maxCount - e.minCount;
			o.optional = true;
			orders.insert(orders.begin(), o);
		}
	}
	for (const UnitEntry &e : entries)
	{
		if (e.thing && e.minCount > 0)
		{
			WorkOrder o;
			o.thing = e.thing;
			o.numRequired = e.minCount;
			o.required = true;
			orders.insert(orders.begin(), o);
		}
	}
	if (orders.empty())
	{
		return; // RW 0x8F71B0 : aucune unité à bâtir
	}
	Team *team = m_logic.players().teams().createTeam(&proto, false); // RW 0x7A6E8E
	if (!team)
	{
		return;
	}
	team->scriptState().beingBuilt = true; // RW 0x79FE29(1)
	team->scriptState().wasBuilt = true;
	TeamInQueue t;
	t.orders = std::move(orders);
	t.team = team->getID();
	t.frameStarted = m_logic.getFrame();
	t.priorityBuild = priorityBuild;
	if (priorityBuild)
	{
		ai.buildQueue.insert(ai.buildQueue.begin(), std::move(t));
	}
	else
	{
		ai.buildQueue.push_back(std::move(t));
	}
}

void AIPlayerTeams::onUnitProduced(Object &factory, Object &unit)
{
	if (!m_enabled)
	{
		return;
	}
	for (PlayerAI &ai : m_players)
	{
		for (TeamInQueue &t : ai.buildQueue)
		{
			for (WorkOrder &o : t.orders)
			{
				if (o.factory != factory.getID() || !BuildAssistant::isEquivalentTo(unit.getTemplate(), o.thing))
				{
					continue;
				}
				o.factory = INVALID_ID;
				++o.numCompleted;
				if (Team *team = m_logic.players().teams().findTeamByID(t.team))
				{
					unit.setTeam(team);
				}
				return;
			}
		}
	}
}

AIPlayerTeams::PlayerAI *AIPlayerTeams::find(const Player &p)
{
	for (PlayerAI &ai : m_players)
	{
		if (m_logic.players().getNthPlayer(ai.playerIndex) == &p)
		{
			return &ai;
		}
	}
	return nullptr;
}

// RW 0x8F47EB
Player *AIPlayerTeams::currentEnemy(Player &p)
{
	PlayerAI *ai = m_enabled ? find(p) : nullptr;
	if (!ai)
	{
		return nullptr;
	}
	const UnsignedInt now = m_logic.getFrame();
	if (now >= ai->nextEnemyFrame)
	{
		ai->nextEnemyFrame = now + 5u * LOGICFRAMES_PER_SECOND;
		acquireEnemy(*ai, p);
	}
	return m_logic.players().getNthPlayer(ai->enemyIndex);
}

namespace
{
// RW 0x6ABD44 / 0x6ABE2C (INFERENCE : un objet vivant à lui) et RW 0x6ABDF7 (un prototype d'équipe avec des objets)
bool hasLivingObjects(GameLogic &logic, const Player &p)
{
	for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getControllingPlayer() == &p && !o->isEffectivelyDead())
		{
			return true;
		}
	}
	return false;
}
} // namespace

// RW 0x8F457D
void AIPlayerTeams::acquireEnemy(PlayerAI &ai, Player &p)
{
	PlayerList &players = m_logic.players();
	Player *current = players.getNthPlayer(ai.enemyIndex);
	if (current && !current->isDefeated() && hasLivingObjects(m_logic, *current))
	{
		return;
	}
	static const int kStructure = ObjectTemplateInfoBuilder::kindOfIndex("STRUCTURE");
	int best = -1;
	float bestDist = 999999995904.0f; // RW 0xC1C4AC
	for (int i = 0; i < players.getPlayerCount(); ++i)
	{
		Player *e = players.getNthPlayer(i);
		if (!e || e == &p || !e->getDefaultTeam() || p.getRelationship(e->getDefaultTeam()) != ENEMIES || e->getPlayerName() == "PlyrCreeps")
		{
			continue;
		}
		const bool alive = !e->isDefeated() && hasLivingObjects(m_logic, *e);
		if (!alive && !hasLivingObjects(m_logic, *e))
		{
			continue; // RW 0x6ABDF7
		}
		// RW 0x8F6511 : le rectangle des structures du joueur
		bool first = true;
		float lox = 0.0f, loy = 0.0f, hix = 0.0f, hiy = 0.0f;
		for (Object *o = m_logic.getFirstObject(); o; o = o->getNextObject())
		{
			if (o->getControllingPlayer() != e || kStructure < 0 || !o->isKindOf((unsigned)kStructure))
			{
				continue;
			}
			const Coord3D *pos = o->getPosition();
			if (first)
			{
				lox = hix = pos->x;
				loy = hiy = pos->y;
				first = false;
			}
			lox = std::min(lox, pos->x);
			loy = std::min(loy, pos->y);
			hix = std::max(hix, pos->x);
			hiy = std::max(hiy, pos->y);
		}
		const float cx = SimMath::subf32(SimMath::addf32(SimMath::mulf32(SimMath::subf32(hix, lox), 0.5f), lox), ai.baseCenter.x);
		const float cy = SimMath::subf32(SimMath::addf32(SimMath::mulf32(SimMath::subf32(hiy, loy), 0.5f), loy), ai.baseCenter.y);
		float dist = SimMath::addf32(SimMath::mulf32(cy, cy), SimMath::mulf32(cx, cx));
		if (!alive)
		{
			dist = 499999997952.0f; // RW 0xC798D4
		}
		for (const PlayerAI &other : m_players)
		{
			if (&other == &ai)
			{
				continue;
			}
			if (other.enemyIndex == i)
			{
				dist = SimMath::addf32(dist, 250000.0f); // RW 0xC798D0
			}
			if (players.getNthPlayer(other.enemyIndex) == &p)
			{
				dist = std::max(0.0f, SimMath::subf32(dist, 625.0f)); // RW 0xC798CC
			}
		}
		if (dist < bestDist)
		{
			bestDist = dist;
			best = i;
		}
	}
	if (best >= 0)
	{
		ai.enemyIndex = best;
	}
}
