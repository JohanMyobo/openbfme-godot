extends Node3D
## Notre version : l'écran du monde de la campagne de BFME1, déroulé par actes (la règle est dans GameLogic/LivingWorld/ActCampaign.h, ActCampaignGame la traduit).
##
## FAITS CIBLE (lotrbfme.exe, données des INI) : les armées (LivingWorldArmyIcon, position, IconSize), la carte LivingMap (MapObject), le modèle de région RegionObject
## et ses sous-objets SubObject, ZOffset, les effets de région (Geometry, couleur = Position des ControlPoint en fonction de Time), la caméra SplineCamera (Catmull-Rom,
## l'angle autour de Z interpolé, les temps absolus) et MoveCamera (Position, ViewAngle, ScrollTime), une région gagnée (la logique).
## FAITS DONNEUR : Open-BFME-1 pour les champs (voir ActCampaign.h).
## INFÉRENCES, de présentation : la hauteur et la distance de la caméra, le choix des effets selon l'état d'une région (active : EnemyBordersEffect ; gagnée :
## FriendlyBordersEffect ; survolée : MouseoverEffectFlareup...), le mélange additif de ces effets, la durée d'un WorldText sans Duration (aucune ligne n'en donne).
## S-5017, non portés : l'interface Apt de l'écran du monde (la fenêtre de région est provisoire), la tour de l'Œil (EyeTowerPoints), les objets animés (UpdateAnimObject),
## ease in / ease out de la caméra, les marqueurs de bataille, le déplacement des armées par le joueur, la sauvegarde de la campagne, ScoreScreen.apt.

const OverlayFx := preload("res://scripts/wotr_region_fx.gd")
const LOGIC_STEP := 1.0 / 30.0
const CAMERA_PITCH := deg_to_rad(65.0) # la même que l'écran du monde de RotWK (wotr.gd), INFÉRENCE pour BFME1
const CAMERA_DISTANCE := 1500.0        # INFÉRENCE
const TEXT_SECONDS := 10.0             # INFÉRENCE
const SOLDIERS_BY_SIZE := {"Small": "Soldiers_Small", "Medium": "Soldiers_Medium", "Large": "Soldiers"} # ArmySoldierLarge / Medium / Small
const SOLDIER_OBJECTS := ["Soldiers", "Soldiers_Medium", "Soldiers_Small"]

var game: Node
var acts: RefCounted
var campaign_name := ""
var difficulty := 1
var active := false
var auto_regions := 0 # --act-auto=N : joue seul N régions, puis quitte (les tests)
var shot_dir := ""    # --act-shots=DIR : des images de l'écran du monde (les tests)
var _shots_taken := 0
var _shot_at := 0.0

var info := {}       # ActCampaignGame.setup
var camp := {}       # ActCampaignGame.start
var map_root: Node3D
var instancer: Node
var builder: RefCounted
var overlay: Node3D
var camera: Camera3D
var cam_target := Vector3.ZERO
var cam_angle := 0.0
var cam_dist := CAMERA_DISTANCE

var _accum := 0.0
var _region_by_name := {}
var _meshes := {}    # le nom du modèle en minuscules -> { nom de région -> MeshInstance3D }
var _materials := {} # MeshInstance3D -> ShaderMaterial
var _hovered := ""
var _hover_time := 0.0
var _flare := {}     # région -> le temps écoulé de son effet de conquête
var _icons := {}     # nom d'armée -> { objects, instances: { nom d'objet -> { id, object } }, size, position }
var _icon_models := {} # nom de modèle -> le modèle du W3DInstancer
var _spline: Array = []
var _spline_time := 0.0
var _scroll := {}    # { from, to, from_angle, to_angle, time, duration }
var _unported := {}  # les stops déjà signalés
var _won_count := 0
var _done_frames := -1
var _auto_busy := false

var _ui: CanvasLayer
var _text_label: Label
var _text_left := 0.0
var _popup: PanelContainer
var _popup_title: Label
var _popup_body: Label
var _popup_region := ""


