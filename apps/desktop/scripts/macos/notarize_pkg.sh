#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 || -z "${CROSSDESK_MACOS_NOTARY_PROFILE:-}" ]]; then
    echo "Usage: CROSSDESK_MACOS_NOTARY_PROFILE=<keychain-profile> $0 <signed.pkg>" >&2
    echo "Create the profile with: xcrun notarytool store-credentials <keychain-profile>" >&2
    exit 2
fi

PACKAGE_FILE="$1"
if [[ ! -f "$PACKAGE_FILE" || "$PACKAGE_FILE" != *.pkg ]]; then
    echo "Expected a signed .pkg file: $PACKAGE_FILE" >&2
    exit 1
fi

pkgutil --check-signature "$PACKAGE_FILE"
NOTARY_ARGS=(--keychain-profile "$CROSSDESK_MACOS_NOTARY_PROFILE")
if [[ -n "${CROSSDESK_MACOS_KEYCHAIN:-}" ]]; then
    NOTARY_ARGS+=(--keychain "$CROSSDESK_MACOS_KEYCHAIN")
fi
UPLOAD_ARGS=()
case "${CROSSDESK_MACOS_NOTARY_S3_ACCELERATION:-true}" in
    true) UPLOAD_ARGS+=(--s3-acceleration) ;;
    false) UPLOAD_ARGS+=(--no-s3-acceleration) ;;
    *)
        echo "CROSSDESK_MACOS_NOTARY_S3_ACCELERATION must be true or false." >&2
        exit 2
        ;;
esac

# Retain the submission response and log beside the installer for diagnostics
# and for resuming a submission if Apple's service takes longer than expected.
SUBMIT_FILE="${PACKAGE_FILE}.notary-submit.json"
RESULT_FILE="${PACKAGE_FILE}.notary.json"
LOG_FILE="${PACKAGE_FILE}.notary-log.json"
SUBMIT_STATUS=0
xcrun notarytool submit "$PACKAGE_FILE" "${NOTARY_ARGS[@]}" "${UPLOAD_ARGS[@]}" \
    --no-wait --output-format json > "$SUBMIT_FILE" || SUBMIT_STATUS=$?
cat "$SUBMIT_FILE"
SUBMISSION_ID="$(plutil -extract id raw -o - "$SUBMIT_FILE" 2>/dev/null || true)"
if [[ "$SUBMIT_STATUS" -ne 0 || -z "$SUBMISSION_ID" ]]; then
    echo "Notarization upload failed; response: $SUBMIT_FILE" >&2
    echo "Check notarytool history before retrying; Apple may have allocated a submission ID." >&2
    exit 1
fi

echo "Waiting for notarization: $SUBMISSION_ID"
WAIT_STATUS=0
xcrun notarytool wait "$SUBMISSION_ID" "${NOTARY_ARGS[@]}" \
    --timeout "${CROSSDESK_MACOS_NOTARY_TIMEOUT:-30m}" \
    --output-format json > "$RESULT_FILE" || WAIT_STATUS=$?
cat "$RESULT_FILE"
STATUS="$(plutil -extract status raw -o - "$RESULT_FILE" 2>/dev/null || true)"
xcrun notarytool log "$SUBMISSION_ID" "${NOTARY_ARGS[@]}" "$LOG_FILE" || true
if [[ "$WAIT_STATUS" -ne 0 || "$STATUS" != "Accepted" ]]; then
    echo "Notarization did not complete successfully (status: ${STATUS:-unknown})." >&2
    echo "Submission: ${SUBMISSION_ID:-unavailable}; response: $RESULT_FILE" >&2
    exit 1
fi

xcrun stapler staple "$PACKAGE_FILE"
xcrun stapler validate "$PACKAGE_FILE"
spctl --assess --type install --verbose=2 "$PACKAGE_FILE"
echo "Notarized and stapled: $PACKAGE_FILE"
