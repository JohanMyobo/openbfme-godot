// OpenBFME. GPL-3.0.
//
// La campagne par actes de BFME1 (GameLogic/LivingWorld/ActCampaign.h) : lecture des blocs et déroulé.

#include "doctest.h"

#include "IniTestUtil.h"
#include "RetailTestMount.h"

#include "GameEngineDevice/Win32Device/Common/Win32BIGFileSystem.h"
#include "GameLogic/LivingWorld/ActCampaign.h"
#include "GameClient/MapUtil.h"
#include "GameLogic/NewGame/StartingBase.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <optional>

using namespace LivingWorld;

namespace
{
const char *const kCampaign = R"(
LivingWorldCampaign TestCampaign
	IsEvilCampaign = No
	Act One
		WorldText
			DelayFromActStart = 2.0
			StringTag = LW:Texte
		End
		AudioEvent
			DelayFromActStart = 1.0
			Sound = UnSon
		End
		SplineCamera
			DelayFromActStart = 0.0
			ControlPoint
				Position = X:10 Y:20 Z:0
				Angle = 30
				EaseOut = 1.0
				Time = 4.0
			End
		End
		SpawnArmy
			Name = Heros
			PlayerArmy = HerosArmy
			Icon = Hero
			PlayerOwned = Yes
			Position = X:0 Y:0
		End
		ToggleArmyControl
			Name = Heros
			PlayerControl = Yes
		End
		EnableRegion
			Region = Moria
		End
	End
	Act Two
		DespawnArmy = Heros
		SpawnArmy
			Name = Marcheur
			PlayerArmy = HerosArmy
			Icon = Hero
			Position = X:0 Y:0
		End
		MoveArmy
			Name = Marcheur
			MoveTo = X:30 Y:40
			MoveSpeed = 150.0
			PlayNextActAfterMove = Yes
		End
	End
	Act Three
		EnableRegion
			Region = Rohan
		End
		EndAct = Yes
	End
	Act Never
		EnableRegion
			Region = Mordor
		End
	End
End
LivingWorldRegionCampaign TestCampaign
	RegionObject = LMR_Fill
	ZOffset = 155
	RegionPopupDefaultColor = R:255 G:33 B:23 A:128
	EnemyBordersEffect
		Geometry LMR_Brdr
		ControlPoint
			Position X:0.25 Y:0.0 Z:0.0
			Time 0.0
		End
	End
	Region Moria
		MapName = "MAP GOOD Moria"
		MovieNameFirstTime = Premier
		MovieNameRepeat = Encore
		SubObject = T35
		ArmyBonus = BONUS_ARMEE
		LegendaryBonus = 1
		MissionObjectiveTag = SCRIPT:Un
		MissionObjectiveTag = SCRIPT:Deux
		BonusMissionObjectiveTag = BONUSROUND:Trois
		ConnectsTo = Rohan Mordor
		SkirmishOpponent
			Name = SansActe
			Faction = FactionMordor
			StartPosition = 2
			TeamID = 1
		End
		SkirmishOpponent
			Name = ActeDeux
			Faction = FactionIsengard
			StartPosition = 3
			TeamID = 4
			AllowInActs = Two Three
		End
		SkirmishOpponent
			Name = SkirmishRohan
			Faction = FactionRohan
			StartPosition = 1
			TeamID = 5
			IsPlayer = Yes
		End
	End
	Region Rohan
		MapName = "MAP GOOD Rohan"
	End
	Region Mordor
		MapName = "MAP GOOD Mordor"
	End
End
LivingWorldPlayerArmy
	Name = HerosArmy
	DisplayNameTag = LWA:Heros
	Color = R:255 G:0 B:0
	ArmyEntry
		ThingTemplate = GondorAragorn
		Quantity = 1
	End
End
)";

const char *const kMap = R"(
LivingWorldMapInfo
	MapObject = LivingMap
	TextLayerSubObject = "text plane"
	Center = X:830 Y:432
	Extent = X:2406 Y:1809
	GondorBanner = GU_Banr_A
	EyeTower
		PupilAnimObject = EyeTower_Pupil
	End
