// OpenBFME. GPL-3.0.
//
// La campagne de BFME1 : un déroulé scripté par actes (LivingWorldCampaign GondorCampaign, MordorCampaign), différent du tour par tour de la Guerre
// de l'Anneau de RotWK (LivingWorldLogic.h), dont le fichier LivingWorldData.cpp refuse d'ailleurs ces blocs pour BFME1.
//
// FAITS CIBLE (lotrbfme.exe, adresses en VA ; base 0x400000) :
//   - tables de champs : LivingWorldCampaign 0x10EC7CC, Act 0x10EC9B8, EnableRegion 0x10ECE14, ForceBattle 0x10ECEC8, SpawnArmy 0x10ECFF0,
//     MoveArmy 0x10ED108, ToggleArmyControl 0x10ED1A0, MergePlayerArmy 0x10ED258, WorldText 0x10ED300, UpdateAnimObject 0x10ED3B8,
//     ModifyArmyEntry 0x10ED420, SplineCamera 0x10ED4C0, SkirmishOpponent 0x10ED540, RegionReinforcements 0x10ED6A0, EyeTowerPoints 0x10ED784,
//     effet de région 0x10ED888, AudioEvent 0x10ECDC8, MoveCamera 0x10ECD18, ControlPoint 0x10753D0, LivingWorldRegionCampaign 0x10EDC48,
//     Region 0x1116D70, LivingWorldPlayerArmy 0x10E90A8, ArmyEntry 0x10E8FE8, LivingWorldMapInfo 0x1116000, LivingWorldArmyIcon 0x1115DC8,
//     objet d'icône 0x1115CC0 ;
//   - les durées (0xC52D90) sont lues en secondes et rangées en millisecondes (facteur 1000.0f à 0x1075C68) ;
//   - entrer dans un acte (0x7AEE70) traite dans l'ordre : régions, batailles forcées, armées créées, marches, armées retirées, contrôle des armées,
//     renforts, fusions, puis le reste de l'acte ; CallActSubroutine (0x7AEF20) entre dans l'acte nommé, à l'intérieur de celui-ci ;
//   - une armée (0x7A51D0, 0x7A4E30, 0x7A50F0, 0x7A4FD0) dépile une marche quand elle est à l'arrêt, avance de min(vitesse, distance) par image, finit une marche
//     sous 1 unité et enchaîne aussitôt la suivante ; quand sa file est vide et que le drapeau PlayNextActAfterMove (celui de l'armée, 0x7C028E) est levé,
//     l'acte avance (0x7BEED0) ;
//   - fin d'une région gagnée (Open-BFME-1 Gen003BFB20OwnerProbe.cpp, 0x7BFB20) : la région passe au joueur, sort des régions actives, sa prime
//     ArmyBonus s'ajoute aux points d'armée, puis soit on entre dans son OnWinActSubroutine sans changer d'acte courant, soit l'acte courant avance ;
//   - avancer d'un acte (Open-BFME-1 Glo012F1024Entry_bfmeStep.cpp, 0x7A7320) : le compteur s'arrête à la limite, le premier acte marqué EndAct
//     (ou le dernier acte, LivingWorldCampaign.cpp 0x7A73C0).
// FAITS DONNEUR : Open-BFME-1 (noms des champs et enregistrements), citée fichier par fichier ci-dessus.
// INFÉRENCES : la cadence de 30 images par seconde (le binaire compte les caméras en secondes * 30, 0x7BE020) ; l'accélération *10 de 0x7A40F1 n'est pas portée.

#pragma once

#include "Common/INIDataTypes.h"

#include <cstdint>
#include <map>
#include <optional>
#include <memory>
#include <set>
#include <string>
#include <vector>

class ArchiveFileSystem;

