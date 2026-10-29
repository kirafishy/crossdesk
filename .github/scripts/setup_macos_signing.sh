#!/usr/bin/env bash
set -euo pipefail
# Never echo commands containing imported certificate or notarization secrets.
set +x

if [[ "${GITHUB_ACTIONS:-}" != "true" ]]; then
    echo "This script prepares an isolated GitHub Actions signing keychain." >&2
    exit 1
fi
: "${RUNNER_TEMP:?RUNNER_TEMP is required}"
: "${GITHUB_ENV:?GITHUB_ENV is required}"
: "${GITHUB_OUTPUT:?GITHUB_OUTPUT is required}"

for secret_name in MACOS_APP_CERTIFICATE_P12_BASE64 MACOS_INSTALLER_CERTIFICATE_P12_BASE64 \
    MACOS_P12_PASSWORD APPLE_ID APPLE_APP_SPECIFIC_PASSWORD APPLE_TEAM_ID; do
    if [[ -z "${!secret_name:-}" ]]; then
        echo "Missing required Actions secret: $secret_name" >&2
        exit 1
    fi
done
if [[ ! "$APPLE_TEAM_ID" =~ ^[A-Z0-9]{10}$ ]]; then
    echo "APPLE_TEAM_ID must contain 10 uppercase letters or digits." >&2
    exit 1
fi

SIGNING_DIR="$RUNNER_TEMP/crossdesk-macos-signing"
KEYCHAIN_PATH="$SIGNING_DIR/signing.keychain-db"
APP_P12="$SIGNING_DIR/application.p12"
INSTALLER_P12="$SIGNING_DIR/installer.p12"
NOTARY_PROFILE="crossdesk-notary"

# Refuse to overwrite anything left by an unrelated task. Each hosted matrix
# runner has its own RUNNER_TEMP, and the workflow also cleans up with always().
umask 077
mkdir "$SIGNING_DIR"
cleanup_setup() {
    local status=$?
    rm -f "$APP_P12" "$INSTALLER_P12"
    if [[ "$status" -ne 0 ]]; then
        security delete-keychain "$KEYCHAIN_PATH" >/dev/null 2>&1 || true
        rm -rf "$SIGNING_DIR"
    fi
    exit "$status"
}
trap cleanup_setup EXIT

printf '%s' "$MACOS_APP_CERTIFICATE_P12_BASE64" | base64 --decode > "$APP_P12"
printf '%s' "$MACOS_INSTALLER_CERTIFICATE_P12_BASE64" | base64 --decode > "$INSTALLER_P12"
KEYCHAIN_PASSWORD="$(openssl rand -hex 32)"
security create-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN_PATH"
security set-keychain-settings -lut 21600 "$KEYCHAIN_PATH"
security unlock-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN_PATH"

for certificate in "$APP_P12" "$INSTALLER_P12"; do
    security import "$certificate" -P "$MACOS_P12_PASSWORD" \
        -t cert -f pkcs12 -k "$KEYCHAIN_PATH" \
        -T /usr/bin/codesign -T /usr/bin/productbuild -T /usr/bin/productsign
done
# Allow the signing tools to use only this job's imported keys without a GUI
# password prompt. The user's login keychain and its access controls are untouched.
security set-key-partition-list -S apple-tool:,apple:,codesign: -s \
    -k "$KEYCHAIN_PASSWORD" "$KEYCHAIN_PATH" >/dev/null

find_identity() {
    local policy="$1"
    local prefix="$2"
    security find-identity -v -p "$policy" "$KEYCHAIN_PATH" | \
        awk -F '"' -v prefix="$prefix" -v suffix=" ($APPLE_TEAM_ID)" '
            index($2, prefix) == 1 &&
            substr($2, length($2) - length(suffix) + 1) == suffix {
                print $2
                count++
            }
            END { if (count != 1) exit 1 }'
}
if ! APP_IDENTITY="$(find_identity codesigning 'Developer ID Application: ')"; then
    echo "Expected one valid Developer ID Application identity for APPLE_TEAM_ID, including its private key." >&2
    exit 1
fi
if ! INSTALLER_IDENTITY="$(find_identity basic 'Developer ID Installer: ')"; then
    echo "Expected one valid Developer ID Installer identity for APPLE_TEAM_ID, including its private key." >&2
    exit 1
fi

xcrun notarytool store-credentials "$NOTARY_PROFILE" \
    --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" \
    --password "$APPLE_APP_SPECIFIC_PASSWORD" --keychain "$KEYCHAIN_PATH" --validate

{
    printf 'CROSSDESK_MACOS_KEYCHAIN=%s\n' "$KEYCHAIN_PATH"
    printf 'CROSSDESK_MACOS_SIGN_IDENTITY=%s\n' "$APP_IDENTITY"
    printf 'CROSSDESK_MACOS_INSTALLER_IDENTITY=%s\n' "$INSTALLER_IDENTITY"
} >> "$GITHUB_ENV"
# Keep notarization separate from packaging so CI can retain a signed package
# and the submission ID when Apple's processing exceeds the wait timeout.
printf 'notary_profile=%s\n' "$NOTARY_PROFILE" >> "$GITHUB_OUTPUT"
echo "Prepared Developer ID application and installer identities for team $APPLE_TEAM_ID."
