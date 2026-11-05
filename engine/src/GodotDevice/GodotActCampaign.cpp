// OpenBFME. GPL-3.0.
//
// Voir GodotActCampaign.h.

#include "GodotDevice/GodotActCampaign.h"

#include "GameEngineDevice/W3DDevice/GameClient/W3DObjectLighting.h"
#include "GameLogic/Object/RetailObjectWorld.h"
#include "GodotDevice/GodotGameWorld.h"
#include "GodotDevice/GodotW3DMaterial.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot
{
namespace
{

String s(const std::string &text) { return String::utf8(text.c_str()); }
std::string n(const String &text) { return std::string(text.utf8().get_data()); }
Vector2 v2(const Coord2D &c) { return Vector2(c.x, c.y); }
Vector3 v3(const Coord3D &c) { return Vector3(c.x, c.y, c.z); }

PackedStringArray strings(const std::vector<std::string> &all)
{
	PackedStringArray out;
	for (const std::string &one : all)
	{
		out.push_back(s(one));
	}
	return out;
}

Array controlPoints(const std::vector<LivingWorld::ActControlPoint> &points)
{
	Array out;
	for (const LivingWorld::ActControlPoint &p : points)
	{
		Dictionary d;
		d["position"] = v3(p.position);
		d["angle"] = p.angle;
		d["time"] = p.timeMs / 1000.0;
		d["ease_in"] = p.easeIn;
		d["ease_out"] = p.easeOut;
		out.push_back(d);
	}
	return out;
}

} // namespace

void ActCampaignGame::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("setup", "game_world"), &ActCampaignGame::setup);
	ClassDB::bind_method(D_METHOD("start", "campaign"), &ActCampaignGame::start);
	ClassDB::bind_method(D_METHOD("update"), &ActCampaignGame::update);
	ClassDB::bind_method(D_METHOD("take_commands"), &ActCampaignGame::take_commands);
	ClassDB::bind_method(D_METHOD("state"), &ActCampaignGame::state);
	ClassDB::bind_method(D_METHOD("region_info", "region"), &ActCampaignGame::region_info);
	ClassDB::bind_method(D_METHOD("can_enter_region", "region"), &ActCampaignGame::can_enter_region);
	ClassDB::bind_method(D_METHOD("enter_region", "region"), &ActCampaignGame::enter_region);
	ClassDB::bind_method(D_METHOD("region_won", "region"), &ActCampaignGame::region_won);
	ClassDB::bind_method(D_METHOD("apply_map_lighting"), &ActCampaignGame::apply_map_lighting);
}

Dictionary ActCampaignGame::setup(Object *game_world)
{
	Dictionary out;
	out["ok"] = false;
	GameWorld *gw = Object::cast_to<GameWorld>(game_world);
	RetailObjectWorld *world = gw ? gw->object_world() : nullptr;
	if (!world)
	{
		out["error"] = "no GameWorld with a loaded object world";
		return out;
	}
	m_logic = LivingWorld::ActCampaignLogic();
	m_setup = false;
	std::string error;
	if (!m_stores.load(world->fileSystem(), &error))
	{
		out["error"] = s(error);
		return out;
	}
	m_setup = true;
	const LivingWorld::ActMapInfo &mi = m_stores.mapInfo;
	Dictionary map;
	map["map_object"] = s(mi.mapObject);
	map["cloud_border_sub_object"] = s(mi.cloudBorderSubObject);
	map["text_layer_sub_object"] = s(mi.textLayerSubObject);
	map["center"] = v2(mi.center);
	map["extent"] = v2(mi.extent);
	map["army_height"] = mi.armyHeight;
	map["banner_height"] = mi.bannerHeight;
	map["camera_bound_x"] = mi.cameraBoundX;
	map["camera_bound_y"] = mi.cameraBoundY;
	map["enter_map_sound"] = s(mi.enterMapSound);
	map["battle_marker"] = s(mi.battleMarker);
	PackedStringArray notRead;
	for (const std::string &field : mi.notRead)
	{
		notRead.push_back(s(field));
	}
	map["not_read"] = notRead;
	out["map_info"] = map;
	PackedStringArray names;
	for (const LivingWorld::ActCampaignData &c : m_stores.campaigns)
	{
		names.push_back(s(c.name));
	}
	out["campaigns"] = names;
	Dictionary icons;
	for (const LivingWorld::ActArmyIcon &icon : m_stores.armyIcons)
	{
		Dictionary d;
		d["on_selected_sound"] = s(icon.onSelectedSound);
		d["on_move_sound"] = s(icon.onMoveSound);
		Array objects;
		for (const LivingWorld::ActIconObject &o : icon.objects)
		{
			Dictionary od;
			od["name"] = s(o.name);
			od["model"] = s(o.model);
			od["z_offset"] = o.zOffset;
			od["scale"] = o.scale;
			od["orient_angle"] = o.orientAngle;
			od["hidden"] = o.hidden;
			objects.push_back(od);
		}
		d["objects"] = objects;
		icons[s(icon.name)] = d;
	}
	out["army_icons"] = icons;
	out["ok"] = true;
	return out;
}

