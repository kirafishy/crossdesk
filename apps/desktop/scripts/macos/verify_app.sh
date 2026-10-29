#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
    echo "Usage: $0 <app-bundle> <expected-architecture> [expected-sdk-version]" >&2
    exit 2
fi

APP_BUNDLE="$1"
EXPECTED_ARCH="$2"
EXPECTED_SDK_VERSION="${3:-}"
APP_EXECUTABLE="$APP_BUNDLE/Contents/MacOS/CrossDesk"
FRAMEWORKS_DIR="$APP_BUNDLE/Contents/Frameworks"
INFO_PLIST="$APP_BUNDLE/Contents/Info.plist"

if [[ ! -x "$APP_EXECUTABLE" ]]; then
    echo "Missing app executable: $APP_EXECUTABLE" >&2
    exit 1
fi

if ! lipo "$APP_EXECUTABLE" -verify_arch "$EXPECTED_ARCH" >/dev/null 2>&1; then
    echo "Unexpected app architecture; expected $EXPECTED_ARCH" >&2
    lipo -archs "$APP_EXECUTABLE" >&2 || true
    exit 1
fi

if ! otool -l "$APP_EXECUTABLE" | awk '
    $1 == "cmd" { in_rpath = ($2 == "LC_RPATH"); next }
    in_rpath && $1 == "path" && $2 == "@executable_path/../Frameworks" { found = 1 }
    END { exit(found ? 0 : 1) }'; then
    echo "Missing app rpath: @executable_path/../Frameworks" >&2
    exit 1
fi