func begin(game_node: Node, campaign: String, diff: int) -> void:
	game = game_node
	campaign_name = campaign
	difficulty = diff
	acts = ClassDB.instantiate("ActCampaignGame")
	info = acts.setup(game._world)
	if not info.ok:
		game._fail("BFME1 campaign: " + str(info.error))
		return
	camp = acts.start(campaign)
	if not camp.ok:
		game._fail("BFME1 campaign: " + str(camp.error))
		return
	for r in camp.regions:
		_region_by_name[String(r.name)] = r
	print("ACTMAP start %s (difficulty %d): %d regions, map info not read: %s" % [campaign, diff, camp.regions.size(), str(info.map_info.not_read.size())])
	game._audio.stop_music(false)
	while game._shell.shell_stack().size() > 0:
		game._shell.shell_pop()
		game._shell.tick(0.033)
	game._shell_leave_for_game()
	if map_root == null:
		_build_map()
	_show_map()
	if not String(info.map_info.enter_map_sound).is_empty():
		game._audio.play_sound(String(info.map_info.enter_map_sound))
	_sync()


func _show_map() -> void:
	active = true
	visible = true
	_ui.visible = true
	_set_pick_bodies(true)
	var lit: Dictionary = acts.apply_map_lighting()
	if not lit.get("ok", false):
		game._fail("BFME1 campaign: " + str(lit.get("error", "")))
		return
	camera.current = true
	game._state = game.State.ACT_MAP


func _hide_map() -> void:
	active = false
	visible = false
	_ui.visible = false
	_set_pick_bodies(false)
	camera.current = false


func _w3d(x: float, y: float, z: float = 0.0) -> Vector3:
	return Vector3(x, z, -y) # W3D -> Godot, comme GodotW3DModelBuilder


func _build_map() -> void:
	builder = ClassDB.instantiate("W3DModelBuilder")
	map_root = Node3D.new()
	map_root.name = "LivingMap"
	add_child(map_root)
	var mi: Dictionary = info.map_info
	instancer = ClassDB.instantiate("W3DInstancer")
	map_root.add_child(instancer)
	var su: Dictionary = instancer.setup(game._fs)
	if not su.ok:
		game._fail("BFME1 campaign: W3DInstancer: " + JSON.stringify(su.errors))
		return
	var living: int = instancer.add_model(String(mi.map_object))
	if living < 0:
		game._fail("BFME1 campaign: the map model %s did not build" % mi.map_object)
		return
	var rep: Dictionary = instancer.get_model_report(living)
	if not rep.errors.is_empty():
		push_error("ACTMAP %s: %s" % [mi.map_object, JSON.stringify(rep.errors)])
	var cloud := String(mi.cloud_border_sub_object).to_upper()
	var text_layer := String(mi.text_layer_sub_object).to_upper()
	var hidden := PackedStringArray()
	var text_hidden := PackedStringArray()
	for so in rep.sub_objects:
		var sub := str(so.name).to_upper()
		var short := sub.get_slice(".", 1)
		if sub.contains("IMPASSABLE") or (not cloud.is_empty() and short == cloud): # comme l'écran du monde de RotWK (wotr.gd, S-1335)
			hidden.append(str(so.name))
		if not text_layer.is_empty() and short == text_layer:
			hidden.append(str(so.name))
		else:
			text_hidden.append(str(so.name))
	var ground: int = instancer.add_instance(living, Transform3D.IDENTITY)
	instancer.set_instance_hidden_subobjects(ground, hidden)
	if text_hidden.size() < rep.sub_objects.size():
		var names: int = instancer.add_instance(living, Transform3D.IDENTITY)
		instancer.set_instance_hidden_subobjects(names, text_hidden)
		instancer.set_instance_opacity(names, 0.2) # wotr.gd
	_build_regions()
	cam_target = _w3d(mi.center.x, mi.center.y)
	camera = Camera3D.new()
	camera.fov = 45
	camera.near = 5.0
	camera.far = 40000.0
	add_child(camera)
	_update_camera()
	_build_ui()