Dictionary ActCampaignGame::start(const String &campaign)
{
	Dictionary out;
	out["ok"] = false;
	if (!m_setup)
	{
		out["error"] = "ActCampaignGame.start before setup";
		return out;
	}
	std::string error;
	if (!m_logic.start(m_stores, n(campaign), &error))
	{
		out["error"] = s(error);
		return out;
	}
	const LivingWorld::ActRegionCampaign &rc = m_logic.regionCampaign();
	out["region_object"] = s(rc.regionObject);
	out["z_offset"] = rc.zOffset;
	out["conquered_sound"] = s(rc.regionConqueredSound);
	out["is_evil"] = m_logic.campaign().isEvil;
	Dictionary effects;
	for (const auto &[name, effect] : rc.effects)
	{
		Dictionary d;
		d["opacity"] = effect.opacity;
		d["geometry"] = strings(effect.geometry);
		d["points"] = controlPoints(effect.points);
		effects[s(name)] = d;
	}
	out["effects"] = effects;
	Array regions;
	for (const LivingWorld::ActRegion &r : rc.regions)
	{
		regions.push_back(region_info(s(r.name)));
	}
	out["regions"] = regions;
	out["ok"] = true;
	return out;
}

void ActCampaignGame::update()
{
	if (m_logic.started())
	{
		m_logic.update();
	}
}

Array ActCampaignGame::take_commands()
{
	Array out;
	if (!m_logic.started())
	{
		return out;
	}
	using LivingWorld::ActCommand;
	for (const ActCommand &c : m_logic.takeCommands())
	{
		Dictionary d;
		switch (c.type)
		{
		case ActCommand::WORLD_TEXT: d["type"] = "world_text"; break;
		case ActCommand::AUDIO: d["type"] = "audio"; break;
		case ActCommand::MOVE_CAMERA: d["type"] = "move_camera"; break;
		case ActCommand::SPLINE_CAMERA: d["type"] = "spline_camera"; break;
		case ActCommand::EYE_TOWER: d["type"] = "eye_tower"; break;
		case ActCommand::UPDATE_ANIM: d["type"] = "update_anim"; break;
		}
		d["text"] = s(c.text);
		d["duration"] = c.durationMs / 1000.0;
		d["position"] = v3(c.position);
		d["angle"] = c.angle;
		d["spline"] = controlPoints(c.spline);
		out.push_back(d);
	}
	return out;
}

Dictionary ActCampaignGame::state() const
{
	Dictionary out;
	if (!m_logic.started())
	{
		return out;
	}
	out["act"] = s(m_logic.actName());
	out["act_index"] = (int64_t)m_logic.actIndex();
	out["frame"] = (int64_t)m_logic.frame();
	out["army_points"] = (int64_t)m_logic.armyPoints();
	out["legendary_points"] = (int64_t)m_logic.legendaryPoints();
	out["won"] = m_logic.campaignWon();
	Array armies;
	for (const LivingWorld::ActArmy &a : m_logic.armies())
	{
		Dictionary d;
		d["name"] = s(a.name);
		d["icon"] = s(a.icon);
		d["icon_size"] = s(a.iconSize);
		d["x"] = a.position.x;
		d["y"] = a.position.y;
		d["owned"] = a.playerOwned;
		d["controlled"] = a.playerControlled;
		d["city"] = a.isCity;
		d["moving"] = a.moving;
		armies.push_back(d);
	}
	out["armies"] = armies;
	out["enabled"] = strings(m_logic.enabledRegions());
	PackedStringArray conquered;
	for (const std::string &r : m_logic.conqueredRegions())
	{
		conquered.push_back(s(r));
	}
	out["conquered"] = conquered;
	PackedStringArray forced;
	for (const LivingWorld::ActForceBattle &b : m_logic.forcedBattles())
	{
		forced.push_back(s(b.region));
	}
	out["forced"] = forced;
	out["errors"] = strings(m_logic.errors());
	return out;
}