End
LivingWorldArmyIcon Hero
	OnSelectedSound = Choisi
	Object Soldiers
		Model = ArmyAntsM_GM01
		ZOffset = 15
		Scale = 0.8
		Clickable = Yes
	End
End
LivingWorldSound Wind
	Sound = Vent
End
)";

initest::FileList testFiles()
{
	return {
		{ "data\\ini\\gamedata.ini", "#define BONUS_ARMEE 40\n" },
		{ "data\\ini\\livingworld.ini", kMap },
		{ "data\\ini\\campaigns\\gondorcampaign.ini", kCampaign },
		{ "data\\ini\\campaigns\\mordorcampaign.ini", "" },
		{ "data\\ini\\campaigns\\democampaigns.ini", "" },
	};
}
} // namespace

TEST_CASE("act campaign: every field of the campaign, region and army blocks reads")
{
	initest::Fixture fx(testFiles());
	ActCampaignStores stores;
	std::string error;
	REQUIRE_MESSAGE(stores.load(fx.fsys, &error), error);
	REQUIRE(stores.campaigns.size() == 1);
	const ActCampaignData &c = stores.campaigns[0];
	CHECK(c.acts.size() == 4);
	CHECK(c.lastAct == 2); // le premier EndAct, pas le dernier acte (0x7A73C0)
	CHECK(c.acts[0].worldTexts[0].delayMs == 2000); // des secondes rangées en millisecondes (0xC52D90)
	CHECK(c.acts[0].splineCameras[0].points[0].timeMs == 4000);
	CHECK(c.acts[0].splineCameras[0].points[0].easeOut == 1.0f);
	CHECK(c.acts[0].splineCameras[0].points[0].easeIn == 0.5f);
	const ActRegionCampaign *rc = stores.findRegionCampaign("TestCampaign");
	REQUIRE(rc != nullptr);
	CHECK(rc->regionObject == "LMR_Fill");
	CHECK(rc->effects.at("EnemyBordersEffect").geometry == std::vector<std::string>{ "LMR_Brdr" });
	const ActRegion *moria = rc->findRegion("Moria");
	REQUIRE(moria != nullptr);
	CHECK(moria->mapName == "MAP GOOD Moria");
	CHECK(moria->connectsTo == std::vector<std::string>{ "Rohan", "Mordor" });
	CHECK(moria->missionObjectiveTags.size() == 2);
	CHECK(moria->opponents[1].allowInActs == std::vector<std::string>{ "Two", "Three" });
	CHECK(stores.findPlayerArmy("HerosArmy")->entries[0].thingTemplate == "GondorAragorn");
	CHECK(stores.findArmyIcon("Hero")->objects[0].model == "ArmyAntsM_GM01");
	CHECK(stores.mapInfo.mapObject == "LivingMap");
	CHECK(stores.mapInfo.textLayerSubObject == "text plane");
	CHECK(stores.mapInfo.notRead.count("GondorBanner") == 1);
	CHECK(stores.mapInfo.notRead.count("EyeTower.PupilAnimObject") == 1);
}

TEST_CASE("act campaign: a missing file or an unknown field is an error")
{
	{
		initest::Fixture fx(initest::FileList{ { "data\\ini\\gamedata.ini", "" }, { "data\\ini\\livingworld.ini", kMap } });
		ActCampaignStores stores;
		std::string error;
		CHECK_FALSE(stores.load(fx.fsys, &error));
		CHECK(error.find("GondorCampaign") != std::string::npos);
	}
	{
		initest::FileList files = testFiles();
		files[2].second = "LivingWorldCampaign X\n\tChampInconnu = 1\nEnd\n";
		initest::Fixture fx(files);
		ActCampaignStores stores;
		std::string error;
		CHECK_FALSE(stores.load(fx.fsys, &error));
	}
}

