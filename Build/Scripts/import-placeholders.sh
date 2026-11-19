#!/usr/bin/env bash
# Copies placeholder character content (UE5 Manny + first-person anims) from the local UE 5.8 templates
# into Content/, keeping the template /Game paths so internal references stay valid (ADR-0011).
# Placeholders are not committed (.gitignore): run once per machine after cloning; re-running is safe.
# Pair script: import-placeholders.ps1.
#
# Usage:  Build/Scripts/import-placeholders.sh [--check]
#   --check  only verify that the placeholders are present (exit 1 if not)
# Env:    UE_ROOT  engine root (default: /Users/Shared/Epic Games/UE_5.8)
set -euo pipefail
# shellcheck source=hollow-common.sh
source "$(dirname "${BASH_SOURCE[0]}")/hollow-common.sh"

CHARACTERS_SRC="${UE_ROOT}/Templates/TemplateResources/High/Characters/Content/Mannequins"
FIRST_PERSON_SRC="${UE_ROOT}/Templates/TP_FirstPerson/Content/FirstPerson/Anims"
CHARACTERS_DST="${HOLLOW_PROJECT_ROOT}/Content/Characters/Mannequins"
FIRST_PERSON_DST="${HOLLOW_PROJECT_ROOT}/Content/FirstPerson/Anims"

# Assets the project references directly; their presence means the import is complete.
REQUIRED=(
	"${CHARACTERS_DST}/Meshes/SKM_Manny_Simple.uasset"
	"${CHARACTERS_DST}/Anims/Unarmed/ABP_Unarmed.uasset"
	"${FIRST_PERSON_DST}/ABP_FP_Copy.uasset"
)

placeholders_present() {
	local file
	for file in "${REQUIRED[@]}"; do
		[[ -f "${file}" ]] || return 1
	done
	return 0
}

if [[ "${1:-}" == "--check" ]]; then
	if placeholders_present; then
		echo "OK: placeholders present."
		exit 0
	fi
	echo "MISSING: placeholders not imported. Run Build/Scripts/import-placeholders.sh" >&2
	exit 1
fi

for SRC in "${CHARACTERS_SRC}" "${FIRST_PERSON_SRC}"; do
	if [[ ! -d "${SRC}" ]]; then
		echo "Template content not found: ${SRC} (set UE_ROOT)" >&2
		exit 2
	fi
done

# Subset closed over its references: Quinn materials and melee attack anims are left out on purpose.
mkdir -p "${CHARACTERS_DST}/Materials" "${CHARACTERS_DST}/Textures" "${CHARACTERS_DST}/Anims" "${FIRST_PERSON_DST}"
rsync -a "${CHARACTERS_SRC}/Meshes/" "${CHARACTERS_DST}/Meshes/"
rsync -a "${CHARACTERS_SRC}/Materials/M_Mannequin.uasset" "${CHARACTERS_DST}/Materials/"
rsync -a "${CHARACTERS_SRC}/Materials/Manny/" "${CHARACTERS_DST}/Materials/Manny/"
rsync -a "${CHARACTERS_SRC}/Textures/Manny/" "${CHARACTERS_DST}/Textures/Manny/"
rsync -a "${CHARACTERS_SRC}/Textures/Shared/" "${CHARACTERS_DST}/Textures/Shared/"
rsync -a "${CHARACTERS_SRC}/Rigs/" "${CHARACTERS_DST}/Rigs/"
rsync -a --exclude 'Attack/' "${CHARACTERS_SRC}/Anims/Unarmed/" "${CHARACTERS_DST}/Anims/Unarmed/"
rsync -a "${FIRST_PERSON_SRC}/" "${FIRST_PERSON_DST}/"

if ! placeholders_present; then
	echo "FAILED: import finished but required assets are missing." >&2
	exit 1
fi
echo "OK: placeholders imported into Content/Characters/Mannequins and Content/FirstPerson/Anims."