namespace LivingWorld
{

struct ActArmyEntry
{
	std::string thingTemplate;
	int quantity = 0;
	bool isDefault = false;
};

struct ActPlayerArmy
{
	std::string name, displayNameTag, replenishArmyName;
	RGBColor color{ 0, 0, 0 }, nightColor{ 0, 0, 0 };
	std::vector<ActArmyEntry> entries;
	int survivalThreshhold = 0, minCommandPoints = 0;
};

struct ActSpawnArmy
{
	std::string name, icon, banner, playerArmy, palantirMovie, iconSize, faction;
	Coord2D position;
	bool playerControlled = false, playerOwned = false, isCity = false;
};

struct ActToggleArmyControl
{
	std::string name;
	bool playerControl = false;
};

struct ActMoveArmy
{
	std::string name, palantirMovie, iconSize;
	Coord2D moveTo;
	bool playNextActAfterMove = false;
	float moveSpeed = 0.0f; // 0 : pas de vitesse donnée par la ligne (constructeur 0x444823)
};

struct ActForceBattle
{
	std::string region, eventName, movie, useArmy;
	Coord2D position, armyAttackDirection;
	bool playNextActAfterMovie = false, removeEvent = false;
};

struct ActMergeArmy
{
	std::string sourceArmy, destArmy, splitArmyTemplate;
	bool splitArmy = false;
};

struct ActRegionReinforcements
{
	std::string regionName, pathFindRule;
	std::vector<std::string> armies;
	int closeDistanceTime = 0, mediumDistanceTime = 0, farDistanceTime = 0;
	bool autoSummon = false;
};

struct ActWorldText
{
	int delayMs = 0, durationMs = 0;
	std::string stringTag;
	bool summaryEvent = false;
};

struct ActAudioEvent
{
	int delayMs = 0;
	std::string sound;
	bool summaryEvent = false;
};

struct ActMoveCamera
{
	int delayMs = 0, scrollTimeMs = 0;
	Coord3D position;
	float viewAngle = 0.0f;
	bool summaryEvent = false;
};

struct ActControlPoint
{
	Coord3D position;
	float angle = 0.0f;
	int timeMs = 0;
	float easeIn = 0.5f, easeOut = 0.5f; // la valeur par défaut que documente l'INI de la campagne
};

struct ActSplineCamera
{
	int delayMs = 0;
	bool summaryEvent = false;
	std::vector<ActControlPoint> points;
};

struct ActUpdateAnimObject
{
	std::string animObjectName;
	Coord3D worldPos;
};

struct ActModifyArmyEntry
{
	std::string playerArmy, curUnitTemplate, newUnitTemplate;
};

struct CampaignAct
{
	std::string name, jumpToAct;
	std::vector<std::string> enableRegions, despawnArmies, callActSubroutines;
	std::vector<ActForceBattle> forceBattles;
	std::vector<ActSpawnArmy> spawnArmies;
	std::vector<ActToggleArmyControl> toggleArmyControls;
	std::vector<ActMoveArmy> moveArmies;
	std::vector<ActRegionReinforcements> regionReinforcements;
	std::vector<ActMergeArmy> mergeArmies;
	std::vector<ActMoveCamera> moveCameras;
	std::vector<ActSplineCamera> splineCameras;
	std::vector<ActWorldText> worldTexts;
	std::vector<ActAudioEvent> audioEvents;
	std::vector<ActUpdateAnimObject> updateAnimObjects;
	std::vector<std::vector<Coord2D>> eyeTowerPoints;
	std::vector<ActModifyArmyEntry> modifyArmyEntries;
	bool endAct = false;
};

struct ActCampaignData
{
	std::string name;
	bool isEvil = false;
	std::vector<CampaignAct> acts;
	int lastAct = 0; // LivingWorldCampaign.cpp 0x7A73C0 : le premier acte marqué EndAct, sinon le dernier

	int actIndex(const std::string &act) const; // -1 : absent
};

struct ActSkirmishOpponent
{
	std::string name, faction;
	int startPosition = 0, teamId = 0;
	std::vector<std::string> allowInActs;
	bool isPlayer = false;
};

// Un emplacement de la partie d'une carte SPS (notre port de 0xA1B500 / 0xA19C40, voir skirmishSlotsOf)
struct ActSkirmishSlot
{
	int slot = 0;           // 0 : le joueur ; les adversaires à partir de 1, dans l'ordre de la région
	std::string name;       // "PlayerHuman" pour le joueur (0x1084094), le Name de l'adversaire sinon : un camp de la carte
	std::string faction;
	int startPosition = 0, team = 0;
	bool human = false;
};

struct ActRegion
{
	std::string name, mapName, displayName, conqueredNotice, movieNameFirstTime, movieNameRepeat, skirmishStillImage, skirmishVoiceTrack, skirmishMusicTrack;
	std::string subObject, onWinActSubroutine, regionPortrait, difficultyText, regionBonus;
	std::vector<ActSkirmishOpponent> opponents;
	std::vector<std::string> connectsTo, missionObjectiveTags, bonusMissionObjectiveTags;
	std::vector<Coord2D> armyPlacementPos;
	bool unpackCamps = false, customCenterPoint = false, endOfCampaign = false, customUIPopupPoint = false;
	Coord2D centerPoint, uiPopupPoint;
	int armyBonus = 0, legendaryBonus = 0, resourceBonus = 0, displayActNum = 0;
};

// Les neuf blocs d'effet d'une campagne de régions (table 0x10ED888)
struct ActRegionEffect
{
	float opacity = 1.0f;
	std::vector<std::string> geometry;
	std::vector<ActControlPoint> points;
};

struct ActRegionCampaign
{
	std::string name, regionObject, regionBonusArmy, regionBonusResource, regionBonusLegendary, regionConqueredSound;
	float zOffset = 0.0f;
	int smallArmyCommandPoints = 0, mediumArmyCommandPoints = 0;
	std::vector<Coord2D> armyPlacementPos;
	std::map<std::string, ActRegionEffect> effects;
	RGBAColorInt regionPopupDefaultColor{ 0, 0, 0, 0 }, regionPopupOverColor{ 0, 0, 0, 0 };
	std::vector<ActRegion> regions;