## Le modèle RegionObject et les modèles des effets (Geometry), un sous-objet par région ; le remplissage porte le corps de sélection
func _build_regions() -> void:
	overlay = OverlayFx.new() # pour son matériau overlay_material, seul
	map_root.add_child(overlay)
	overlay.position.y = float(camp.z_offset)
	var models := {}
	models[String(camp.region_object).to_lower()] = true
	for e in camp.effects.values():
		for g in e.geometry:
			models[String(g).to_lower()] = true
	var missing: Array = []
	for model_name in models.keys():
		var model: Node3D = builder.build_model(game._fs, model_name)
		var rep: Dictionary = builder.get_report()
		if model == null or not rep.get("errors", []).is_empty():
			game._fail("BFME1 campaign: the region model %s: %s" % [model_name, JSON.stringify(rep.get("errors", []))])
			return
		overlay.add_child(model)
		var by_name := {}
		for n in model.find_children("*", "MeshInstance3D", true, false):
			by_name[str(n.name).to_upper()] = n
			n.visible = false
		var prefix := str(model.name).to_upper() + "_"
		_meshes[model_name] = {}
		for r in camp.regions:
			var sub := String(r.sub_object).to_upper()
			if sub.is_empty():
				continue
			if not by_name.has(prefix + sub):
				missing.append("%s.%s (%s)" % [model_name, sub, r.name])
				continue
			var mesh: MeshInstance3D = by_name[prefix + sub]
			_meshes[model_name][String(r.name)] = mesh
			var src: Material = mesh.get_active_material(0)
			var tex: Texture2D = (src as BaseMaterial3D).albedo_texture if src is BaseMaterial3D else null
			var black := Color(0, 0, 0)
			var m: ShaderMaterial = overlay.overlay_material("blend_add", black, black, black, 1.0, tex, 2)
			mesh.material_override = m
			_materials[mesh] = m
	if not missing.is_empty():
		print("ACTMAP STOP regions without a sub-object: ", ", ".join(missing))
	var fill: Dictionary = _meshes.get(String(camp.region_object).to_lower(), {})
	for region_name in fill.keys():
		var mesh: MeshInstance3D = fill[region_name]
		var body := StaticBody3D.new()
		body.set_meta("region", region_name)
		var shape := CollisionShape3D.new()
		shape.shape = mesh.mesh.create_trimesh_shape()
		body.add_child(shape)
		mesh.add_child(body)


func _set_pick_bodies(on: bool) -> void:
	var fill: Dictionary = _meshes.get(String(camp.region_object).to_lower(), {})
	for mesh in fill.values():
		for b in mesh.get_children():
			if b is StaticBody3D:
				(b as StaticBody3D).process_mode = Node.PROCESS_MODE_INHERIT if on else Node.PROCESS_MODE_DISABLED
				for c in b.get_children():
					(c as CollisionShape3D).disabled = not on


func _build_ui() -> void:
	_ui = CanvasLayer.new()
	add_child(_ui)
	_text_label = Label.new()
	_text_label.set_meta("game_text", true)
	_text_label.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
	_text_label.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_text_label.grow_vertical = Control.GROW_DIRECTION_BEGIN
	_text_label.offset_bottom = -48
	_text_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_text_label.custom_minimum_size = Vector2(760, 0)
	_text_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_text_label.add_theme_font_size_override("font_size", 20)
	_text_label.add_theme_color_override("font_outline_color", Color.BLACK)
	_text_label.add_theme_constant_override("outline_size", 6)
	_text_label.visible = false
	_ui.add_child(_text_label)
	_popup = PanelContainer.new()
	_popup.set_anchors_preset(Control.PRESET_CENTER)
	_popup.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_popup.grow_vertical = Control.GROW_DIRECTION_BOTH
	_popup.custom_minimum_size = Vector2(460, 0)
	_popup.visible = false
	_ui.add_child(_popup)
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 10)
	_popup.add_child(box)
	_popup_title = Label.new()
	_popup_title.set_meta("game_text", true)
	_popup_title.add_theme_font_size_override("font_size", 24)
	box.add_child(_popup_title)
	_popup_body = Label.new()
	_popup_body.set_meta("game_text", true)
	_popup_body.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(_popup_body)
	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_END
	box.add_child(row)
	var cancel := Button.new()
	cancel.text = "Cancel"
	cancel.pressed.connect(_close_popup)
	row.add_child(cancel)
	var enter := Button.new()
	enter.text = "OK"
	enter.pressed.connect(func() -> void: enter_region(_popup_region))
	row.add_child(enter)