TEST_CASE("act campaign: the first act runs its timeline and a won region plays the next act")
{
	initest::Fixture fx(testFiles());
	ActCampaignStores stores;
	std::string error;
	REQUIRE(stores.load(fx.fsys, &error));
	ActCampaignLogic logic;
	REQUIRE(logic.start(stores, "TestCampaign", &error));
	CHECK(logic.actName() == "One");
	CHECK(logic.enabledRegions() == std::vector<std::string>{ "Moria" });
	REQUIRE(logic.armies().size() == 1);
	CHECK(logic.armies()[0].playerControlled); // ToggleArmyControl après SpawnArmy (0x7AEE70)

	std::vector<std::pair<int, std::string>> seen;
	for (int i = 0; i < 90; ++i)
	{
		logic.update();
		for (const ActCommand &c : logic.takeCommands())
		{
			seen.push_back({ logic.frame(), c.type == ActCommand::SPLINE_CAMERA ? std::string("spline") : c.text });
		}
	}
	const std::vector<std::pair<int, std::string>> expected = { { 1, "spline" }, { 30, "UnSon" }, { 60, "LW:Texte" } };
	CHECK(seen == expected);

	CHECK(logic.canEnterRegion("Moria"));
	CHECK_FALSE(logic.canEnterRegion("Rohan"));
	CHECK(logic.enterRegion("Moria") == "Premier");
	CHECK(logic.enterRegion("Moria") == "Encore");
	// 0xA1B500 : à l'acte One, aucun adversaire n'est permis (0x7B9100 : une liste vide ne contient rien) : le premier non joueur prend l'emplacement 1 (0xA1A0F0)
	{
		const std::vector<LivingWorld::ActSkirmishSlot> slots = logic.skirmishSlotsOf("Moria");
		REQUIRE(slots.size() == 2);
		CHECK(slots[0].slot == 0);
		CHECK(slots[0].human);
		CHECK(slots[0].name == "PlayerHuman"); // pas le Name du fichier, SkirmishRohan (0xA19CA8)
		CHECK(slots[0].faction == "FactionRohan");
		CHECK(slots[0].startPosition == 1);
		CHECK(slots[0].team == 5);
		CHECK(slots[1].slot == 1);
		CHECK_FALSE(slots[1].human);
		CHECK(slots[1].name == "SansActe");
		CHECK(slots[1].startPosition == 2);
		CHECK(slots[1].team == 1);
	}
	CHECK(logic.skirmishSlotsOf("Rohan").empty()); // sans adversaire : une mission ordinaire (0xA1B557)

	CHECK_FALSE(logic.regionWon("Rohan"));
	REQUIRE(logic.regionWon("Moria"));
	CHECK(logic.armyPoints() == 40);
	CHECK(logic.legendaryPoints() == 1);
	CHECK(logic.enabledRegions().empty());
	CHECK(logic.conqueredRegions().count("Moria") == 1);
	CHECK(logic.actName() == "Two");
	{
		// à l'acte Two, l'adversaire permis ; le premier non joueur n'est plus repris
		const std::vector<LivingWorld::ActSkirmishSlot> slots = logic.skirmishSlotsOf("Moria");
		REQUIRE(slots.size() == 2);
		CHECK(slots[0].human);
		CHECK(slots[1].slot == 1);
		CHECK(slots[1].name == "ActeDeux");
		CHECK(slots[1].faction == "FactionIsengard");
		CHECK(slots[1].team == 4);
	}
	CHECK(logic.findArmy("Heros") == nullptr);

	// 0x7A4E30 : MoveSpeed 150 * 1/6 = 25 unités par image ; 50 unités : la marche finit à la deuxième image et joue l'acte suivant (0x7A51D0)
	logic.update();
	CHECK(logic.actName() == "Two");
	CHECK(logic.findArmy("Marcheur")->moving);
	logic.update();
	CHECK(logic.actName() == "Three");
	CHECK_FALSE(logic.findArmy("Marcheur")->moving);
	CHECK_FALSE(logic.findArmy("Marcheur")->playNextAct);
	CHECK(logic.enabledRegions() == std::vector<std::string>{ "Rohan" });

	// EndAct : l'acte Never ne se joue jamais (0x7A7320)
	logic.stepAct();
	CHECK(logic.actName() == "Three");
	CHECK(logic.errors().empty());
}