verify_dependencies() {
    local macho="$1"
    local dependency=""
    local relative_path=""

    while IFS= read -r dependency; do
        case "$dependency" in
            /System/Library/*|/usr/lib/*)
                ;;
            @rpath/*)
                relative_path="${dependency#@rpath/}"
                if [[ ! -f "$FRAMEWORKS_DIR/$relative_path" ]]; then
                    echo "Unbundled dependency in $macho: $dependency" >&2
                    return 1
                fi
                ;;
            @executable_path/../Frameworks/*)
                relative_path="${dependency#@executable_path/../Frameworks/}"
                if [[ ! -f "$FRAMEWORKS_DIR/$relative_path" ]]; then
                    echo "Unbundled dependency in $macho: $dependency" >&2
                    return 1
                fi
                ;;
            @loader_path/*)
                relative_path="${dependency#@loader_path/}"
                if [[ ! -f "$(dirname "$macho")/$relative_path" ]]; then
                    echo "Unbundled dependency in $macho: $dependency" >&2
                    return 1
                fi
                ;;
            *)
                echo "External non-system dependency in $macho: $dependency" >&2
                return 1
                ;;
        esac
    done < <(otool -L "$macho" | tail -n +2 | awk '{ print $1 }')
}

verify_dependencies "$APP_EXECUTABLE"
UPDATER="$APP_BUNDLE/Contents/MacOS/crossdesk_updater"
if [[ ! -x "$UPDATER" ]] || ! lipo "$UPDATER" -verify_arch "$EXPECTED_ARCH" >/dev/null 2>&1; then
    echo "Missing updater or incorrect updater architecture" >&2
    exit 1
fi
# The updater runs outside the bundle during installation, so it must not rely
# on private frameworks that are being replaced.
while IFS= read -r dependency; do
    case "$dependency" in
        /System/Library/*|/usr/lib/*) ;;
        *) echo "Updater has a non-system dynamic dependency: $dependency" >&2; exit 1 ;;
    esac
done < <(otool -L "$UPDATER" | tail -n +2 | awk '{ print $1 }')

SLINT_LIBRARY="$(find "$FRAMEWORKS_DIR" -maxdepth 1 -type f -name 'libslint_cpp*.dylib' -print -quit 2>/dev/null || true)"
if [[ -z "$SLINT_LIBRARY" ]]; then
    echo "The packaged app does not contain libslint_cpp.dylib" >&2
    exit 1
fi

if ! lipo "$SLINT_LIBRARY" -verify_arch "$EXPECTED_ARCH" >/dev/null 2>&1; then
    echo "Unexpected Slint runtime architecture; expected $EXPECTED_ARCH" >&2
    lipo -archs "$SLINT_LIBRARY" >&2 || true
    exit 1
fi
verify_dependencies "$SLINT_LIBRARY"

PLIST_MIN_VERSION="$(/usr/libexec/PlistBuddy -c 'Print :LSMinimumSystemVersion' "$INFO_PLIST")"
BINARY_MIN_VERSION="$(otool -l "$APP_EXECUTABLE" | awk '
    $1 == "cmd" { build_version = ($2 == "LC_BUILD_VERSION"); version_min = ($2 == "LC_VERSION_MIN_MACOSX"); next }
    build_version && $1 == "minos" { print $2; exit }
    version_min && $1 == "version" { print $2; exit }')"
if [[ -z "$BINARY_MIN_VERSION" || "$PLIST_MIN_VERSION" != "$BINARY_MIN_VERSION" ]]; then
    echo "Minimum macOS version mismatch: plist=$PLIST_MIN_VERSION binary=${BINARY_MIN_VERSION:-unknown}" >&2
    exit 1
fi

if [[ -n "$EXPECTED_SDK_VERSION" ]]; then
    verify_sdk_version() {
        local macho="$1"
        local actual_sdk=""
        actual_sdk="$(otool -l "$macho" | awk '
            $1 == "cmd" { build_version = ($2 == "LC_BUILD_VERSION"); next }
            build_version && $1 == "sdk" { print $2; exit }')"
        if [[ "$actual_sdk" != "$EXPECTED_SDK_VERSION" ]]; then
            echo "Unexpected macOS SDK in $macho: expected $EXPECTED_SDK_VERSION, got ${actual_sdk:-unknown}" >&2
            return 1
        fi
    }

    verify_sdk_version "$APP_EXECUTABLE"
    verify_sdk_version "$SLINT_LIBRARY"
fi

codesign --verify --deep --strict --verbose=2 "$APP_BUNDLE"

# A distribution build must not silently fall back to an ad-hoc/development
# identity, omit a secure timestamp, or leave its updater/runtime unsigned.
APP_SIGNATURE="$(codesign --display --verbose=4 "$APP_BUNDLE" 2>&1)"
if [[ "${CROSSDESK_MACOS_SIGN_IDENTITY:--}" != "-" ||
      "$APP_SIGNATURE" == *"Authority=Developer ID Application: "* ]]; then
    EXPECTED_TEAM="$(printf '%s\n' "$APP_SIGNATURE" | sed -n 's/^TeamIdentifier=//p')"
    verify_distribution_signature() {
        local target="$1"
        local signature=""
        codesign --verify --strict --verbose=2 \
            -R='anchor apple generic and certificate 1[field.1.2.840.113635.100.6.2.6] exists and certificate leaf[field.1.2.840.113635.100.6.1.13] exists' \
            "$target"
        signature="$(codesign --display --verbose=4 "$target" 2>&1)"
        if ! printf '%s\n' "$signature" | grep -q '^CodeDirectory .*flags=.*runtime'; then
            echo "Hardened Runtime is missing: $target" >&2
            return 1
        fi
        if ! printf '%s\n' "$signature" | grep -q '^Timestamp='; then
            echo "Secure signing timestamp is missing: $target" >&2
            return 1
        fi
        if ! printf '%s\n' "$signature" | grep -Fxq "TeamIdentifier=$EXPECTED_TEAM"; then
            echo "Signing team mismatch: $target" >&2
            return 1
        fi
        if [[ "${CROSSDESK_MACOS_SIGN_IDENTITY:--}" != "-" ]] &&
           ! printf '%s\n' "$signature" | grep -Fxq "Authority=$CROSSDESK_MACOS_SIGN_IDENTITY"; then
            echo "Unexpected signing identity: $target" >&2
            return 1
        fi
    }

    verify_distribution_signature "$APP_BUNDLE"
    verify_distribution_signature "$UPDATER"
    while IFS= read -r -d '' library; do
        verify_distribution_signature "$library"
    done < <(find "$FRAMEWORKS_DIR" -type f -name '*.dylib' -print0)
fi

echo "Verified $APP_BUNDLE ($EXPECTED_ARCH, macOS $BINARY_MIN_VERSION+)"
