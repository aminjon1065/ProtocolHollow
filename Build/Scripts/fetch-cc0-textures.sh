#!/usr/bin/env bash
# Art-01 (ADR-0043): downloads the CC0 textures of Build/Art/cc0_textures.json (Poly Haven, 1K JPG) into
# Intermediate/HollowArt/cc0/<id>/ and checks each file's SHA-256 against the manifest. Build/Python/create_art_kit.py
# imports them; the imported textures are what git keeps, so only a machine that changes the set needs this.
# Usage: Build/Scripts/fetch-cc0-textures.sh [--print-hashes]
#   --print-hashes  prints the SHA-256 of every downloaded file (to fill the manifest when the set changes)
# Exit codes: 0 every file present and verified | 1 a download or a hash failed | 2 usage or environment
set -euo pipefail
source "$(dirname "$0")/hollow-common.sh"

PRINT=0
[[ "${1:-}" == "--print-hashes" ]] && PRINT=1
MANIFEST="$HOLLOW_PROJECT_ROOT/Build/Art/cc0_textures.json"
DEST="$HOLLOW_PROJECT_ROOT/Intermediate/HollowArt/cc0"
command -v curl > /dev/null || { echo "curl is needed"; exit 2; }
command -v python3 > /dev/null || { echo "python3 is needed"; exit 2; }
mkdir -p "$DEST"

# id <tab> map <tab> url <tab> sha256, from the manifest (the project's own file).
python3 -I -c '
import json, sys
for t in json.load(open(sys.argv[1]))["textures"]:
    for key, m in sorted(t["maps"].items()):
        print("\t".join((t["id"], key, m["url"], m.get("sha256", ""))))
' "$MANIFEST" | while IFS=$'\t' read -r id map url sha; do
	dir="$DEST/$id"
	file="$dir/$(basename "$url")"
	mkdir -p "$dir"
	if [[ ! -s "$file" ]]; then
		curl -sSfL --retry 3 -m 120 -A "HollowArtFetch/1.0" -o "$file.part" "$url" || { echo "FAILED download $url"; exit 1; }
		mv "$file.part" "$file"
	fi
	actual=$(shasum -a 256 "$file" | cut -d' ' -f1)
	if ((PRINT)); then
		echo "$id $map $actual"
	elif [[ "$actual" != "$sha" ]]; then
		echo "FAILED hash $file: $actual, the manifest says $sha"
		exit 1
	fi
done
echo "OK: CC0 textures in $DEST"