	const ActRegion *findRegion(const std::string &region) const;
};

struct ActIconObject
{
	std::string name, model, pickbox;
	float zOffset = 0.0f, scale = 1.0f, orientAngle = 0.0f;
	bool clickable = false, hidden = false, canFade = false;
};

struct ActArmyIcon
{
	std::string name, onSelectedSound, onMoveSound;
	std::vector<ActIconObject> objects;
};

// Les champs de LivingWorldMapInfo que le déroulé utilise ; les autres lignes de la table 0x1116000 sont lues et comptées dans notRead.
struct ActMapInfo
{
	std::string mapObject, cloudBorderSubObject, textLayerSubObject, battleMarker, palantirMarker, enterMapSound, battleMarkerCreatedSound;
	Coord2D center, extent, sunDir, accent1Dir, accent2Dir;
	RGBColor ambient{ 0, 0, 0 }, sunRGB{ 0, 0, 0 }, accent1RGB{ 0, 0, 0 }, accent2RGB{ 0, 0, 0 };
	float cameraBoundX = 0.0f, cameraBoundY = 0.0f, armyHeight = 0.0f, bannerHeight = 0.0f;
	std::set<std::string> notRead;
	bool loaded = false;
};

class ActCampaignStores
{
public:
	// Les InitFile de SubsystemLegend.ini : TheLivingWorldManager (LivingWorld.ini), TheLivingWorldCampaignManager (trois fichiers de campagne)
	bool load(ArchiveFileSystem &fs, std::string *error);

	const ActCampaignData *findCampaign(const std::string &name) const;
	const ActRegionCampaign *findRegionCampaign(const std::string &name) const;
	const ActPlayerArmy *findPlayerArmy(const std::string &name) const;
	const ActArmyIcon *findArmyIcon(const std::string &name) const;

	std::vector<ActCampaignData> campaigns;
	std::vector<ActRegionCampaign> regionCampaigns;
	std::vector<ActPlayerArmy> playerArmies;
	std::vector<ActArmyIcon> armyIcons;
	ActMapInfo mapInfo;
};

// notre version, BFME1 : l'armée du joueur qui entre dans une région (ActCampaignLogic::walkOnArmyOf)
struct ActWalkOnArmy
{
	std::string name, playerArmy;
	std::vector<ActArmyEntry> entries;
	int survivalThreshhold = 0;
	Coord2D direction;
};

// Une armée en jeu sur la carte du monde
struct ActArmy
{
	std::string name, icon, banner, playerArmy, palantirMovie, iconSize, faction;
	Coord2D position;
	bool playerControlled = false, playerOwned = false, isCity = false;
	struct Move
	{
		Coord2D to;
		float speed = 0.0f; // MoveSpeed ; 0 : garde la vitesse de l'armée
		std::string palantirMovie, iconSize;
	};
	std::vector<Move> moves; // la file (+0x28), la première ligne en tête
	Coord2D target;			 // +0x14
	float speedPerFrame = 0.0f; // +0x20
	bool moving = false;		// +0x1C
	bool playNextAct = false;	// +0x1F
};

// Ce que la présentation doit faire à cette image
struct ActCommand
{
	enum Type
	{
		WORLD_TEXT,
		AUDIO,
		MOVE_CAMERA,
		SPLINE_CAMERA,
		EYE_TOWER,
		UPDATE_ANIM,
	} type = WORLD_TEXT;
	std::string text; // la balise du texte, le nom du son ou de l'objet animé
	int durationMs = 0;
	bool summaryEvent = false;
	Coord3D position;
	float angle = 0.0f;
	std::vector<ActControlPoint> spline;
	std::vector<Coord2D> lookPoints;
};

class ActCampaignLogic
{
public:
	static constexpr int FRAMES_PER_SECOND = 30; // inférence : la cadence du monde (voir ActCampaign.cpp)

	// false : la campagne, sa campagne de régions ou une de ses armées manque (une erreur, jamais un défaut)
	bool start(const ActCampaignStores &stores, const std::string &campaign, std::string *error);
	bool started() const { return m_campaign != nullptr; }

	void update(); // une image
	std::vector<ActCommand> takeCommands();