func _text(label: String) -> String:
	if label.is_empty():
		return ""
	var t: Dictionary = game._shell.fetch_text(label)
	if not t.get("found", false):
		_stop("text", "no string for " + label)
		return label
	return String(t.text)


func _stop(kind: String, line: String) -> void:
	var key := kind + line
	if not _unported.has(key):
		_unported[key] = true
		print("ACTMAP STOP [S-5017] %s: %s" % [kind, line])


# ---- l'image ----

func _process(delta: float) -> void:
	if not active:
		return
	_accum += delta
	var steps := 0
	while _accum >= LOGIC_STEP and steps < 5:
		_accum -= LOGIC_STEP
		steps += 1
		acts.update()
		_run_commands()
	if steps > 0:
		_sync()
	_accum = minf(_accum, LOGIC_STEP)
	_camera_tick(delta)
	_effects_tick(delta)
	if _text_left > 0.0:
		_text_left -= delta
		if _text_left <= 0.0:
			_text_label.visible = false
	if not shot_dir.is_empty():
		_shot_tick(delta)
	if auto_regions != 0 or _done_frames >= 0:
		_auto_tick()


func _run_commands() -> void:
	for c in acts.take_commands():
		match String(c.type):
			"world_text":
				_text_label.text = _text(String(c.text))
				_text_label.visible = true
				_text_left = float(c.duration) if float(c.duration) > 0.0 else TEXT_SECONDS
				print("ACTMAP text %s" % c.text)
			"audio":
				game._audio.play_sound(String(c.text))
				print("ACTMAP audio %s" % c.text)
			"move_camera":
				_spline = []
				_scroll = {"from": cam_target, "to": _w3d(c.position.x, c.position.y), "from_angle": cam_angle, "to_angle": deg_to_rad(float(c.angle)), "time": 0.0,
					"duration": maxf(float(c.duration), 0.001)}
				print("ACTMAP camera to %s in %.1f s" % [str(c.position), float(c.duration)])
			"spline_camera":
				_scroll = {}
				_spline = c.spline
				_spline_time = 0.0
				print("ACTMAP spline of %d points" % _spline.size())
			"eye_tower":
				_stop("EyeTowerPoints", "the eye tower is not drawn")
			"update_anim":
				_stop("UpdateAnimObject", String(c.text))


## L'état des armées sur la carte ; les erreurs de données de la logique vont au journal
func _sync() -> void:
	var st: Dictionary = acts.state()
	for e in st.errors:
		if not _unported.has(e):
			_unported[e] = true
			push_error("ACTMAP " + String(e))
	var seen := {}
	for a in st.armies:
		seen[String(a.name)] = true
		var icon: Dictionary = _icons.get(String(a.name), {})
		if icon.is_empty():
			icon = _make_icon(a)
			if icon.is_empty():
				continue
			_icons[String(a.name)] = icon
		if String(a.icon_size) != String(icon.size):
			_show_soldiers(icon, String(a.icon_size))
		_place_icon(icon, _w3d(float(a.x), float(a.y), float(info.map_info.army_height)))
	for name in _icons.keys():
		if not seen.has(name):
			for entry in _icons[name].instances.values():
				instancer.remove_instance(int(entry.id))
			_icons.erase(name)


