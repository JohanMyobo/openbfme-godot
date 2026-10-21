// Notre version : Gollum porteur de l'Anneau (NeutralGollum_RingHero, AnimalAIUpdate) obéit à un ordre de déplacement. GPL-3.0.
//
// L'errance d'AnimalAIUpdate (AmbientAIUpdate, PR #68) n'agit que lorsque Gollum est inactif : il atteint le point, puis repart errer. Passe (SKIP) sans les installations.

#include "Siege1TestUtil.h"

#include <algorithm>

using namespace siege1test;

TEST_CASE("notre version : Gollum porteur de l'Anneau marche jusqu'au point qu'on lui donne")
{
	OPENBFME_REQUIRE_START(s);
	FortressGame g(*s, "maps/map wor minas tirith/map wor minas tirith.map");
	g.frames(2);
	Player *me = g.human();
	REQUIRE(me);
	const Waypoint *w = g.game->logic().terrain()->findWaypointByName("Player_2_Start");
	REQUIRE(w);
	std::string error;
	const Coord3D start{ w->location.x, w->location.y, 0.0f };
	Object *gollum = g.game->createObject("NeutralGollum_RingHero", me->getPlayerIndex(), start, 0.0f, &error);
	REQUIRE_MESSAGE(gollum, error);
	g.frames(3);
	AIUpdateInterface *ai = gollum->getAIUpdateInterface();
	REQUIRE(ai);
	const Coord3D dest{ start.x + 250.0f, start.y, 0.0f };
	ai->aiMoveToPosition(dest, CMD_FROM_PLAYER);
	const ObjectID id = gollum->getID();
	float closest = 1e9f;
	for (int f = 0; f < 120; ++f)
	{
		g.frames(1);
		const Object *o = g.game->logic().findObjectByID(id);
		REQUIRE(o);
		const float ex = dest.x - o->getPosition()->x, ey = dest.y - o->getPosition()->y;
		closest = std::min(closest, std::sqrt(ex * ex + ey * ey));
		if (f % 10 == 0)
		{
			MESSAGE("frame " << f << " : x " << o->getPosition()->x - start.x << " y " << o->getPosition()->y - start.y << " idle " << o->getAIUpdateInterface()->isIdle());
		}
	}
	// il atteint le point, puis l'errance d'AnimalAIUpdate (WanderPercentage 80) le fait repartir : on vérifie le passage au point, pas la position finale
	CHECK(closest < 20.0f); // tolerance: le rayon d'arrivée de l'IA et les cases de 10 du pathfinder, pas une position exacte
}
