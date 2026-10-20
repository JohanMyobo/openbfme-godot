#!/bin/zsh
# Notre version : exporte OpenBFME.app universel (Apple Silicon + Intel), sans lanceur ni mise à jour.
# Suit l'ancien chemin « universel » de docs/MACOS.md, avec les deux dylibs (le manifeste choisit
# l'arm64 sur Apple Silicon, l'universelle sur Intel).
# Usage : tools/notre-version/package_universal.sh   (depuis la racine du dépôt)
set -eu
ROOT="${0:A:h:h:h}"
cd "$ROOT"
TOOLS="$ROOT/workspace/mac3-tools"
GODOT="$TOOLS/Godot.app/Contents/MacOS/Godot"
TPZ="$TOOLS/Godot_v4.7.2-stable_export_templates.tpz"
TPZ_SHA=f298490b8d44d934be425a5a65a51bf15f422428b229a06a6e11d9ffea248011

[[ -x $GODOT ]] || { print -u2 "Godot 4.7.2 absent : $GODOT"; exit 1 }
print "$TPZ_SHA  $TPZ" | shasum -a 256 -c - >/dev/null

# Templates macOS (une seule fois)
TEMPLATE_DIR="$HOME/Library/Application Support/Godot/export_templates/4.7.2.stable"
if [[ ! -f $TEMPLATE_DIR/macos.zip ]]; then
  mkdir -p "$TEMPLATE_DIR"
  unzip -p "$TPZ" templates/macos.zip >"$TEMPLATE_DIR/macos.zip"
  unzip -tq "$TEMPLATE_DIR/macos.zip"
fi

cmake --build workspace/mac3-build --target openbfme -j 8
for f in universal arm64; do
  [[ -f godot/bin/openbfme.macos.template_debug.$f.dylib ]] || { print -u2 "dylib $f absente"; exit 1 }
done

VERSION="notre-version-$(git rev-parse --short=10 HEAD)$(git diff --quiet && git diff --cached --quiet || print -- -modifie)"
STAGE="$ROOT/workspace/mac3-stage"
PKG="$ROOT/workspace/mac3-release/openbfme-$VERSION-macos-universal"
rm -rf "$STAGE" "$PKG"
mkdir -p "$STAGE" "$PKG"

# Copie des fichiers suivis par git (+ modifications locales non commitées), sans caches ni données privées
git ls-files -z godot | rsync -a --from0 --files-from=- ./ "$STAGE/"
mkdir -p "$STAGE/godot/bin"
cp godot/bin/openbfme.macos.template_debug.{universal,arm64}.dylib "$STAGE/godot/bin/"

"$GODOT" --headless --editor --path "$STAGE/godot" --import --quit >"$ROOT/workspace/mac3-import.log" 2>&1 || true
"$GODOT" --headless --path "$STAGE/godot" --export-debug 'macOS Universal' "$PKG/OpenBFME.app" \
  >"$ROOT/workspace/mac3-export.log" 2>&1

codesign --verify --deep --strict "$PKG/OpenBFME.app"
print "OpenBFME.app : $(lipo -archs "$PKG/OpenBFME.app/Contents/MacOS/OpenBFME")"
cp LICENSE NOTICE "$PKG/"
printf 'OpenBFME %s\ncommit %s\n' "$VERSION" "$(git rev-parse HEAD)" >"$PKG/VERSION"
ditto -c -k --keepParent "$PKG" "$PKG.zip"
print "OK → $PKG"
