#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <app-bundle>" >&2
    echo "Set CROSSDESK_MACOS_SIGN_IDENTITY to a full Developer ID Application name." >&2
    echo "Without an identity, the bundle receives an ad-hoc signature for local use." >&2
    exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_BUNDLE="$1"
SIGN_IDENTITY="${CROSSDESK_MACOS_SIGN_IDENTITY:--}"
SIGN_ARGS=(--force --sign "$SIGN_IDENTITY")
APP_ARGS=()

case "$SIGN_IDENTITY" in
    -) ;;
    "Developer ID Application: "*)
        SIGN_ARGS+=(--options runtime --timestamp)
        APP_ARGS+=(--entitlements "$SCRIPT_DIR/entitlements.plist")
        ;;
    *)
        echo "CROSSDESK_MACOS_SIGN_IDENTITY must be a full Developer ID Application name or '-'." >&2
        exit 1
        ;;
esac

if [[ -n "${CROSSDESK_MACOS_KEYCHAIN:-}" ]]; then
    SIGN_ARGS+=(--keychain "$CROSSDESK_MACOS_KEYCHAIN")
fi

for executable in CrossDesk crossdesk_updater; do
    if [[ ! -x "$APP_BUNDLE/Contents/MacOS/$executable" ]]; then
        echo "Missing app executable: $APP_BUNDLE/Contents/MacOS/$executable" >&2
        exit 1
    fi
done

if [[ ! -d "$APP_BUNDLE/Contents/Frameworks" ]]; then
    echo "Missing bundled frameworks: $APP_BUNDLE/Contents/Frameworks" >&2
    exit 1
fi

# Sign nested code explicitly, from the inside out. The updater also runs
# outside the bundle, and must retain its own valid Developer ID signature.
while IFS= read -r -d '' library; do
    codesign "${SIGN_ARGS[@]}" "$library"
done < <(find "$APP_BUNDLE/Contents/Frameworks" -type f -name '*.dylib' -print0)
codesign "${SIGN_ARGS[@]}" "$APP_BUNDLE/Contents/MacOS/crossdesk_updater"
codesign "${SIGN_ARGS[@]}" ${APP_ARGS[@]+"${APP_ARGS[@]}"} "$APP_BUNDLE"
codesign --verify --deep --strict --verbose=2 "$APP_BUNDLE"

echo "App signed with: $SIGN_IDENTITY"