## Les objets visibles d'une icône, des instances du W3DInstancer : le matériau W3D complet (LM_PickNoVis, City_*, a la profondeur NEVER : invisible, seulement sélectionnable)
func _make_icon(a: Dictionary) -> Dictionary:
	var data: Dictionary = info.army_icons.get(String(a.icon), {})
	if data.is_empty():
		_stop("army icon", "no LivingWorldArmyIcon %s (army %s)" % [a.icon, a.name])
		return {}
	var icon := {"objects": data.objects, "instances": {}, "size": "\n", "position": Vector3.INF}
	_show_soldiers(icon, String(a.icon_size))
	return icon


func _icon_model(model_name: String) -> int:
	if _icon_models.has(model_name):
		return int(_icon_models[model_name])
	var model: int = instancer.add_model(model_name)
	if model < 0:
		_stop("army icon", "the model %s did not build" % model_name)
	else:
		var rep: Dictionary = instancer.get_model_report(model)
		if not rep.errors.is_empty():
			_stop("army icon", "%s: %s" % [model_name, JSON.stringify(rep.errors)])
	_icon_models[model_name] = model
	return model


## Les objets non cachés ; parmi les soldats, celui de IconSize (ArmySoldierLarge / Medium / Small)
func _show_soldiers(icon: Dictionary, size: String) -> void:
	icon.size = size
	var shown: String = SOLDIERS_BY_SIZE.get(size, "Soldiers")
	for o in icon.objects:
		var object_name := String(o.name)
		var want: bool = object_name == shown if SOLDIER_OBJECTS.has(object_name) else not bool(o.hidden)
		var has: bool = icon.instances.has(object_name)
		if want and not has:
			var model := _icon_model(String(o.model))
			if model >= 0:
				icon.instances[object_name] = {"id": instancer.add_instance(model, Transform3D.IDENTITY), "object": o}
				icon.position = Vector3.INF
		elif has and not want:
			instancer.remove_instance(int(icon.instances[object_name].id))
			icon.instances.erase(object_name)


func _place_icon(icon: Dictionary, position: Vector3) -> void:
	if position == icon.position:
		return
	icon.position = position
	for entry in icon.instances.values():
		var o: Dictionary = entry.object
		var basis := Basis(Vector3.UP, deg_to_rad(float(o.orient_angle))).scaled(Vector3.ONE * float(o.scale))
		instancer.set_instance_transform(int(entry.id), Transform3D(basis, position + Vector3(0, float(o.z_offset), 0)))


# ---- la caméra ----

func _update_camera() -> void:
	var off := Vector3(sin(cam_angle) * cos(CAMERA_PITCH), sin(CAMERA_PITCH), cos(cam_angle) * cos(CAMERA_PITCH)) * cam_dist
	camera.position = cam_target + off
	camera.look_at(cam_target, Vector3.UP)


func _catmull(p0: float, p1: float, p2: float, p3: float, t: float) -> float:
	return 0.5 * (2.0 * p1 + (p2 - p0) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t * t + (3.0 * p1 - p0 - 3.0 * p2 + p3) * t * t * t)