	const ActCampaignData &campaign() const { return *m_campaign; }
	const ActRegionCampaign &regionCampaign() const { return *m_regions; }
	int frame() const { return m_frame; }
	int actIndex() const { return m_actIndex; }
	const std::string &actName() const;
	const std::vector<ActArmy> &armies() const { return m_armies; }
	const std::vector<std::string> &enabledRegions() const { return m_enabled; }
	const std::set<std::string> &conqueredRegions() const { return m_conquered; }
	const std::vector<ActForceBattle> &forcedBattles() const { return m_forced; }
	int armyPoints() const { return m_armyPoints; }
	int legendaryPoints() const { return m_legendaryPoints; }
	bool campaignWon() const { return m_campaignWon; }

	bool canEnterRegion(const std::string &region) const; // active et pas encore gagnée, ou une bataille forcée
	// FAITS CIBLE (lotrbfme.exe) : 0xA1B500 remplit SinglePlayerSkirmishGameInfo depuis les SkirmishOpponent de la région. Sans adversaire, pas de SinglePlayerSkirmishGameInfo : la
	// carte est une mission ordinaire (0xA1B557, liste vide). IsPlayer (+0x18) : l'emplacement 0, nommé "PlayerHuman" (0xA19CA8), état SLOT_PLAYER (5). Sinon, l'adversaire n'entre
	// que si AllowInActs contient le nom de l'acte (0x7B9100 : une liste vide ne contient rien) : emplacement suivant à partir de 1, nommé de son Name (0xA196C0), état SLOT_MED_AI
	// (3). Aucun adversaire retenu : le premier non joueur de la région prend l'emplacement 1, quel que soit l'acte (0xA1A0F0). Couleur = numéro d'emplacement, position =
	// StartPosition, équipe = TeamID (0xA19D44 ff).
	// INFÉRENCE : les adversaires sont parcourus dans l'ordre du fichier ; 0xA1B500 passe par un itérateur (0x9277B0, 0x9278B0, 0x927830) non lu, l'ordre ne change que les couleurs
	// d'un camp sans playerColor. S-5017, non porté : le camp du joueur devient celui de l'armée d'invasion de la région (Region +0xB8, 0x7CA262 -> 0x7C0110 -> armée +0x40), le joueur
	// ne déplaçant pas encore d'armée
	std::vector<ActSkirmishSlot> skirmishSlotsOf(const std::string &region) const;
	// FAITS CIBLE (lotrbfme.exe) : l'armée d'une région est Region +0xB8, sa direction +0xBC / +0xC0, écrites par 0x7C9CB0 depuis deux chemins : ForceBattle (UseArmy et
	// ArmyAttackDirection, 0x7AD6E1 / 0x7AD726) et la fin de marche d'une armée du joueur (0x7A4FD0 : +0x1D et +0x1E vrais, la région sous sa position, la direction de sa dernière
	// marche). L'entrée dans la région (0x7C3850 -> 0x7BDE00 -> 0x787060) les recopie dans TheGameLogic +0x174 / +0x178 pour la mission.
	// INFÉRENCE : le joueur ne déplace pas encore d'armée sur la carte (S-5017). Sans ForceBattle, l'armée retenue est celle que le joueur possède et contrôle, la plus proche du
	// centre de la région, et sa direction va d'elle vers ce centre ; sans centre connu, la direction est nulle. Aucune armée possible : nullopt, comme une région sans Region +0xB8.
	std::optional<ActWalkOnArmy> walkOnArmyOf(const std::string &region) const;
	// Le film à montrer : la première entrée dans une région, puis le film de répétition
	std::string enterRegion(const std::string &region);

	// Gen003BFB20OwnerProbe.cpp + Rva003BFB20_gate.cpp : la région gagnée. false : la région n'était pas jouable (l'appelant a un bug)
	bool regionWon(const std::string &region);
	// Glo012F1024Entry_bfmeStep.cpp 0x7A7320
	void stepAct();

	ActArmy *findArmy(const std::string &name);

	// Les erreurs de données rencontrées en cours de route (un acte ou une armée nommés et absents) ; lues par le rapport, jamais ignorées
	const std::vector<std::string> &errors() const { return m_errors; }

private:
	struct Timed
	{
		int frame;
		ActCommand command;
	};
	void enterAct(const CampaignAct &act, int depth);

	const ActCampaignStores *m_stores = nullptr;
	const ActCampaignData *m_campaign = nullptr;
	const ActRegionCampaign *m_regions = nullptr;
	int m_frame = 0, m_actIndex = 0, m_armyPoints = 0, m_legendaryPoints = 0;
	std::vector<Timed> m_timed;
	std::vector<ActCommand> m_commands;
	std::vector<ActArmy> m_armies;
	std::vector<std::string> m_enabled;
	std::set<std::string> m_conquered, m_visited;
	std::vector<ActForceBattle> m_forced;
	std::map<std::string, std::vector<ActArmyEntry>> m_modifiedEntries; // les ModifyArmyEntry déjà joués, par LivingWorldPlayerArmy
	std::vector<std::string> m_errors;
	bool m_campaignWon = false;
};

} // namespace LivingWorld
