#!/bin/bash
set -euo pipefail

# Load environment variables from .env file if it exists
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -f "$SCRIPT_DIR/.env" ]]; then
  # shellcheck disable=SC1091
  source "$SCRIPT_DIR/.env"
fi

# Input configuration (with defaults)
IPA_PATH="${IOS_IPA_PATH:-$(pwd)/output/IOS64/xcsoar.ipa}"
PROFILE_PATH="${IOS_PROFILE_PATH:-}"
CERTIFICATE_NAME="${APPLE_DISTRIBUTION_CERTIFICATE_NAME:-}"

# Output configuration
IPA_SIGNED_PATH="${IOS_SIGNED_IPA_PATH:-$(pwd)/output/IOS64/xcsoar-signed.ipa}"
# The signed app bundle is kept next to the signed IPA, because Xcode and
# devicectl install and launch a bundle, not a zipped IPA.
SIGNED_APP_ROOT="${IOS_SIGNED_APP_ROOT:-$(pwd)/output/IOS64/signed}"

# Validate required environment variables
if [[ -z "$PROFILE_PATH" ]]; then
  echo "❌ IOS_PROFILE_PATH not set"
  echo "Set it via: export IOS_PROFILE_PATH=/path/to/profile.mobileprovision"
  echo "Or configure it in $SCRIPT_DIR/.env (see .env.example)"
  exit 1
fi

if [[ -z "$CERTIFICATE_NAME" ]]; then
  echo "❌ APPLE_DISTRIBUTION_CERTIFICATE_NAME not set"
  echo "Set it via: export APPLE_DISTRIBUTION_CERTIFICATE_NAME='Apple Distribution: ...'"
  echo "Or configure it in $SCRIPT_DIR/.env (see .env.example)"
  exit 1
fi

# Guard against missing build artefact
if [[ ! -f "$IPA_PATH" ]]; then
  echo "❌ IPA not found: $IPA_PATH"
  exit 1
fi

# Create temporary directories
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT
SIGNED_IPA="$TMP_DIR/signed.ipa"
ENTITLEMENTS_TMP="$TMP_DIR/entitlements.plist"

# Unzip IPA into the persistent output directory
rm -rf "$SIGNED_APP_ROOT"
mkdir -p "$SIGNED_APP_ROOT"
unzip -q "$IPA_PATH" -d "$SIGNED_APP_ROOT"

# Locate .app inside Payload
APP_PATH=$(find "$SIGNED_APP_ROOT/Payload" -name "*.app" -type d | head -n 1)

if [ ! -d "$APP_PATH" ]; then
  echo "❌ .app not found in IPA"
  exit 1
fi

# Embed provisioning profile
cp "$PROFILE_PATH" "$APP_PATH/embedded.mobileprovision"

# Extract entitlements from provisioning profile
security cms -D -i "$PROFILE_PATH" > "$TMP_DIR/profile.plist"
if ! /usr/libexec/PlistBuddy -x -c "Print :Entitlements" "$TMP_DIR/profile.plist" > "$ENTITLEMENTS_TMP"; then
  echo "❌ Failed to extract entitlements from provisioning profile"
  exit 1
fi

# Sign the app
echo "🔏 Signing with certificate '$CERTIFICATE_NAME'..."
codesign -f -s "$CERTIFICATE_NAME" --entitlements "$ENTITLEMENTS_TMP" "$APP_PATH"

# Verify signature
if ! codesign --verify --deep --strict "$APP_PATH"; then
  echo "❌ Code signing verification failed"
  exit 1
fi

# Repackage IPA (without changing working directory)
(
  cd "$SIGNED_APP_ROOT"
  zip -qr "$SIGNED_IPA" Payload
)

# Move signed IPA to output
mv "$SIGNED_IPA" "$IPA_SIGNED_PATH"

echo "✅ Signed IPA created at: $IPA_SIGNED_PATH"
echo "✅ Signed app bundle at: $APP_PATH"