func _camera_tick(delta: float) -> void:
	if not _spline.is_empty():
		_spline_time += delta
		var n := _spline.size()
		var i := 0
		while i < n - 2 and _spline_time > float(_spline[i + 1].time):
			i += 1
		if n == 1 or _spline_time >= float(_spline[n - 1].time):
			var last: Dictionary = _spline[n - 1]
			cam_target = _w3d(last.position.x, last.position.y)
			cam_angle = deg_to_rad(float(last.angle))
			_spline = []
		else:
			var a: Dictionary = _spline[maxi(i - 1, 0)]
			var b: Dictionary = _spline[i]
			var c: Dictionary = _spline[i + 1]
			var d: Dictionary = _spline[mini(i + 2, n - 1)]
			var span := maxf(float(c.time) - float(b.time), 0.001)
			var t := clampf((_spline_time - float(b.time)) / span, 0.0, 1.0)
			cam_target = _w3d(_catmull(a.position.x, b.position.x, c.position.x, d.position.x, t), _catmull(a.position.y, b.position.y, c.position.y, d.position.y, t))
			cam_angle = deg_to_rad(_catmull(float(a.angle), float(b.angle), float(c.angle), float(d.angle), t))
	elif not _scroll.is_empty():
		_scroll.time += delta
		var t := clampf(float(_scroll.time) / float(_scroll.duration), 0.0, 1.0)
		cam_target = (_scroll.from as Vector3).lerp(_scroll.to, t)
		cam_angle = lerpf(float(_scroll.from_angle), float(_scroll.to_angle), t)
		if t >= 1.0:
			_scroll = {}
	else:
		var pan := Vector3.ZERO
		if Input.is_key_pressed(KEY_LEFT):
			pan.x -= 1.0
		if Input.is_key_pressed(KEY_RIGHT):
			pan.x += 1.0
		if Input.is_key_pressed(KEY_UP):
			pan.z -= 1.0
		if Input.is_key_pressed(KEY_DOWN):
			pan.z += 1.0
		cam_target += pan.rotated(Vector3.UP, cam_angle) * delta * cam_dist * 0.6
	_update_camera()


# ---- les effets de région ----

## La couleur d'un effet à l'instant `t` : Position d'un point de contrôle = RVB, Time en secondes, interpolation linéaire, tenue après le dernier point
func _effect_color(effect_name: String, t: float) -> Color:
	var e: Dictionary = camp.effects.get(effect_name, {})
	var points: Array = e.get("points", [])
	if points.is_empty():
		return Color(0, 0, 0)
	var prev: Dictionary = points[0]
	if t <= float(prev.time):
		return Color(prev.position.x, prev.position.y, prev.position.z)
	for p in points:
		if t <= float(p.time):
			var span := maxf(float(p.time) - float(prev.time), 0.001)
			var u := (t - float(prev.time)) / span
			var v: Vector3 = (prev.position as Vector3).lerp(p.position, u)
			return Color(v.x, v.y, v.z)
		prev = p
	return Color(prev.position.x, prev.position.y, prev.position.z)


func _effects_tick(delta: float) -> void:
	var st: Dictionary = acts.state()
	var colors := {} # MeshInstance3D -> Color (les effets s'ajoutent)
	var add := func(effect_name: String, region: String, t: float) -> void:
		var e: Dictionary = camp.effects.get(effect_name, {})
		var c: Color = _effect_color(effect_name, t)
		for g in e.get("geometry", []):
			var mesh = _meshes.get(String(g).to_lower(), {}).get(region, null)
			if mesh != null:
				var old: Color = colors.get(mesh, Color(0, 0, 0))
				colors[mesh] = Color(old.r + c.r, old.g + c.g, old.b + c.b)
	for r in st.enabled:
		add.call("EnemyBordersEffect", String(r), 0.0)
	for r in st.forced:
		add.call("EnemyBordersEffect", String(r), 0.0)
	for r in st.conquered:
		add.call("FriendlyBordersEffect", String(r), 0.0)
	for r in _flare.keys():
		_flare[r] += delta
		add.call("ConqueredEffectEvenglow", String(r), float(_flare[r]))
		add.call("ConqueredEffectFlareup", String(r), float(_flare[r]))
		if float(_flare[r]) > 6.0: # le dernier point de ces deux effets
			_flare.erase(r)
	if not _hovered.is_empty():
		_hover_time += delta
		add.call("MouseoverEffectFlareupOwned" if st.conquered.has(_hovered) else "MouseoverEffectFlareupContested", _hovered, _hover_time)
	for mesh in _materials.keys():
		var c: Color = colors.get(mesh, Color(0, 0, 0))
		var lit := c.r > 0.0 or c.g > 0.0 or c.b > 0.0
		mesh.visible = lit
		if lit:
			_materials[mesh].set_shader_parameter("emissive", Vector3(c.r, c.g, c.b))


# ---- le joueur ----

