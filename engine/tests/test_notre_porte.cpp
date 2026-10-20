// Notre version : une porte fermée arrête les flèches (ProjectileModules.cpp, closedGateCrossed). GPL-3.0.
//
// Sur Minas Tirith, un archer du Gondor se tient d'un côté de la porte, un ennemi de l'autre. Porte fermée : la cible ne perd aucun point de vie ; porte
// ouverte : le même tir la touche (le test prouve que l'archer tire bien). Passe (SKIP) sans les installations.

#include "Siege1TestUtil.h"

#include "GameLogic/Module/GateModules.h"

using namespace siege1test;

namespace
{
struct GateDuel
{
	float healthBefore = 0.0f, healthAfter = 0.0f;
	bool ordered = false;
	int state = -1;
};

GateDuel duel(starttest::Shared &s, bool open)
{
	GateDuel r;
	FortressGame g(s, "maps/map wor minas tirith/map wor minas tirith.map");
	g.frames(2);
	Object *gate = g.named("Gate");
	REQUIRE(gate);
	GateOpenAndCloseBehavior *b = GateOpenAndCloseBehavior::findGate(*gate);
	REQUIRE(b);
	const GateOpenAndCloseBehavior::State want = open ? GateOpenAndCloseBehavior::OPEN : GateOpenAndCloseBehavior::CLOSED;
	if (b->state() != want)
	{
		open ? b->open() : b->close();
		g.frames(80);
	}
	r.state = (int)b->state();
	REQUIRE(b->state() == want);
	Player *defender = gate->getControllingPlayer();
	Player *attacker = g.human();
	REQUIRE(defender);
	REQUIRE(attacker);
	g.game->shroud().revealMapForPlayerPermanently(defender->getPlayerIndex());
	g.game->shroud().revealMapForPlayerPermanently(attacker->getPlayerIndex());
	// l'axe X de la porte (première colonne de la base) traverse son épaisseur
	const float *basis = gate->getBasis();
	const Coord3D at = *gate->getPosition();
	const float ax = basis[0], ay = basis[3];
	std::string error;
	Object *archer = g.game->createObject("GondorArcher", defender->getPlayerIndex(), Coord3D{ at.x - ax * 90.0f, at.y - ay * 90.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(archer, error);
	Object *victim = g.game->createObject("MordorFighter", attacker->getPlayerIndex(), Coord3D{ at.x + ax * 90.0f, at.y + ay * 90.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(victim, error);
	g.frames(3);
	r.healthBefore = healthOf(victim);
	AIUpdateInterface *ai = archer->getAIUpdateInterface();
	r.ordered = ai && ai->aiAttackObject(victim, CMD_FROM_PLAYER);
	const ObjectID vid = victim->getID();
	g.frames(150);
	Object *v = g.game->logic().findObjectByID(vid);
	r.healthAfter = v ? healthOf(v) : 0.0f;
	MESSAGE("porte " << (open ? "ouverte" : "fermée") << " : vie " << r.healthBefore << " -> " << r.healthAfter);
	return r;
}
} // namespace

TEST_CASE("notre version : la porte fermée de Minas Tirith arrête les flèches, la porte ouverte les laisse passer")
{
	OPENBFME_REQUIRE_START(s);
	const GateDuel closed = duel(*s, false);
	CHECK(closed.ordered);
	CHECK(closed.healthAfter == closed.healthBefore);
	const GateDuel opened = duel(*s, true);
	CHECK(opened.ordered);
	CHECK(opened.healthAfter < opened.healthBefore); // tolerance: la porte ouverte prouve seulement que l'archer touche ; le montant des dégâts n'est pas le sujet
}
