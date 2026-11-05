// OpenBFME. GPL-3.0.
//
// Voir ActCampaign.h pour les faits cible, donneur et les inférences.

#include "GameLogic/LivingWorld/ActCampaign.h"

#include "Common/ArchiveFileSystem.h"
#include "Common/INI.h"
#include "Common/INI/INIBlockStubs.h"
#include "Common/INIException.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace LivingWorld
{
namespace
{

// 0x7A54A4 : la vitesse d'une armée créée, 2/3 d'unité par image (0x3F2AAAAB) ; 0x7A4E30 : une ligne MoveArmy avec une vitesse la remplace par MoveSpeed * 1/6 (0x10EC714)
// et la garde pour les marches suivantes ; une ligne sans vitesse (0, 0x1075350) ne la change pas
constexpr float kInitialSpeedPerFrame = 2.0f / 3.0f;
constexpr float kMoveSpeedScale = 1.0f / 6.0f;
constexpr float kArrivalDistance = 1.0f; // 0x7A5185 : la marche se termine sous 1.0 (0x107C640)

// 0xC52D90 : une durée lue en secondes, rangée en millisecondes
void parseSecondsToMs(INI *ini, void *, void *store, const void *)
{
	*static_cast<int *>(store) = (int)std::lround(ini->scanReal(ini->getNextToken()) * 1000.0f);
}

// 0x429BE0, 0x40B960 : un Coord2D de plus
void parseCoord2DAppend(INI *ini, void *, void *store, const void *)
{
	Coord2D c;
	INI::parseCoord2D(ini, nullptr, &c, nullptr);
	static_cast<std::vector<Coord2D> *>(store)->push_back(c);
}

// Une chaîne de plus, un seul jeton par ligne (0xC541E0)
void parseStringAppend(INI *ini, void *, void *store, const void *)
{
	static_cast<std::vector<std::string> *>(store)->push_back(ini->getNextToken());
}

// 0xC542B0, 0x42245D : tous les jetons de la ligne
void parseStringsOfLine(INI *ini, void *, void *store, const void *)
{
	auto *out = static_cast<std::vector<std::string> *>(store);
	while (const char *token = ini->getNextTokenOrNull())
	{
		out->push_back(token);
	}
}

// Une ligne de LivingWorldMapInfo que le déroulé n'utilise pas encore : la table la connaît, sa valeur n'est pas interprétée, et le rapport le dit
void parseMapInfoNotRead(INI *ini, void *instance, void *, const void *userData)
{
	while (ini->getNextTokenOrNull())
	{
	}
	static_cast<ActMapInfo *>(instance)->notRead.insert(static_cast<const char *>(userData));
}

#define OF(type, member) (int)offsetof(type, member)

// ---- LivingWorldPlayerArmy (0x10E90A8), ArmyEntry (0x10E8FE8) ----
const FieldParse kArmyEntryFields[] = {
	{ "ThingTemplate", INI::parseAsciiString, nullptr, OF(ActArmyEntry, thingTemplate) },
	{ "Quantity", INI::parseInt, nullptr, OF(ActArmyEntry, quantity) },
	{ "Default", INI::parseBool, nullptr, OF(ActArmyEntry, isDefault) },
	{ nullptr, nullptr, nullptr, 0 },
};
void parseArmyEntry(INI *ini, void *instance, void *, const void *)
{
	ActArmyEntry e;
	ini->initFromINI(&e, kArmyEntryFields);
	static_cast<ActPlayerArmy *>(instance)->entries.push_back(e);
}
const FieldParse kPlayerArmyFields[] = {
	{ "Name", INI::parseAsciiString, nullptr, OF(ActPlayerArmy, name) },
	{ "DisplayNameTag", INI::parseAsciiString, nullptr, OF(ActPlayerArmy, displayNameTag) },
	{ "Color", INI::parseRGBColor, nullptr, OF(ActPlayerArmy, color) },
	{ "NightColor", INI::parseRGBColor, nullptr, OF(ActPlayerArmy, nightColor) },
	{ "ArmyEntry", parseArmyEntry, nullptr, 0 },
	{ "SurvivalThreshhold", INI::parseInt, nullptr, OF(ActPlayerArmy, survivalThreshhold) },
	{ "MinCommandPoints", INI::parseInt, nullptr, OF(ActPlayerArmy, minCommandPoints) },
	{ "ReplenishArmyName", INI::parseAsciiString, nullptr, OF(ActPlayerArmy, replenishArmyName) },
	{ nullptr, nullptr, nullptr, 0 },
};

// ---- Les lignes d'un acte ----
const FieldParse kSpawnArmyFields[] = {
	{ "Name", INI::parseAsciiString, nullptr, OF(ActSpawnArmy, name) },
	{ "Icon", INI::parseAsciiString, nullptr, OF(ActSpawnArmy, icon) },
	{ "Banner", INI::parseAsciiString, nullptr, OF(ActSpawnArmy, banner) },
	{ "Position", INI::parseCoord2D, nullptr, OF(ActSpawnArmy, position) },
	{ "PlayerControlled", INI::parseBool, nullptr, OF(ActSpawnArmy, playerControlled) },
	{ "PlayerOwned", INI::parseBool, nullptr, OF(ActSpawnArmy, playerOwned) },
	{ "PlayerArmy", INI::parseAsciiString, nullptr, OF(ActSpawnArmy, playerArmy) },
	{ "PalantirMovie", INI::parseAsciiString, nullptr, OF(ActSpawnArmy, palantirMovie) },
	{ "IconSize", INI::parseAsciiString, nullptr, OF(ActSpawnArmy, iconSize) },
	{ "Faction", INI::parseAsciiString, nullptr, OF(ActSpawnArmy, faction) },
	{ "IsCity", INI::parseBool, nullptr, OF(ActSpawnArmy, isCity) },
	{ nullptr, nullptr, nullptr, 0 },
};
const FieldParse kToggleFields[] = {
	{ "Name", INI::parseAsciiString, nullptr, OF(ActToggleArmyControl, name) },
	{ "PlayerControl", INI::parseBool, nullptr, OF(ActToggleArmyControl, playerControl) },
	{ nullptr, nullptr, nullptr, 0 },
};
const FieldParse kMoveArmyFields[] = {
	{ "Name", INI::parseAsciiString, nullptr, OF(ActMoveArmy, name) },
	{ "MoveTo", INI::parseCoord2D, nullptr, OF(ActMoveArmy, moveTo) },
	{ "PlayNextActAfterMove", INI::parseBool, nullptr, OF(ActMoveArmy, playNextActAfterMove) },
	{ "PalantirMovie", INI::parseAsciiString, nullptr, OF(ActMoveArmy, palantirMovie) },
	{ "IconSize", INI::parseAsciiString, nullptr, OF(ActMoveArmy, iconSize) },
	{ "MoveSpeed", INI::parseReal, nullptr, OF(ActMoveArmy, moveSpeed) },
	{ nullptr, nullptr, nullptr, 0 },
};
const FieldParse kForceBattleFields[] = {
	{ "Region", INI::parseAsciiString, nullptr, OF(ActForceBattle, region) },
	{ "EventName", INI::parseAsciiString, nullptr, OF(ActForceBattle, eventName) },
	{ "Position", INI::parseCoord2D, nullptr, OF(ActForceBattle, position) },
	{ "Movie", INI::parseAsciiString, nullptr, OF(ActForceBattle, movie) },
	{ "PlayNextActAfterMovie", INI::parseBool, nullptr, OF(ActForceBattle, playNextActAfterMovie) },
	{ "RemoveEvent", INI::parseBool, nullptr, OF(ActForceBattle, removeEvent) },
	{ "UseArmy", INI::parseAsciiString, nullptr, OF(ActForceBattle, useArmy) },
	{ "ArmyAttackDirection", INI::parseCoord2D, nullptr, OF(ActForceBattle, armyAttackDirection) },
	{ nullptr, nullptr, nullptr, 0 },
};
const FieldParse kMergeFields[] = {
	{ "SourceArmy", INI::parseAsciiString, nullptr, OF(ActMergeArmy, sourceArmy) },
	{ "DestArmy", INI::parseAsciiString, nullptr, OF(ActMergeArmy, destArmy) },
	{ "SplitArmyTemplate", INI::parseAsciiString, nullptr, OF(ActMergeArmy, splitArmyTemplate) },
	{ "SplitArmy", INI::parseBool, nullptr, OF(ActMergeArmy, splitArmy) },
	{ nullptr, nullptr, nullptr, 0 },
};
const FieldParse kReinforcementFields[] = {
	{ "RegionName", INI::parseAsciiString, nullptr, OF(ActRegionReinforcements, regionName) },
	{ "AddReinforcementArmy", parseStringAppend, nullptr, OF(ActRegionReinforcements, armies) },
	{ "CloseDistanceTime", INI::parseInt, nullptr, OF(ActRegionReinforcements, closeDistanceTime) },
	{ "MediumDistanceTime", INI::parseInt, nullptr, OF(ActRegionReinforcements, mediumDistanceTime) },
	{ "FarDistanceTime", INI::parseInt, nullptr, OF(ActRegionReinforcements, farDistanceTime) },
	{ "PathFindRule", INI::parseAsciiString, nullptr, OF(ActRegionReinforcements, pathFindRule) },
	{ "AutoSummon", INI::parseBool, nullptr, OF(ActRegionReinforcements, autoSummon) },
	{ nullptr, nullptr, nullptr, 0 },
};
const FieldParse kWorldTextFields[] = {
	{ "DelayFromActStart", parseSecondsToMs, nullptr, OF(ActWorldText, delayMs) },
	{ "Duration", parseSecondsToMs, nullptr, OF(ActWorldText, durationMs) },
	{ "StringTag", INI::parseAsciiString, nullptr, OF(ActWorldText, stringTag) },
	{ "SummaryEvent", INI::parseBool, nullptr, OF(ActWorldText, summaryEvent) },
	{ nullptr, nullptr, nullptr, 0 },
};
const FieldParse kAudioFields[] = {
	{ "DelayFromActStart", parseSecondsToMs, nullptr, OF(ActAudioEvent, delayMs) },
	{ "Sound", INI::parseAsciiString, nullptr, OF(ActAudioEvent, sound) },
	{ "SummaryEvent", INI::parseBool, nullptr, OF(ActAudioEvent, summaryEvent) },
	{ nullptr, nullptr, nullptr, 0 },
};
const FieldParse kMoveCameraFields[] = {
	{ "DelayFromActStart", parseSecondsToMs, nullptr, OF(ActMoveCamera, delayMs) },
	{ "Position", INI::parseCoord3D, nullptr, OF(ActMoveCamera, position) },
	{ "ViewAngle", INI::parseReal, nullptr, OF(ActMoveCamera, viewAngle) },
	{ "ScrollTime", parseSecondsToMs, nullptr, OF(ActMoveCamera, scrollTimeMs) },
	{ "SummaryEvent", INI::parseBool, nullptr, OF(ActMoveCamera, summaryEvent) },
	{ nullptr, nullptr, nullptr, 0 },
};
const FieldParse kControlPointFields[] = {
	{ "Position", INI::parseCoord3D, nullptr, OF(ActControlPoint, position) },
	{ "Angle", INI::parseReal, nullptr, OF(ActControlPoint, angle) },
	{ "Time", parseSecondsToMs, nullptr, OF(ActControlPoint, timeMs) },
	{ "EaseIn", INI::parseReal, nullptr, OF(ActControlPoint, easeIn) },
	{ "EaseOut", INI::parseReal, nullptr, OF(ActControlPoint, easeOut) },
	{ nullptr, nullptr, nullptr, 0 },
};
void parseSplineControlPoint(INI *ini, void *instance, void *, const void *)
{
	ActControlPoint c;
	ini->initFromINI(&c, kControlPointFields);
	static_cast<ActSplineCamera *>(instance)->points.push_back(c);
}
const FieldParse kSplineFields[] = {
	{ "DelayFromActStart", parseSecondsToMs, nullptr, OF(ActSplineCamera, delayMs) },
	{ "SummaryEvent", INI::parseBool, nullptr, OF(ActSplineCamera, summaryEvent) },
	{ "ControlPoint", parseSplineControlPoint, nullptr, 0 },
	{ nullptr, nullptr, nullptr, 0 },
};
const FieldParse kUpdateAnimFields[] = {
	{ "AnimObjectName", INI::parseAsciiString, nullptr, OF(ActUpdateAnimObject, animObjectName) },
	{ "WorldPos", INI::parseCoord3D, nullptr, OF(ActUpdateAnimObject, worldPos) },
	{ nullptr, nullptr, nullptr, 0 },
};
const FieldParse kModifyFields[] = {
	{ "PlayerArmy", INI::parseAsciiString, nullptr, OF(ActModifyArmyEntry, playerArmy) },
	{ "CurUnitTemplate", INI::parseAsciiString, nullptr, OF(ActModifyArmyEntry, curUnitTemplate) },
	{ "NewUnitTemplate", INI::parseAsciiString, nullptr, OF(ActModifyArmyEntry, newUnitTemplate) },
	{ nullptr, nullptr, nullptr, 0 },
};

template <typename T, std::vector<T> CampaignAct::*Member>
void parseIntoAct(INI *ini, void *instance, void *, const void *userData)
{
	T record;
	ini->initFromINI(&record, static_cast<const FieldParse *>(userData));
	(static_cast<CampaignAct *>(instance)->*Member).push_back(std::move(record));
}
// EnableRegion (0x10ECE14) : un bloc d'une seule ligne, Region
void parseEnableRegion(INI *ini, void *instance, void *, const void *)
{
	std::string region;
	static const FieldParse kFields[] = { { "Region", INI::parseAsciiString, nullptr, 0 }, { nullptr, nullptr, nullptr, 0 } };
	ini->initFromINI(&region, kFields);
	static_cast<CampaignAct *>(instance)->enableRegions.push_back(region);
}
// EyeTowerPoints (0x10ED784)
void parseEyeTower(INI *ini, void *instance, void *, const void *)
{
	std::vector<Coord2D> points;
	static const FieldParse kFields[] = { { "LookPoint", parseCoord2DAppend, nullptr, 0 }, { nullptr, nullptr, nullptr, 0 } };
	ini->initFromINI(&points, kFields);
	static_cast<CampaignAct *>(instance)->eyeTowerPoints.push_back(std::move(points));
}
const FieldParse kActFields[] = {
	{ "EnableRegion", parseEnableRegion, nullptr, 0 },
	{ "ForceBattle", parseIntoAct<ActForceBattle, &CampaignAct::forceBattles>, kForceBattleFields, 0 },
	{ "SpawnArmy", parseIntoAct<ActSpawnArmy, &CampaignAct::spawnArmies>, kSpawnArmyFields, 0 },
	{ "DespawnArmy", parseStringAppend, nullptr, OF(CampaignAct, despawnArmies) },
	{ "CallActSubroutine", parseStringAppend, nullptr, OF(CampaignAct, callActSubroutines) },
	{ "JumpToAct", INI::parseAsciiString, nullptr, OF(CampaignAct, jumpToAct) },
	{ "MoveArmy", parseIntoAct<ActMoveArmy, &CampaignAct::moveArmies>, kMoveArmyFields, 0 },
	{ "ToggleArmyControl", parseIntoAct<ActToggleArmyControl, &CampaignAct::toggleArmyControls>, kToggleFields, 0 },
	{ "RegionReinforcements", parseIntoAct<ActRegionReinforcements, &CampaignAct::regionReinforcements>, kReinforcementFields, 0 },
	{ "MergePlayerArmy", parseIntoAct<ActMergeArmy, &CampaignAct::mergeArmies>, kMergeFields, 0 },
	{ "MoveCamera", parseIntoAct<ActMoveCamera, &CampaignAct::moveCameras>, kMoveCameraFields, 0 },
	{ "SplineCamera", parseIntoAct<ActSplineCamera, &CampaignAct::splineCameras>, kSplineFields, 0 },
	{ "WorldText", parseIntoAct<ActWorldText, &CampaignAct::worldTexts>, kWorldTextFields, 0 },
	{ "AudioEvent", parseIntoAct<ActAudioEvent, &CampaignAct::audioEvents>, kAudioFields, 0 },
	{ "EndAct", INI::parseBool, nullptr, OF(CampaignAct, endAct) },
	{ "UpdateAnimObject", parseIntoAct<ActUpdateAnimObject, &CampaignAct::updateAnimObjects>, kUpdateAnimFields, 0 },
	{ "EyeTowerPoints", parseEyeTower, nullptr, 0 },
	{ "ModifyArmyEntry", parseIntoAct<ActModifyArmyEntry, &CampaignAct::modifyArmyEntries>, kModifyFields, 0 },
	{ nullptr, nullptr, nullptr, 0 },
};

// ---- LivingWorldCampaign (0x10EC7CC) ----
void parseAct(INI *ini, void *instance, void *, const void *)
{
	CampaignAct act;
	const char *name = ini->getNextTokenOrNull();
	if (!name)
	{
		throw INIException(3, "ParseLivingWorldCampaignAct::No act name specified."); // ParseLivingWorldCampaignAct.cpp, 0x7B2BB0
	}
	act.name = name;
	ini->initFromINI(&act, kActFields);
	static_cast<ActCampaignData *>(instance)->acts.push_back(std::move(act));
}
const FieldParse kCampaignFields[] = {
	{ "Act", parseAct, nullptr, 0 },
	{ "IsEvilCampaign", INI::parseBool, nullptr, OF(ActCampaignData, isEvil) },
	{ nullptr, nullptr, nullptr, 0 },
};

// ---- Region (0x1116D70), SkirmishOpponent (0x10ED540) ----
const FieldParse kOpponentFields[] = {
	{ "Name", INI::parseAsciiString, nullptr, OF(ActSkirmishOpponent, name) },
	{ "Faction", INI::parseAsciiString, nullptr, OF(ActSkirmishOpponent, faction) },
	{ "StartPosition", INI::parseInt, nullptr, OF(ActSkirmishOpponent, startPosition) },
	{ "AllowInActs", parseStringsOfLine, nullptr, OF(ActSkirmishOpponent, allowInActs) },
	{ "IsPlayer", INI::parseBool, nullptr, OF(ActSkirmishOpponent, isPlayer) },
	{ "TeamID", INI::parseInt, nullptr, OF(ActSkirmishOpponent, teamId) },
	{ nullptr, nullptr, nullptr, 0 },
};
void parseOpponent(INI *ini, void *instance, void *, const void *)
{
	ActSkirmishOpponent o;
	ini->initFromINI(&o, kOpponentFields);
	static_cast<ActRegion *>(instance)->opponents.push_back(std::move(o));
}
const FieldParse kRegionFields[] = {
	{ "MapName", INI::parseQuotedAsciiString, nullptr, OF(ActRegion, mapName) },
	{ "DisplayName", INI::parseAsciiString, nullptr, OF(ActRegion, displayName) },
	{ "ConqueredNotice", INI::parseAsciiString, nullptr, OF(ActRegion, conqueredNotice) },
	{ "MovieNameFirstTime", INI::parseAsciiString, nullptr, OF(ActRegion, movieNameFirstTime) },
	{ "MovieNameRepeat", INI::parseAsciiString, nullptr, OF(ActRegion, movieNameRepeat) },
	{ "SkirmishStillImage", INI::parseAsciiString, nullptr, OF(ActRegion, skirmishStillImage) },
	{ "SkirmishVoiceTrack", INI::parseAsciiString, nullptr, OF(ActRegion, skirmishVoiceTrack) },
	{ "SkirmishMusicTrack", INI::parseAsciiString, nullptr, OF(ActRegion, skirmishMusicTrack) },
	{ "SubObject", INI::parseAsciiString, nullptr, OF(ActRegion, subObject) },
	{ "RegionBonus", INI::parseAsciiString, nullptr, OF(ActRegion, regionBonus) }, // 0x431B1A : le nom, seules les campagnes de démonstration l'emploient
	{ "SkirmishOpponent", parseOpponent, nullptr, 0 },
	{ "ConnectsTo", parseStringsOfLine, nullptr, OF(ActRegion, connectsTo) },
	{ "OnWinActSubroutine", INI::parseAsciiString, nullptr, OF(ActRegion, onWinActSubroutine) },
	{ "RegionPortrait", INI::parseAsciiString, nullptr, OF(ActRegion, regionPortrait) },
	{ "ArmyPlacementPos", parseCoord2DAppend, nullptr, OF(ActRegion, armyPlacementPos) },
	{ "UnpackCamps", INI::parseBool, nullptr, OF(ActRegion, unpackCamps) },
	{ "MissionObjectiveTag", parseStringAppend, nullptr, OF(ActRegion, missionObjectiveTags) },
	{ "BonusMissionObjectiveTag", parseStringAppend, nullptr, OF(ActRegion, bonusMissionObjectiveTags) },
	{ "CustomCenterPoint", INI::parseBool, nullptr, OF(ActRegion, customCenterPoint) },
	{ "CenterPoint", INI::parseCoord2D, nullptr, OF(ActRegion, centerPoint) },
	{ "ArmyBonus", INI::parseInt, nullptr, OF(ActRegion, armyBonus) },
	{ "LegendaryBonus", INI::parseInt, nullptr, OF(ActRegion, legendaryBonus) },
	{ "ResourceBonus", INI::parseInt, nullptr, OF(ActRegion, resourceBonus) },
	{ "EndOfCampaign", INI::parseBool, nullptr, OF(ActRegion, endOfCampaign) },
	{ "DisplayActNum", INI::parseInt, nullptr, OF(ActRegion, displayActNum) },
	{ "CustomUIPopupPoint", INI::parseBool, nullptr, OF(ActRegion, customUIPopupPoint) },
	{ "UIPopupPoint", INI::parseCoord2D, nullptr, OF(ActRegion, uiPopupPoint) },
	{ "DifficultyText", INI::parseAsciiString, nullptr, OF(ActRegion, difficultyText) },
	{ nullptr, nullptr, nullptr, 0 },
};

// ---- LivingWorldRegionCampaign (0x10EDC48), l'effet de région (0x10ED888) ----
void parseEffectControlPoint(INI *ini, void *instance, void *, const void *)
{
	ActControlPoint c;
	ini->initFromINI(&c, kControlPointFields);
	static_cast<ActRegionEffect *>(instance)->points.push_back(c);
}
const FieldParse kEffectFields[] = {
	{ "Opacity", INI::parseReal, nullptr, OF(ActRegionEffect, opacity) },
	{ "ControlPoint", parseEffectControlPoint, nullptr, 0 },
	{ "Geometry", parseStringAppend, nullptr, OF(ActRegionEffect, geometry) },
	{ nullptr, nullptr, nullptr, 0 },
};
void parseEffect(INI *ini, void *instance, void *, const void *userData)
{
	ActRegionEffect effect;
	ini->initFromINI(&effect, kEffectFields);
	static_cast<ActRegionCampaign *>(instance)->effects[static_cast<const char *>(userData)] = std::move(effect);
}
void parseRegion(INI *ini, void *instance, void *, const void *)
{
	ActRegion region;
	const char *name = ini->getNextTokenOrNull();
	if (!name)
	{
		throw INIException(3, "LivingWorldRegionCampaign: Region name expected");
	}
	region.name = name;
	ini->initFromINI(&region, kRegionFields);
	auto *campaign = static_cast<ActRegionCampaign *>(instance);
	if (campaign->findRegion(region.name))
	{
		throw INIException(3, "LivingWorldRegionCampaign %s: Region %s defined twice", campaign->name.c_str(), region.name.c_str());
	}
	campaign->regions.push_back(std::move(region));
}
#define EFFECT(name) { name, parseEffect, name, 0 }
const FieldParse kRegionCampaignFields[] = {
	{ "Region", parseRegion, nullptr, 0 },
	{ "RegionObject", INI::parseAsciiString, nullptr, OF(ActRegionCampaign, regionObject) },
	{ "ZOffset", INI::parseReal, nullptr, OF(ActRegionCampaign, zOffset) },
	{ "RegionBonusArmy", INI::parseAsciiString, nullptr, OF(ActRegionCampaign, regionBonusArmy) },
	{ "RegionBonusResource", INI::parseAsciiString, nullptr, OF(ActRegionCampaign, regionBonusResource) },
	{ "RegionBonusLegendary", INI::parseAsciiString, nullptr, OF(ActRegionCampaign, regionBonusLegendary) },
	{ "SmallArmyCommandPoints", INI::parseInt, nullptr, OF(ActRegionCampaign, smallArmyCommandPoints) },
	{ "MediumArmyCommandPoints", INI::parseInt, nullptr, OF(ActRegionCampaign, mediumArmyCommandPoints) },
	{ "ArmyPlacementPos", parseCoord2DAppend, nullptr, OF(ActRegionCampaign, armyPlacementPos) },
	EFFECT("ConqueredEffectEvenglow"),
	EFFECT("ConqueredEffectFlareup"),
	EFFECT("MouseoverEffectFlareupOwned"),
	EFFECT("MouseoverEffectFlareupContested"),
	EFFECT("MouseoutEffectFlareupOwned"),
	EFFECT("MouseoutEffectFlareupContested"),
	EFFECT("EnemyBordersEffect"),
	EFFECT("FriendlyBordersEffect"),
	EFFECT("HilightBordersEffect"),
	{ "RegionPopupDefaultColor", INI::parseRGBAColorInt, nullptr, OF(ActRegionCampaign, regionPopupDefaultColor) },
	{ "RegionPopupOverColor", INI::parseRGBAColorInt, nullptr, OF(ActRegionCampaign, regionPopupOverColor) },
	{ "RegionConqueredSound", INI::parseAsciiString, nullptr, OF(ActRegionCampaign, regionConqueredSound) },
	{ nullptr, nullptr, nullptr, 0 },
};
#undef EFFECT

// ---- LivingWorldArmyIcon (0x1115DC8), objet d'icône (0x1115CC0) ----
const FieldParse kIconObjectFields[] = {
	{ "Model", INI::parseAsciiString, nullptr, OF(ActIconObject, model) },
	{ "Pickbox", INI::parseAsciiString, nullptr, OF(ActIconObject, pickbox) },
	{ "ZOffset", INI::parseReal, nullptr, OF(ActIconObject, zOffset) },
	{ "Scale", INI::parseReal, nullptr, OF(ActIconObject, scale) },
	{ "Clickable", INI::parseBool, nullptr, OF(ActIconObject, clickable) },
	{ "Hidden", INI::parseBool, nullptr, OF(ActIconObject, hidden) },
	{ "CanFade", INI::parseBool, nullptr, OF(ActIconObject, canFade) },
	{ "OrientAngle", INI::parseReal, nullptr, OF(ActIconObject, orientAngle) },
	{ nullptr, nullptr, nullptr, 0 },
};
void parseIconObject(INI *ini, void *instance, void *, const void *)
{
	ActIconObject object;
	object.name = ini->getNextToken();
	ini->initFromINI(&object, kIconObjectFields);
	static_cast<ActArmyIcon *>(instance)->objects.push_back(std::move(object));
}
const FieldParse kArmyIconFields[] = {
	{ "Object", parseIconObject, nullptr, 0 },
	{ "OnSelectedSound", INI::parseAsciiString, nullptr, OF(ActArmyIcon, onSelectedSound) },
	{ "OnMoveSound", INI::parseAsciiString, nullptr, OF(ActArmyIcon, onMoveSound) },
	{ nullptr, nullptr, nullptr, 0 },
};

// ---- LivingWorldMapInfo (0x1116000) ----
#define NOTREAD(name) { name, parseMapInfoNotRead, name, 0 }
void parseEyeTowerMapInfo(INI *ini, void *instance, void *, const void *);
const FieldParse kMapInfoFields[] = {
	{ "MapObject", INI::parseAsciiString, nullptr, OF(ActMapInfo, mapObject) },
	{ "CloudBorderSubObject", INI::parseAsciiString, nullptr, OF(ActMapInfo, cloudBorderSubObject) },
	{ "TextLayerSubObject", INI::parseQuotedAsciiString, nullptr, OF(ActMapInfo, textLayerSubObject) },
	{ "Center", INI::parseCoord2D, nullptr, OF(ActMapInfo, center) },
	{ "Extent", INI::parseCoord2D, nullptr, OF(ActMapInfo, extent) },
	{ "Ambient", INI::parseRGBColor, nullptr, OF(ActMapInfo, ambient) },
	{ "SunDir", INI::parseCoord2D, nullptr, OF(ActMapInfo, sunDir) },
	{ "SunRGB", INI::parseRGBColor, nullptr, OF(ActMapInfo, sunRGB) },
	{ "Accent1Dir", INI::parseCoord2D, nullptr, OF(ActMapInfo, accent1Dir) },
	{ "Accent1RGB", INI::parseRGBColor, nullptr, OF(ActMapInfo, accent1RGB) },
	{ "Accent2Dir", INI::parseCoord2D, nullptr, OF(ActMapInfo, accent2Dir) },
	{ "Accent2RGB", INI::parseRGBColor, nullptr, OF(ActMapInfo, accent2RGB) },
	{ "BattleMarker", INI::parseAsciiString, nullptr, OF(ActMapInfo, battleMarker) },
	{ "PalantirMarker", INI::parseAsciiString, nullptr, OF(ActMapInfo, palantirMarker) },
	{ "BattleMarkerCreatedSound", INI::parseAsciiString, nullptr, OF(ActMapInfo, battleMarkerCreatedSound) },
	{ "EnterMapSound", INI::parseAsciiString, nullptr, OF(ActMapInfo, enterMapSound) },
	{ "BannerHeight", INI::parseReal, nullptr, OF(ActMapInfo, bannerHeight) },
	{ "ArmyHeight", INI::parseReal, nullptr, OF(ActMapInfo, armyHeight) },
	{ "CameraBoundX", INI::parseReal, nullptr, OF(ActMapInfo, cameraBoundX) },
	{ "CameraBoundY", INI::parseReal, nullptr, OF(ActMapInfo, cameraBoundY) },
	NOTREAD("EmbersPartSys"), NOTREAD("CloudPos"), NOTREAD("GondorBanner"), NOTREAD("RohanBanner"), NOTREAD("MordorBanner"), NOTREAD("IsengardBanner"),
	NOTREAD("NeutralBanner"), NOTREAD("GondorAnts"), NOTREAD("RohanAnts"), NOTREAD("MordorAnts"), NOTREAD("IsengardAnts"), NOTREAD("NeutralAnts"),
	NOTREAD("BannerScaleSpeed"), NOTREAD("BannerMaxScale"), NOTREAD("BannerTiltAngle"), NOTREAD("BeaconHeight"), NOTREAD("AnimRays"), NOTREAD("AnimRaysPartSys"),
	NOTREAD("AnimRaysPartSysOffset"), NOTREAD("AnimRaysColor"), NOTREAD("AnimRaysColorScale"), NOTREAD("AnimRaysEffectShells"), NOTREAD("AnimRaysEffectDiameter"),
	NOTREAD("AnimRaysEffectLifetime"), NOTREAD("AnimRaysCreateSound"), NOTREAD("AnimCloud"), NOTREAD("AnimCloudPartSys"), NOTREAD("NumAnimClouds"),
	NOTREAD("AnimCloudLifetime"), NOTREAD("AnimCloudRegionMin"), NOTREAD("AnimCloudRegionMax"), NOTREAD("MordorCloud"), NOTREAD("CloudGrowthPos"),
	NOTREAD("CloudGrowthRate"), NOTREAD("CloudInitialSize"), NOTREAD("CloudGrowthSize"), NOTREAD("CloudInitialOpacity"), NOTREAD("ShadowColor"),
	NOTREAD("EnableMapShadows"), NOTREAD("ArmySelectedIconObject"), NOTREAD("ArmyHilightedIconObject"), NOTREAD("ArmySelectedFadeInStart"),
	NOTREAD("ArmySelectedFadeInEnd"), NOTREAD("ArmySelectedFadeOutStart"), NOTREAD("ArmySelectedFadeOutEnd"), NOTREAD("ArmyHilightedFadeInTime"),
	NOTREAD("ArmyHilightedFadeOutTime"), NOTREAD("ArmySoldierLarge"), NOTREAD("ArmySoldierMedium"), NOTREAD("ArmySoldierSmall"),
	{ "EyeTower", parseEyeTowerMapInfo, nullptr, 0 },
	{ nullptr, nullptr, nullptr, 0 },
};
#undef NOTREAD
void parseEyeTowerMapInfo(INI *ini, void *instance, void *, const void *)
{
	static const FieldParse kFields[] = {
		{ "PupilAnimObject", parseMapInfoNotRead, "EyeTower.PupilAnimObject", 0 },
		{ "PupilBeamAnimObject", parseMapInfoNotRead, "EyeTower.PupilBeamAnimObject", 0 },
		{ "EyeDecalAnimObject", parseMapInfoNotRead, "EyeTower.EyeDecalAnimObject", 0 },
		{ "EyeDecalBeamAnimObject", parseMapInfoNotRead, "EyeTower.EyeDecalBeamAnimObject", 0 },
		{ nullptr, nullptr, nullptr, 0 },
	};
	ini->initFromINI(instance, kFields);
}

#undef OF

template <typename T>
const T *findNamed(const std::vector<T> &all, const std::string &name)
{
	for (const T &one : all)
	{
		if (one.name == name)
		{
			return &one;
		}
	}
	return nullptr;
}

} // namespace

int ActCampaignData::actIndex(const std::string &act) const
{
	for (size_t i = 0; i < acts.size(); ++i)
	{
		if (acts[i].name == act)
		{
			return (int)i;
		}
	}
	return -1;
}

const ActRegion *ActRegionCampaign::findRegion(const std::string &region) const { return findNamed(regions, region); }
const ActCampaignData *ActCampaignStores::findCampaign(const std::string &name) const { return findNamed(campaigns, name); }
const ActRegionCampaign *ActCampaignStores::findRegionCampaign(const std::string &name) const { return findNamed(regionCampaigns, name); }
const ActPlayerArmy *ActCampaignStores::findPlayerArmy(const std::string &name) const { return findNamed(playerArmies, name); }
const ActArmyIcon *ActCampaignStores::findArmyIcon(const std::string &name) const { return findNamed(armyIcons, name); }

bool ActCampaignStores::load(ArchiveFileSystem &fs, std::string *error)
{
	campaigns.clear();
	regionCampaigns.clear();
	playerArmies.clear();
	armyIcons.clear();
	mapInfo = ActMapInfo();

	INIEnvironment env;
	env.fileSystem = &fs;
	INIBlockRecorder recorder;
	RegisterRecordingBlockStubs(env.blocks, recorder, { "LivingWorldCampaign", "LivingWorldRegionCampaign", "LivingWorldPlayerArmy", "LivingWorldMapInfo", "LivingWorldArmyIcon" },
		StubExtent::Lenient);
	env.blocks.registerBlock("LivingWorldCampaign", [this](INI *ini) {
		ActCampaignData campaign;
		const char *name = ini->getNextTokenOrNull();
		if (!name)
		{
			throw INIException(3, "ParseLivingWorldCampaignBlock::Campaign name expected"); // INILivingWorldCampaign.cpp, 0x7AA860
		}
		campaign.name = name;
		ini->initFromINI(&campaign, kCampaignFields);
		// LivingWorldCampaign.cpp 0x7A73C0 (indexMissions)
		campaign.lastAct = (int)campaign.acts.size() - 1;
		for (size_t i = 0; i < campaign.acts.size(); ++i)
		{
			if (campaign.acts[i].endAct)
			{
				campaign.lastAct = (int)i;
				break;
			}
		}
		campaigns.push_back(std::move(campaign));
	});
	env.blocks.registerBlock("LivingWorldRegionCampaign", [this](INI *ini) {
		ActRegionCampaign campaign;
		const char *name = ini->getNextTokenOrNull();
		if (!name)
		{
			throw INIException(3, "LivingWorldRegionCampaign: name expected");
		}
		campaign.name = name;
		ini->initFromINI(&campaign, kRegionCampaignFields);
		regionCampaigns.push_back(std::move(campaign));
	});
	env.blocks.registerBlock("LivingWorldPlayerArmy", [this](INI *ini) {
		ActPlayerArmy army;
		ini->initFromINI(&army, kPlayerArmyFields);
		playerArmies.push_back(std::move(army));
	});
	env.blocks.registerBlock("LivingWorldMapInfo", [this](INI *ini) {
		ini->initFromINI(&mapInfo, kMapInfoFields);
		mapInfo.loaded = true;
	});
	env.blocks.registerBlock("LivingWorldArmyIcon", [this](INI *ini) {
		ActArmyIcon icon;
		const char *name = ini->getNextTokenOrNull();
		if (!name)
		{
			throw INIException(3, "LivingWorldArmyIcon: name expected");
		}
		icon.name = name;
		ini->initFromINI(&icon, kArmyIconFields);
		armyIcons.push_back(std::move(icon));
	});

	// Les macros de région (DEFAULT_REGION_RESOURCE_BONUS...) viennent de GameData.ini, que TheWritableGlobalData lit en premier (SubsystemLegend.ini) ;
	// la table des macros est commune à tous les fichiers : son prétraitement suffit
	static const char *const kGameData = "Data\\INI\\GameData.ini";
	if (!fs.doesFileExist(kGameData))
	{
		if (error)
		{
			*error = std::string("file not found in any mounted archive: ") + kGameData;
		}
		return false;
	}
	{
		INI ini(env);
		ini.preprocessFile(kGameData, INI_LOAD_OVERWRITE);
	}

	static const char *const kFiles[] = { "Data\\INI\\LivingWorld.ini", "Data\\INI\\Campaigns\\GondorCampaign.ini", "Data\\INI\\Campaigns\\MordorCampaign.ini",
		"Data\\INI\\Campaigns\\DemoCampaigns.ini" };
	for (const char *file : kFiles)
	{
		if (!fs.doesFileExist(file))
		{
			if (error)
			{
				*error = std::string("file not found in any mounted archive: ") + file;
			}
			return false;
		}
		try
		{
			INI ini(env);
			ini.load(file, INI_LOAD_OVERWRITE);
		}
		catch (const INIException &e)
		{
			if (error)
			{
				*error = std::string(file) + ": " + e.what();
			}
			return false;
		}
	}
	if (!mapInfo.loaded)
	{
		if (error)
		{
			*error = "Data\\INI\\LivingWorld.ini has no LivingWorldMapInfo block";
		}
		return false;
	}
	return true;
}

const std::string &ActCampaignLogic::actName() const { return m_campaign->acts[m_actIndex].name; }

bool ActCampaignLogic::start(const ActCampaignStores &stores, const std::string &campaign, std::string *error)
{
	const ActCampaignData *data = stores.findCampaign(campaign);
	const ActRegionCampaign *regions = stores.findRegionCampaign(campaign);
	if (!data || !regions || data->acts.empty())
	{
		if (error)
		{
			*error = "no playable campaign named '" + campaign + "' (acts and regions)";
		}
		return false;
	}
	*this = ActCampaignLogic();
	m_stores = &stores;
	m_campaign = data;
	m_regions = regions;
	enterAct(data->acts[0], 0);
	return true;
}

// 0x7AEE70 (+0x08 régions, +0x14 batailles, +0x20 créations, +0x2C marches, +0x38 retraits, +0x54 contrôle, +0x6C fusions) ; dans un même vecteur, l'ordre du fichier
void ActCampaignLogic::enterAct(const CampaignAct &act, int depth)
{
	for (const std::string &region : act.enableRegions)
	{
		if (!m_regions->findRegion(region))
		{
			m_errors.push_back("act " + act.name + ": EnableRegion names an unknown region " + region);
		}
		else if (m_conquered.count(region) == 0 && std::find(m_enabled.begin(), m_enabled.end(), region) == m_enabled.end())
		{
			m_enabled.push_back(region);
		}
	}
	for (const ActForceBattle &battle : act.forceBattles)
	{
		if (battle.removeEvent)
		{
			m_forced.erase(std::remove_if(m_forced.begin(), m_forced.end(), [&](const ActForceBattle &b) { return b.eventName == battle.eventName; }), m_forced.end());
		}
		else
		{
			m_forced.push_back(battle);
		}
	}
	for (const ActSpawnArmy &spawn : act.spawnArmies)
	{
		if (!spawn.playerArmy.empty() && !m_stores->findPlayerArmy(spawn.playerArmy))
		{
			m_errors.push_back("act " + act.name + ": army " + spawn.name + " names an unknown LivingWorldPlayerArmy " + spawn.playerArmy);
		}
		if (findArmy(spawn.name))
		{
			m_errors.push_back("act " + act.name + ": army " + spawn.name + " is spawned twice");
			continue;
		}
		ActArmy army;
		army.name = spawn.name;
		army.icon = spawn.icon;
		army.banner = spawn.banner;
		army.playerArmy = spawn.playerArmy;
		army.palantirMovie = spawn.palantirMovie;
		army.iconSize = spawn.iconSize;
		army.faction = spawn.faction;
		army.position = spawn.position;
		army.playerControlled = spawn.playerControlled;
		army.playerOwned = spawn.playerOwned;
		army.isCity = spawn.isCity;
		army.speedPerFrame = kInitialSpeedPerFrame;
		m_armies.push_back(std::move(army));
	}
	for (const ActMoveArmy &move : act.moveArmies) // +0x2C, 0x7C0250 -> 0x7A58E0
	{
		if (ActArmy *army = findArmy(move.name))
		{
			ActArmy::Move m;
			m.to = move.moveTo;
			m.speed = move.moveSpeed;
			m.palantirMovie = move.palantirMovie;
			m.iconSize = move.iconSize;
			army->moves.push_back(std::move(m));
			if (move.playNextActAfterMove)
			{
				army->playNextAct = true; // 0x7C028E : le drapeau est celui de l'armée (+0x1F), lu à la fin de toute sa file
			}
		}
		else
		{
			m_errors.push_back("act " + act.name + ": MoveArmy names an unknown army " + move.name);
		}
	}
	for (const std::string &gone : act.despawnArmies)
	{
		const size_t before = m_armies.size();
		m_armies.erase(std::remove_if(m_armies.begin(), m_armies.end(), [&](const ActArmy &a) { return a.name == gone; }), m_armies.end());
		if (m_armies.size() == before)
		{
			m_errors.push_back("act " + act.name + ": DespawnArmy names an unknown army " + gone);
		}
	}
	for (const ActToggleArmyControl &toggle : act.toggleArmyControls)
	{
		if (ActArmy *army = findArmy(toggle.name))
		{
			army->playerControlled = toggle.playerControl;
		}
		else
		{
			m_errors.push_back("act " + act.name + ": ToggleArmyControl names an unknown army " + toggle.name);
		}
	}
	for (const ActMergeArmy &merge : act.mergeArmies)
	{
		// INFÉRENCE : la fusion et la scission (0x7B4510) changent ce que contiennent les armées ; seule la disparition de l'armée source fusionnée
		// se voit sur la carte. La composition sera portée avec les armées qui entrent en mission.
		if (!merge.splitArmy)
		{
			m_armies.erase(std::remove_if(m_armies.begin(), m_armies.end(), [&](const ActArmy &a) { return a.name == merge.sourceArmy; }), m_armies.end());
		}
	}
	auto schedule = [&](int delayMs, ActCommand command) {
		m_timed.push_back({ m_frame + delayMs * FRAMES_PER_SECOND / 1000, std::move(command) });
	};
	for (const ActMoveCamera &camera : act.moveCameras)
	{
		ActCommand c;
		c.type = ActCommand::MOVE_CAMERA;
		c.position = camera.position;
		c.angle = camera.viewAngle;
		c.durationMs = camera.scrollTimeMs;
		c.summaryEvent = camera.summaryEvent;
		schedule(camera.delayMs, std::move(c));
	}
	for (const ActSplineCamera &spline : act.splineCameras)
	{
		ActCommand c;
		c.type = ActCommand::SPLINE_CAMERA;
		c.spline = spline.points;
		c.summaryEvent = spline.summaryEvent;
		schedule(spline.delayMs, std::move(c));
	}
	for (const ActWorldText &text : act.worldTexts)
	{
		ActCommand c;
		c.type = ActCommand::WORLD_TEXT;
		c.text = text.stringTag;
		c.durationMs = text.durationMs;
		c.summaryEvent = text.summaryEvent;
		schedule(text.delayMs, std::move(c));
	}
	for (const ActAudioEvent &audio : act.audioEvents)
	{
		ActCommand c;
		c.type = ActCommand::AUDIO;
		c.text = audio.sound;
		c.summaryEvent = audio.summaryEvent;
		schedule(audio.delayMs, std::move(c));
	}
	for (const std::vector<Coord2D> &points : act.eyeTowerPoints)
	{
		ActCommand c;
		c.type = ActCommand::EYE_TOWER;
		c.lookPoints = points;
		schedule(0, std::move(c));
	}
	for (const ActUpdateAnimObject &anim : act.updateAnimObjects)
	{
		ActCommand c;
		c.type = ActCommand::UPDATE_ANIM;
		c.text = anim.animObjectName;
		c.position = anim.worldPos;
		schedule(0, std::move(c));
	}
	for (const std::string &sub : act.callActSubroutines) // 0x7AEF20
	{
		const int index = m_campaign->actIndex(sub);
		if (index < 0 || depth > 16)
		{
			m_errors.push_back("act " + act.name + ": CallActSubroutine names an unknown act " + sub);
			continue;
		}
		enterAct(m_campaign->acts[index], depth + 1);
	}
	if (!act.jumpToAct.empty())
	{
		// La ligne existe dans la table (+0x50) mais aucune campagne de BFME1 ne l'emploie : son sens n'est pas lu, donc une erreur, pas un saut
		m_errors.push_back("act " + act.name + ": JumpToAct is not ported");
	}
}

void ActCampaignLogic::stepAct()
{
	if (m_actIndex >= m_campaign->lastAct)
	{
		m_actIndex = m_campaign->lastAct; // 0x7A7320 : ramené à la limite, rien n'est entré
		return;
	}
	++m_actIndex;
	enterAct(m_campaign->acts[m_actIndex], 0);
}

void ActCampaignLogic::update()
{
	++m_frame;
	// Les événements de cette image, dans l'ordre de leur programmation
	for (size_t i = 0; i < m_timed.size();)
	{
		if (m_timed[i].frame <= m_frame)
		{
			m_commands.push_back(std::move(m_timed[i].command));
			m_timed.erase(m_timed.begin() + (std::ptrdiff_t)i);
		}
		else
		{
			++i;
		}
	}
	// 0x7A51D0, une armée par image. Avancer d'un acte peut créer ou retirer des armées : la fin de marche est notée, puis traitée après la boucle
	int stepsToPlay = 0;
	for (ActArmy &army : m_armies)
	{
		auto popMove = [&army]() { // 0x7A4E30
			if (army.moves.empty())
			{
				return false;
			}
			const ActArmy::Move &next = army.moves.front();
			army.target = next.to;
			if (!next.palantirMovie.empty())
			{
				army.palantirMovie = next.palantirMovie;
			}
			if (!next.iconSize.empty())
			{
				army.iconSize = next.iconSize;
			}
			if (next.speed != 0.0f)
			{
				army.speedPerFrame = next.speed * kMoveSpeedScale;
			}
			army.moves.erase(army.moves.begin());
			army.moving = true;
			return true;
		};
		if (!army.moving)
		{
			popMove();
		}
		const bool was = army.moving;
		if (army.moving)
		{
			// 0x7A50F0 : le pas est le minimum de la vitesse et de la distance restante (0x7A40A0)
			float dx = army.target.x - army.position.x, dy = army.target.y - army.position.y;
			const float distance = std::sqrt(dx * dx + dy * dy);
			if (distance > 0.0f)
			{
				const float stride = std::min(army.speedPerFrame, distance);
				army.position.x += dx / distance * stride;
				army.position.y += dy / distance * stride;
				dx = army.target.x - army.position.x;
				dy = army.target.y - army.position.y;
			}
			if (std::sqrt(dx * dx + dy * dy) < kArrivalDistance && !popMove()) // 0x7A4FD0 : à l'arrivée, la ligne suivante part aussitôt
			{
				army.moving = false;
			}
		}
		if (was && !army.moving && army.playNextAct)
		{
			army.playNextAct = false;
			++stepsToPlay; // 0x7BEED0 : avance l'acte et finit le pas
		}
	}
	for (int i = 0; i < stepsToPlay; ++i)
	{
		stepAct();
	}
}

std::vector<ActCommand> ActCampaignLogic::takeCommands()
{
	std::vector<ActCommand> out;
	out.swap(m_commands);
	return out;
}

ActArmy *ActCampaignLogic::findArmy(const std::string &name)
{
	for (ActArmy &army : m_armies)
	{
		if (army.name == name)
		{
			return &army;
		}
	}
	return nullptr;
}

bool ActCampaignLogic::canEnterRegion(const std::string &region) const
{
	if (m_conquered.count(region))
	{
		return false;
	}
	if (std::find(m_enabled.begin(), m_enabled.end(), region) != m_enabled.end())
	{
		return true;
	}
	return std::any_of(m_forced.begin(), m_forced.end(), [&](const ActForceBattle &b) { return b.region == region; });
}

std::vector<ActSkirmishOpponent> ActCampaignLogic::opponentsOf(const std::string &region) const
{
	std::vector<ActSkirmishOpponent> out;
	if (const ActRegion *r = m_regions->findRegion(region))
	{
		for (const ActSkirmishOpponent &o : r->opponents)
		{
			if (o.allowInActs.empty() || std::find(o.allowInActs.begin(), o.allowInActs.end(), actName()) != o.allowInActs.end())
			{
				out.push_back(o);
			}
		}
	}
	return out;
}

std::string ActCampaignLogic::enterRegion(const std::string &region)
{
	const ActRegion *r = m_regions->findRegion(region);
	if (!r)
	{
		return std::string();
	}
	const bool first = m_visited.insert(region).second;
	return first ? r->movieNameFirstTime : r->movieNameRepeat;
}

bool ActCampaignLogic::regionWon(const std::string &region)
{
	const ActRegion *r = m_regions->findRegion(region);
	if (!r || !canEnterRegion(region))
	{
		return false;
	}
	m_conquered.insert(region);
	m_enabled.erase(std::remove(m_enabled.begin(), m_enabled.end(), region), m_enabled.end());
	m_forced.erase(std::remove_if(m_forced.begin(), m_forced.end(), [&](const ActForceBattle &b) { return b.region == region; }), m_forced.end());
	m_armyPoints += r->armyBonus;
	m_legendaryPoints += r->legendaryBonus; // [inférence : l'ajout d'ArmyBonus est lu (0x7BFB20), celui de LegendaryBonus suit le même modèle]
	if (r->endOfCampaign)
	{
		m_campaignWon = true;
	}
	if (!r->onWinActSubroutine.empty())
	{
		const int index = m_campaign->actIndex(r->onWinActSubroutine);
		if (index < 0)
		{
			m_errors.push_back("region " + region + ": OnWinActSubroutine names an unknown act " + r->onWinActSubroutine);
			return true;
		}
		enterAct(m_campaign->acts[index], 0);
		return true;
	}
	stepAct();
	return true;
}

} // namespace LivingWorld
