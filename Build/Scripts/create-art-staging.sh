#!/usr/bin/env bash
# Art-01 (ADR-0043): creates the Fab staging project outside git (default ~/Desktop/HollowArtStaging): an empty UE 5.8
# project without starter content, with Python. Fab packs go in there ("Add to project" in the editor's Fab window),
# Build/Python/prepare_fab_assets.py makes them fit the budget there, and only what Build/Art/fab_manifest.json names
# is migrated into the game's Content/Fab (GitHub Free LFS). Safe to run again: an existing project is only checked.
# Usage: Build/Scripts/create-art-staging.sh [path]
# Exit codes: 0 ready | 2 usage or environment
set -euo pipefail
source "$(dirname "$0")/hollow-common.sh"

STAGING="${1:-$HOME/Desktop/HollowArtStaging}"
NAME="$(basename "$STAGING")"
UPROJECT="$STAGING/$NAME.uproject"
case "$STAGING" in
	"$HOLLOW_PROJECT_ROOT"*) echo "The staging project must live outside the game's repository: $STAGING"; exit 2 ;;
esac
mkdir -p "$STAGING/Content" "$STAGING/Config"
if [[ ! -f "$UPROJECT" ]]; then
	cat > "$UPROJECT" <<JSON
{
	"FileVersion": 3,
	"EngineAssociation": "5.8",
	"Category": "",
	"Description": "Protocol: Hollow - Fab staging, outside git (ADR-0043). Packs come in here; only the manifest's assets go to the game.",
	"Plugins": [
		{ "Name": "PythonScriptPlugin", "Enabled": true },
		{ "Name": "EditorScriptingUtilities", "Enabled": true }
	]
}
JSON
	echo "created $UPROJECT"
fi
if [[ ! -f "$STAGING/Config/DefaultEngine.ini" ]]; then
	cat > "$STAGING/Config/DefaultEngine.ini" <<'INI'
[/Script/EngineSettings.GameMapsSettings]
EditorStartupMap=
GameDefaultMap=

[/Script/Engine.RendererSettings]
; As the game (ADR-0021): no hardware ray tracing; Nanite and Lumen stay at the engine's defaults.
r.Lumen.HardwareRayTracing=0
INI
fi
python3 -I -c 'import json,sys; d=json.load(open(sys.argv[1])); assert d["EngineAssociation"]=="5.8"' "$UPROJECT" || { echo "Not a UE 5.8 project: $UPROJECT"; exit 2; }
echo "OK: Fab staging at $STAGING"
echo "  1. Open $UPROJECT in UE 5.8, Window > Fab, Add to Project the packs you want."
echo "  2. Build/Scripts/run-editor-python.sh Build/Python/prepare_fab_assets.py --project \"$UPROJECT\""
echo "  3. Name the assets you keep in Build/Art/fab_manifest.json, then run step 2 again with HOLLOW_FAB_MIGRATE=1."
echo "  4. Build/Scripts/run-editor-python.sh Build/Python/prepare_fab_assets.py (in the game: checks and swaps)."
