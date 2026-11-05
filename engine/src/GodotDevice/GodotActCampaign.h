// OpenBFME. GPL-3.0.
//
// ActCampaignGame : la campagne par actes de BFME1 (GameLogic/LivingWorld/ActCampaign.h) en données Godot, pour godot/scripts/act_campaign.gd.
// La règle est celle de la logique ; cette classe ne fait que traduire. Modèle : GodotLivingWorld (la Guerre de l'Anneau de RotWK).

#pragma once

#include "GameLogic/LivingWorld/ActCampaign.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot
{

class ActCampaignGame : public RefCounted
{
	GDCLASS(ActCampaignGame, RefCounted)

public:
	// Les blocs des fichiers de campagne du monde chargé -> { ok, error, map_info, campaigns, army_icons }
	Dictionary setup(Object *game_world);
	// -> { ok, error, region_object, z_offset, conquered_sound, popup_default_color, popup_over_color, effects, regions: [region_info] }
	Dictionary start(const String &campaign);
	void update();
	Array take_commands();
	// { act, act_index, frame, army_points, legendary_points, won, armies, enabled, conquered, forced, errors }
	Dictionary state() const;
	// { name, map, display_name, sub_object, movie_first, movie_repeat, still, voice, music, objectives, bonus_objectives, conquered_notice, ... }
	Dictionary region_info(const String &region) const;
	bool can_enter_region(const String &region) const;
	String enter_region(const String &region); // le film de cette entrée
	bool region_won(const String &region);
	Dictionary apply_map_lighting() const; // LivingWorldMapInfo -> le jeu de lumières des shaders W3D (comme LivingWorldGame)

protected:
	static void _bind_methods();

private:
	LivingWorld::ActCampaignStores m_stores;
	LivingWorld::ActCampaignLogic m_logic;
	bool m_setup = false;
};

} // namespace godot