TEST_CASE("act campaign: notre version, BFME1: the direction of an army names its walk-on points (0x4300A0, 0x7601A0)")
{
	CHECK(StartingBase::walkOnAxisName(0.0f, 1.0f) == "TopArmy");
	CHECK(StartingBase::walkOnAxisName(0.0f, -1.0f) == "BottomArmy");
	CHECK(StartingBase::walkOnAxisName(2.0f, 1.0f) == "LeftArmy");
	CHECK(StartingBase::walkOnAxisName(-2.0f, 1.0f) == "RightArmy");
	CHECK(StartingBase::walkOnAxisName(0.0f, 0.0f) == "BottomArmy"); // |x| = |y|, y pas > 0
	CHECK(StartingBase::walkOnAxisName(1.0f, 1.0f) == "TopArmy");    // |x| = |y| : l'axe y
}

TEST_CASE("act campaign retail: notre version, BFME1: the Fellowship walks into Moria, a forced battle's UseArmy wins")
{
	const char *install = std::getenv("BFME1_INSTALL");
	if (!install)
	{
		retailtest::printSkip("BFME1 campaigns (BFME1_INSTALL unset)");
		return;
	}
	Win32BIGFileSystem fs;
	std::string error;
	auto archive = fs.openArchiveFile((std::filesystem::u8path(install) / "INI.big").u8string(), &error);
	REQUIRE_MESSAGE(archive, error);
	fs.mountArchive(std::move(archive), "INI.big", false);
	ActCampaignStores stores;
	REQUIRE_MESSAGE(stores.load(fs, &error), error);
	ActCampaignLogic logic;
	REQUIRE_MESSAGE(logic.start(stores, "GondorCampaign", &error), error);
	const std::optional<ActWalkOnArmy> moria = logic.walkOnArmyOf("Moria");
	REQUIRE(moria.has_value());
	CHECK(moria->name == "Fellowship");
	CHECK(moria->playerArmy == "FellowshipPlayerArmy");
	CHECK(moria->entries.size() == 9);
	CHECK(std::any_of(moria->entries.begin(), moria->entries.end(), [](const ActArmyEntry &e) { return e.thingTemplate == "GondorAragorn" && e.quantity == 1; }));
	CHECK_FALSE(logic.walkOnArmyOf("NoSuchRegion").has_value());
	CHECK(logic.errors().empty());
}