func _pick_region(screen: Vector2) -> String:
	var from := camera.project_ray_origin(screen)
	var to := from + camera.project_ray_normal(screen) * 50000.0
	var hit := get_world_3d().direct_space_state.intersect_ray(PhysicsRayQueryParameters3D.create(from, to))
	if hit.is_empty() or not hit.collider.has_meta("region"):
		return ""
	return String(hit.collider.get_meta("region"))


func _unhandled_input(event: InputEvent) -> void:
	if not active:
		return
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_ESCAPE:
		get_viewport().set_input_as_handled()
		if _popup.visible:
			_close_popup()
		else:
			leave_to_menu()
	elif event is InputEventMouseMotion:
		var r := _pick_region(event.position)
		if r != _hovered:
			_hovered = r
			_hover_time = 0.0
	elif event is InputEventMouseButton and event.pressed:
		match event.button_index:
			MOUSE_BUTTON_WHEEL_UP:
				cam_dist = maxf(cam_dist * 0.9, 500.0)
			MOUSE_BUTTON_WHEEL_DOWN:
				cam_dist = minf(cam_dist * 1.1, 4000.0)
			MOUSE_BUTTON_LEFT:
				var r := _pick_region(event.position)
				if not r.is_empty() and acts.can_enter_region(r):
					_open_popup(r)


func _open_popup(region: String) -> void:
	var r: Dictionary = _region_by_name[region]
	_popup_region = region
	_popup_title.text = _text(String(r.display_name))
	var lines := PackedStringArray()
	for o in r.objectives:
		lines.append(_text(String(o)))
	_popup_body.text = "\n".join(lines)
	_popup.visible = true


func _close_popup() -> void:
	_popup.visible = false
	_popup_region = ""


## Le joueur entre dans une région : sa mission, par le flux de campagne
func enter_region(region: String) -> void:
	_close_popup()
	if not acts.can_enter_region(region):
		push_error("ACTMAP %s cannot be entered in act %s" % [region, acts.state().act])
		return
	var r: Dictionary = _region_by_name[region]
	_popup_region = region
	var movie_name: String = acts.enter_region(region)
	print("ACTMAP enter %s (act %s): map '%s', movie '%s'" % [region, acts.state().act, r.map, movie_name])
	_hide_map()
	game._campaign_flow.begin_region_mission(self, {"map": String(r.map), "still": String(r.still), "music": String(r.music), "voice": String(r.voice), "movie": movie_name,
		"difficulty": difficulty})


## La mission est finie et sa partie effacée : retour à l'écran du monde (campaign_flow.after_score)
func mission_over(victory: bool) -> void:
	var region := _popup_region
	_popup_region = ""
	game._campaign_flow.act_map = null
	print("ACTMAP mission %s: %s" % [region, "victory" if victory else "defeat"])
	while game._shell.shell_stack().size() > 0:
		game._shell.shell_pop()
		game._shell.tick(0.033)
	game._audio.stop_music(false)
	_show_map()
	if not victory:
		return
	if not acts.region_won(region):
		push_error("ACTMAP %s was not playable when it was won" % region)
		return
	_won_count += 1
	_flare[region] = 0.0
	if not String(camp.conquered_sound).is_empty():
		game._audio.play_sound(String(camp.conquered_sound))
	_stop("ConqueredNotice", String(_region_by_name[region].conquered_notice))
	var st: Dictionary = acts.state()
	print("ACTMAP %s won: act %s, army points %d, legendary points %d, regions %s" % [region, st.act, st.army_points, st.legendary_points, str(st.enabled)])


func leave_to_menu() -> void:
	_hide_map()
	game._campaign_flow.act_map = null
	game._campaign_flow._to_main_menu()


# ---- les tests : --act-auto=N ----

func _shot_tick(delta: float) -> void:
	_shot_at += delta
	if _shot_at < 4.0:
		return
	_shot_at = 0.0
	var path := shot_dir.path_join("map%02d.jpg" % _shots_taken)
	_shots_taken += 1
	var err: int = get_viewport().get_texture().get_image().save_jpg(path, 0.85)
	print("ACTMAP shot %s act %s (%d)" % [path, acts.state().act, err])


