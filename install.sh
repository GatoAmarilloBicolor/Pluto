#!/bin/sh
# Pluto 0.6.0 installer for Haiku
# Run from the extracted release directory.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN_DIR="$HOME/config/non-packaged/bin"
ADDON_DIR="$HOME/config/non-packaged/add-ons/Tracker"
SETTINGS_DIR="$HOME/config/settings/pluto"
MAN_DIR="$HOME/config/non-packaged/documentation/man/man1"
DATA_DIR="$SETTINGS_DIR"

echo "Installing Pluto 0.6.0..."
echo ""

# Create directories
mkdir -p "$BIN_DIR"
mkdir -p "$ADDON_DIR"
mkdir -p "$SETTINGS_DIR"
mkdir -p "$MAN_DIR"

# CLI binary
echo "  CLI        -> $BIN_DIR/pluto"
cp "$SCRIPT_DIR/Pluto_cli" "$BIN_DIR/pluto"
chmod 755 "$BIN_DIR/pluto"

# GUI binary
echo "  GUI        -> $BIN_DIR/Pluto"
cp "$SCRIPT_DIR/Pluto" "$BIN_DIR/Pluto"
chmod 755 "$BIN_DIR/Pluto"

# Tracker add-on
echo "  Tracker    -> $ADDON_DIR/Ask pluto"
cp "$SCRIPT_DIR/Ask pluto" "$ADDON_DIR/Ask pluto"
chmod 755 "$ADDON_DIR/Ask pluto"

# BeAPI knowledge index
if [ ! -f "$DATA_DIR/beapi-index.jsonl" ]; then
    echo "  BeAPI data -> $DATA_DIR/beapi-index.jsonl"
    cp "$SCRIPT_DIR/data/beapi-index.jsonl" "$DATA_DIR/"
else
    echo "  BeAPI data -> (already exists, skipping)"
fi

# Man page
echo "  Man page   -> $MAN_DIR/pluto.1"
cp "$SCRIPT_DIR/docs/pluto.1" "$MAN_DIR/"

# Example agents (don't overwrite existing)
if [ -d "$SCRIPT_DIR/examples/agents" ]; then
    for agent_dir in "$SCRIPT_DIR"/examples/agents/*/; do
        agent_name="$(basename "$agent_dir")"
        dest="$SETTINGS_DIR/agents/$agent_name"
        if [ ! -d "$dest" ]; then
            mkdir -p "$SETTINGS_DIR/agents"
            cp -r "$agent_dir" "$dest"
            echo "  Agent      -> $dest"
        else
            echo "  Agent      -> $agent_name (already installed, skipping)"
        fi
    done
fi

# Example routines (don't overwrite existing)
if [ -d "$SCRIPT_DIR/examples/routines" ]; then
    mkdir -p "$SETTINGS_DIR/routines"
    for routine in "$SCRIPT_DIR"/examples/routines/*.md; do
        name="$(basename "$routine")"
        dest="$SETTINGS_DIR/routines/$name"
        if [ ! -f "$dest" ]; then
            cp "$routine" "$dest"
            echo "  Routine    -> $dest"
        else
            echo "  Routine    -> $name (already exists, skipping)"
        fi
    done
fi

# Example provider profiles (don't overwrite existing)
if [ -d "$SCRIPT_DIR/examples/providers" ]; then
    mkdir -p "$SETTINGS_DIR/providers"
    for provider in "$SCRIPT_DIR"/examples/providers/*.conf; do
        name="$(basename "$provider")"
        dest="$SETTINGS_DIR/providers/$name"
        if [ ! -f "$dest" ]; then
            cp "$provider" "$dest"
            echo "  Provider   -> $dest"
        else
            echo "  Provider   -> $name (already exists, skipping)"
        fi
    done
fi

echo ""
echo "Done! Pluto 0.6.0 is installed."
echo ""
echo "Quick start:"
echo "  pluto login              # authenticate with Claude (OAuth)"
echo "  pluto                    # start interactive CLI"
echo "  Pluto                    # start GUI (or double-click in Tracker)"
echo "  pluto --help             # show all options"
echo ""
echo "To uninstall:"
echo "  rm '$BIN_DIR/pluto' '$BIN_DIR/Pluto'"
echo "  rm '$ADDON_DIR/Ask pluto'"
echo "  rm -rf '$SETTINGS_DIR'  # removes all settings and sessions"