TEST_CASE("act campaign retail: Gondor and Mordor read whole, act one enables Moria and winning it plays act two")
{
	const char *install = std::getenv("BFME1_INSTALL");
	if (!install || !*install)
	{
		retailtest::printSkip("BFME1 campaigns (BFME1_INSTALL unset)");
		return;
	}
	Win32BIGFileSystem fs;
	std::string error;
	auto archive = fs.openArchiveFile((std::filesystem::u8path(install) / "INI.big").u8string(), &error);
	REQUIRE_MESSAGE(archive, error);
	fs.mountArchive(std::move(archive), "INI.big", false);
	ActCampaignStores stores;
	REQUIRE_MESSAGE(stores.load(fs, &error), error);
	const ActCampaignData *gondor = stores.findCampaign("GondorCampaign");
	REQUIRE(gondor != nullptr);
	CHECK(gondor->acts.size() == 46);
	CHECK_FALSE(gondor->isEvil);
	const ActCampaignData *mordor = stores.findCampaign("MordorCampaign");
	REQUIRE(mordor != nullptr);
	CHECK(mordor->acts.size() == 45);
	CHECK(mordor->isEvil);
	CHECK(stores.findRegionCampaign("GondorCampaign")->regions.size() == 38);
	CHECK(stores.findRegionCampaign("MordorCampaign")->regions.size() == 37);
	CHECK(stores.mapInfo.mapObject == "LivingMap");

	ActCampaignLogic logic;
	REQUIRE(logic.start(stores, "GondorCampaign", &error));
	CHECK(logic.actName() == "One");
	CHECK(logic.enabledRegions() == std::vector<std::string>{ "Moria" });
	CHECK(logic.findArmy("Fellowship")->playerControlled);
	CHECK(logic.errors().empty());
	CHECK(stores.findRegionCampaign("GondorCampaign")->findRegion("Moria")->mapName == "MAP GOOD Moria");
	REQUIRE(logic.regionWon("Moria"));
	CHECK(logic.actName() == "Two");
	CHECK(logic.findArmy("Moria_Mouseover") == nullptr);
	CHECK_FALSE(logic.findArmy("Fellowship")->playerControlled);
	CHECK(logic.enabledRegions().size() == 3);
	CHECK(logic.errors().empty());

	// notre version : un SummaryEvent va dans la file du résumé (0x7C2280, +0x5C), pas dans celle qui est jouée : à 2 s de l'acte Two, une seule voix
	{
		ActCampaignLogic fresh;
		REQUIRE(fresh.start(stores, "GondorCampaign", &error));
		REQUIRE(fresh.regionWon("Moria"));
		fresh.takeCommands();
		std::vector<std::string> sounds;
		for (int i = 0; i < 90; ++i) // 3 s à 30 images par seconde, avant les voix restantes de l'acte One (à 16 s)
		{
			fresh.update();
			for (const ActCommand &c : fresh.takeCommands())
			{
				if (c.type == ActCommand::AUDIO)
				{
					sounds.push_back(c.text);
				}
			}
		}
		CHECK(std::count(sounds.begin(), sounds.end(), "GoodRohanSkirmish1-2") == 1);
		CHECK(std::count(sounds.begin(), sounds.end(), "GoodRohanSkirmish1-1") == 0);
	}

	// Tous les actes de bout en bout : aucune armée ni région ne manque dans les données
	for (const char *name : { "GondorCampaign", "MordorCampaign" })
	{
		ActCampaignLogic all;
		REQUIRE(all.start(stores, name, &error));
		for (size_t i = 0; i < all.campaign().acts.size(); ++i)
		{
			all.stepAct();
		}
		CAPTURE(name);
		std::string joined;
		for (const std::string &e : all.errors())
		{
			joined += e + "\n";
		}
		CHECK_MESSAGE(all.errors().empty(), joined);
	}
}

TEST_CASE("act campaign retail: notre version, BFME1: the zones of a map are its PolygonTriggers, the scripts find them by name")
{
	const char *install = std::getenv("BFME1_INSTALL");
	if (!install)
	{
		retailtest::printSkip("BFME1 maps (BFME1_INSTALL unset)");
		return;
	}
	Win32BIGFileSystem fs;
	std::string error;
	auto archive = fs.openArchiveFile((std::filesystem::u8path(install) / "Maps.big").u8string(), &error);
	REQUIRE_MESSAGE(archive, error);
	fs.mountArchive(std::move(archive), "Maps.big", false);
	std::vector<std::uint8_t> bytes;
	REQUIRE_MESSAGE(fs.readFile("maps/map good moria/map good moria.map", bytes, &error), error);
	MapReadOptions options;
	options.bfme1 = true;
	LoadedMap map;
	REQUIRE_MESSAGE(MapReader::load(bytes, "map good moria.map", options, map, &error), error);
	CHECK(map.chunks.hasTriggerAreas);
	for (const char *name : { "Balins_Tomb_Trigger", "Dialog Encounter 05", "Dialog Encounter 28" })
	{
		// tolerance: un polygone compte au moins 3 points ; ce test vérifie que la zone est lue, pas son nombre exact de points
		CHECK(std::any_of(map.chunks.triggerAreas.begin(), map.chunks.triggerAreas.end(), [&](const TriggerArea &t) { return t.name == name && t.points.size() >= 3; }));
	}
}