Dictionary ActCampaignGame::region_info(const String &region) const
{
	Dictionary d;
	if (!m_logic.started())
	{
		return d;
	}
	const LivingWorld::ActRegion *r = m_logic.regionCampaign().findRegion(n(region));
	if (!r)
	{
		return d;
	}
	d["name"] = s(r->name);
	d["map"] = s(r->mapName);
	d["display_name"] = s(r->displayName);
	d["sub_object"] = s(r->subObject);
	d["movie_first"] = s(r->movieNameFirstTime);
	d["movie_repeat"] = s(r->movieNameRepeat);
	d["still"] = s(r->skirmishStillImage);
	d["voice"] = s(r->skirmishVoiceTrack);
	d["music"] = s(r->skirmishMusicTrack);
	d["objectives"] = strings(r->missionObjectiveTags);
	d["bonus_objectives"] = strings(r->bonusMissionObjectiveTags);
	d["conquered_notice"] = s(r->conqueredNotice);
	d["difficulty_text"] = s(r->difficultyText);
	d["center"] = v2(r->centerPoint);
	d["custom_center"] = r->customCenterPoint;
	d["end_of_campaign"] = r->endOfCampaign;
	Array slots;
	for (const LivingWorld::ActSkirmishSlot &o : m_logic.skirmishSlotsOf(r->name))
	{
		Dictionary od;
		od["slot"] = (int64_t)o.slot;
		od["name"] = s(o.name);
		od["faction"] = s(o.faction);
		od["start_position"] = (int64_t)o.startPosition;
		od["team"] = (int64_t)o.team;
		od["human"] = o.human;
		slots.push_back(od);
	}
	d["slots"] = slots; // à l'acte courant : à relire juste avant d'entrer dans la région
	if (const std::optional<LivingWorld::ActWalkOnArmy> walkOn = m_logic.walkOnArmyOf(r->name))
	{
		Dictionary wd;
		wd["name"] = s(walkOn->name);
		wd["player_army"] = s(walkOn->playerArmy);
		wd["survival_threshhold"] = (int64_t)walkOn->survivalThreshhold;
		wd["direction"] = v2(walkOn->direction);
		Array entries;
		for (const LivingWorld::ActArmyEntry &e : walkOn->entries)
		{
			Dictionary ed;
			ed["template"] = s(e.thingTemplate);
			ed["quantity"] = (int64_t)e.quantity;
			ed["default"] = e.isDefault;
			entries.push_back(ed);
		}
		wd["entries"] = entries;
		d["walk_on"] = wd;
	}
	return d;
}

bool ActCampaignGame::can_enter_region(const String &region) const
{
	return m_logic.started() && m_logic.canEnterRegion(n(region));
}

String ActCampaignGame::enter_region(const String &region)
{
	return m_logic.started() ? s(m_logic.enterRegion(n(region))) : String();
}

bool ActCampaignGame::region_won(const String &region)
{
	return m_logic.started() && m_logic.regionWon(n(region));
}

Dictionary ActCampaignGame::apply_map_lighting() const
{
	Dictionary out;
	if (!m_setup)
	{
		out["ok"] = false;
		out["error"] = "apply_map_lighting before setup";
		return out;
	}
	const LivingWorld::ActMapInfo &mi = m_stores.mapInfo;
	const float ambient[3] = { mi.ambient.red, mi.ambient.green, mi.ambient.blue };
	const float rgb[3][3] = { { mi.sunRGB.red, mi.sunRGB.green, mi.sunRGB.blue },
							  { mi.accent1RGB.red, mi.accent1RGB.green, mi.accent1RGB.blue },
							  { mi.accent2RGB.red, mi.accent2RGB.green, mi.accent2RGB.blue } };
	const float dirs[3][2] = { { mi.sunDir.x, mi.sunDir.y }, { mi.accent1Dir.x, mi.accent1Dir.y }, { mi.accent2Dir.x, mi.accent2Dir.y } };
	W3DObjectLighting env;
	W3DObjectLightingUtil::fromLivingWorldMap(ambient, rgb, dirs, env);
	W3D_Apply_Object_Lighting(env);
	out["ok"] = true;
	return out;
}

} // namespace godot
