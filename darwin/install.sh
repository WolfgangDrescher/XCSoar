#!/bin/bash
set -euo pipefail

# Load environment variables from .env file if it exists
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -f "$SCRIPT_DIR/.env" ]]; then
  # shellcheck disable=SC1091
  source "$SCRIPT_DIR/.env"
fi

# Configuration
IPA_SIGNED_PATH="${IOS_SIGNED_IPA_PATH:-$(pwd)/output/IOS64/xcsoar-signed.ipa}"
DEVICE_NAME="${IOS_DEVICE_NAME:-}"
BUNDLE_ID="${IOS_BUNDLE_ID:-}"

# The bundle identifier depends on TESTING and IOS_APP_BUNDLE_IDENTIFIER (see
# build/ios.mk); read it back from the IPA instead of duplicating it here
read_bundle_id() {
  local plist
  plist="$(mktemp)"
  unzip -p "$IPA_SIGNED_PATH" 'Payload/*.app/Info.plist' > "$plist" 2>/dev/null &&
    /usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$plist" 2>/dev/null
  local status=$?
  rm -f "$plist"
  return $status
}

# Validate required environment variables
if [[ -z "$DEVICE_NAME" ]]; then
  echo "❌ IOS_DEVICE_NAME not set"
  echo "Set it via: export IOS_DEVICE_NAME='Your Device Name'"
  echo "Or configure it in $SCRIPT_DIR/.env (see .env.example)"
  echo "To find device name run: xcrun devicectl list devices"
  exit 1
fi

# Input validation
if [ ! -f "$IPA_SIGNED_PATH" ]; then
  echo "❌ Signed IPA not found: $IPA_SIGNED_PATH"
  echo "   Please run the signing script first."
  exit 1
fi

if [[ -z "$BUNDLE_ID" ]] && ! BUNDLE_ID="$(read_bundle_id)"; then
  echo "❌ Could not read the bundle identifier from $IPA_SIGNED_PATH"
  echo "Set it via: export IOS_BUNDLE_ID='org.example.XCSoar'"
  exit 1
fi

# Install app on device
echo "📱 Installing app on device '$DEVICE_NAME'..."
if ! xcrun devicectl device install app "$IPA_SIGNED_PATH" --device "$DEVICE_NAME"; then
  echo "❌ Installation failed."
  exit 1
fi

echo "✅ App installed successfully"

# Launch app with console attached to view logs (Ctrl+C to stop)
if [[ "${IOS_SHOW_LOGS:-1}" != "0" ]]; then
  echo "📄 Launching '$BUNDLE_ID' with console attached (Ctrl+C to stop)..."
  if ! xcrun devicectl device process launch --console \
      --terminate-existing --device "$DEVICE_NAME" "$BUNDLE_ID"; then
    echo "⚠️  Failed to launch app with console (app may already be running or bundle ID may be incorrect)"
  fi
fi
