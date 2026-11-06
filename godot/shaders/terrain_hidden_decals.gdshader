// OpenBFME. GPL-3.0. Notre version, BFME1.
//
// TERRAIN_RENDER_DISABLE (0xB14405, docs/BFME1.md) saute le maillage du terrain, pas ce qui se projette dessus : le gestionnaire d'ombres projetées dessine
// ses décalques avec ses propres sommets, lus dans la carte des hauteurs (ZH RadiusDecal.cpp addDecal, W3DProjectedShadow.cpp queueDecal, flushDecals). À la Moria,
// où le sol est fait de modèles, le cercle de visée d'un pouvoir reste donc visible. Le port le compose dans terrain.gdshader (S-4640) : quand le terrain est
// masqué, ses morceaux portent ce matériau, qui ne montre que le cercle.
//
// INFÉRENCE : la cible teste la profondeur contre le sol en modèles avec un décalage que ce fichier ne reprend pas ; DECAL_LIFT rapproche le cercle de la caméra
// pour que les marches et les dalles, un peu au-dessus des hauteurs, ne le coupent pas.
//
// Passe transparente : les couleurs restent dans l'espace gamma (GodotGammaComposite.h).
shader_type spatial;
render_mode unshaded, blend_premul_alpha, depth_draw_never, cull_back, shadows_disabled, ambient_light_disabled, fog_disabled;

#include "res://shaders/radius_cursor.gdshaderinc"

const float DECAL_LIFT = 4.0;

varying vec2 v_sage_xy;

void vertex() {
	vec3 world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_sage_xy = vec2(world.x, -world.z);
	vec3 lifted = world + normalize(CAMERA_POSITION_WORLD - world) * DECAL_LIFT;
	// sans cercle, rien à dessiner : tous les sommets hors de l'écran
	POSITION = radius_cursor_transform.z > 0.0 ? PROJECTION_MATRIX * VIEW_MATRIX * vec4(lifted, 1.0) : vec4(2.0, 2.0, 2.0, 1.0);
}

void fragment() {
	vec4 cursor = radius_cursor_sample(v_sage_xy);
	ALBEDO = cursor.rgb * cursor.a;
	ALPHA = radius_cursor_params.y > 0.5 ? cursor.a : 0.0; // le style additif n'efface rien de ce qui est dessous
}