func _auto_tick() -> void:
	if _done_frames >= 0:
		_done_frames += 1
		if _done_frames >= 600: # 20 s d'images de l'acte atteint, ou la fin de marche qui y mène
			var st: Dictionary = acts.state()
			print("ACTMAP auto done: act %s, won %d, conquered %s, enabled %s, armies %d, errors %s" % [st.act, _won_count, str(st.conquered), str(st.enabled), st.armies.size(), str(st.errors)])
			get_tree().quit(0 if st.errors.is_empty() else 1)
		return
	if _won_count >= auto_regions:
		auto_regions = 0
		_done_frames = 0
		return
	var state: Dictionary = acts.state()
	if _auto_busy or int(state.frame) < (30 if shot_dir.is_empty() else 500) or state.enabled.is_empty():
		return
	_auto_busy = true
	await _auto_click_region(String(state.enabled[0]))
	_auto_busy = false


## Le chemin d'un joueur : un point de la région sur l'écran, le clic de souris, la fenêtre, son OK
func _auto_click_region(region: String) -> void:
	var fill: Dictionary = _meshes.get(String(camp.region_object).to_lower(), {})
	var picks := 0
	for r in fill.keys():
		var c: Vector3 = fill[r].global_transform * fill[r].get_aabb().get_center()
		if not _pick_region(camera.unproject_position(c)).is_empty():
			picks += 1
	print("ACTMAP pick check: %d of %d regions answer at their centre (%d regions)" % [picks, fill.size(), camp.regions.size()])
	if not fill.has(region):
		game._fail("ACTMAP %s has no pick body" % region)
		return
	var mesh: MeshInstance3D = fill[region]
	var box: AABB = mesh.global_transform * mesh.get_aabb()
	_spline = []
	_scroll = {}
	cam_target = Vector3(box.get_center().x, 0.0, box.get_center().z)
	_update_camera()
	await get_tree().physics_frame
	await get_tree().physics_frame
	var size := get_viewport().get_visible_rect().size
	var grid := PackedStringArray()
	for gy in range(1, 6):
		var row := PackedStringArray()
		for gx in range(1, 8):
			var name_at := _pick_region(Vector2(size.x * gx / 8.0, size.y * gy / 6.0))
			row.append(name_at if not name_at.is_empty() else "-")
		grid.append(" ".join(row))
	print("ACTMAP pick grid around %s:\n  %s" % [region, "\n  ".join(grid)])
	var screen := Vector2(-1, -1)
	for i in range(1, 8):
		for j in range(1, 8):
			var p := box.position + Vector3(box.size.x * i / 8.0, box.size.y, box.size.z * j / 8.0)
			var at := camera.unproject_position(p)
			if get_viewport().get_visible_rect().has_point(at) and _pick_region(at) == region:
				screen = at
				break
		if screen.x >= 0.0:
			break
	if screen.x < 0.0:
		game._fail("ACTMAP no point of %s answers the mouse" % region)
		return
	var motion := InputEventMouseMotion.new()
	motion.position = screen
	motion.global_position = screen
	get_viewport().push_input(motion, true)
	await get_tree().process_frame
	for pressed in [true, false]:
		var button := InputEventMouseButton.new()
		button.button_index = MOUSE_BUTTON_LEFT
		button.pressed = pressed
		button.position = screen
		button.global_position = screen
		get_viewport().push_input(button, true)
		await get_tree().process_frame
	await get_tree().process_frame
	if not _popup.visible or _popup_region != region:
		game._fail("ACTMAP the click at %s did not open the window of %s (hovered '%s')" % [str(screen), region, _hovered])
		get_tree().quit(1)
		return
	print("ACTMAP clicked %s at %s: window '%s' / '%s'" % [region, str(screen), _popup_title.text, _popup_body.text.replace("\n", " | ").left(120)])
	enter_region(region)
