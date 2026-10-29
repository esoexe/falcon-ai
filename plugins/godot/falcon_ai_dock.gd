@tool
extends EditorPlugin
# Falcon AI v3.0 — Official Godot 4.x Live-Link Dock (Pure GDScript, Zero Python)
# Copy `addons/falcon_ai/` into your Godot 4 project and enable in Project Settings -> Plugins.

var dock: VBoxContainer
var prompt_input: TextEdit
var status_lbl: Label
var http_gen: HTTPRequest
var http_dl: HTTPRequest
var server_url := "http://127.0.0.1:8000"
var last_asset_id := ""

func _enter_tree() -> void:
	dock = VBoxContainer.new()
	dock.name = "Falcon AI v3.0"

	var title := Label.new()
	title.text = "FALCON AI v3.0 · GODOT 4 LIVE-LINK"
	dock.add_child(title)

	prompt_input = TextEdit.new()
	prompt_input.custom_minimum_size = Vector2(0, 72)
	prompt_input.text = "Stylized fantasy crystal golem guardian with glowing runes"
	dock.add_child(prompt_input)

	var btn := Button.new()
	btn.text = "Generate Rigged 3D GLB & Spawn in Scene"
	btn.pressed.connect(_on_generate_pressed)
	dock.add_child(btn)

	status_lbl = Label.new()
	status_lbl.text = "Ready · Local C++20 Engine (:8000)"
	status_lbl.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	dock.add_child(status_lbl)

	http_gen = HTTPRequest.new()
	http_gen.request_completed.connect(_on_gen_completed)
	dock.add_child(http_gen)

	http_dl = HTTPRequest.new()
	http_dl.request_completed.connect(_on_glb_downloaded)
	dock.add_child(http_dl)

	add_control_to_dock(DOCK_SLOT_RIGHT_UL, dock)

func _exit_tree() -> void:
	if dock:
		remove_control_from_docks(dock)
		dock.queue_free()

func _on_generate_pressed() -> void:
	status_lbl.text = "Synthesizing 3D mesh + skeleton in Falcon AI..."
	var body := JSON.stringify({"prompt": prompt_input.text, "style": "realistic", "ultra_detail": true})
	http_gen.request(server_url + "/api/generate/text", ["Content-Type: application/json"], HTTPClient.METHOD_POST, body)

func _on_gen_completed(_result: int, code: int, _headers: PackedStringArray, body: PackedByteArray) -> void:
	if code != 200:
		status_lbl.text = "Generation failed (HTTP %d)" % code
		return
	var meta: Dictionary = JSON.parse_string(body.get_string_from_utf8())
	last_asset_id = meta.get("id", "")
	status_lbl.text = "Downloading Skinned + Animated GLB (%s)..." % last_asset_id
	http_dl.request(server_url + "/api/export/" + last_asset_id + "/glb?rigged=1")

func _on_glb_downloaded(_result: int, code: int, _headers: PackedStringArray, glb_bytes: PackedByteArray) -> void:
	if code != 200 or glb_bytes.is_empty():
		status_lbl.text = "GLB download failed."
		return
	DirAccess.make_dir_recursive_absolute("res://falcon_assets")
	var out_path := "res://falcon_assets/falcon_" + last_asset_id + ".glb"
	var f := FileAccess.open(out_path, FileAccess.WRITE)
	f.store_buffer(glb_bytes)
	f.close()

	var doc := GLTFDocument.new()
	var state := GLTFState.new()
	if doc.append_from_buffer(glb_bytes, "", state) == OK:
		var root_scn := get_editor_interface().get_edited_scene_root()
		if root_scn:
			var inst := doc.generate_scene(state)
			inst.name = "FalconAsset_" + last_asset_id
			root_scn.add_child(inst)
			inst.owner = root_scn
	get_editor_interface().get_resource_filesystem().scan()
	status_lbl.text = "Spawned & saved: " + out_path
