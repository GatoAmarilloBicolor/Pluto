#!/bin/bash
# fetch_pins.sh - Update certificate pins for known providers
# 
# Certificate pins rotate with certificate renewal (~90 days).
# Run this script periodically to update the pins in curl_security.h
#
# Usage: ./fetch_pins.sh [--dry-run] [--update-file]
#
# Options:
#   --dry-run      Show new pins without updating file
#   --update-file  Update curl_security.h with new pins
#
# Requirements: openssl, curl

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CURL_SECURITY_H="$PROJECT_ROOT/src/curl_security.h"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

DRY_RUN=false
UPDATE_FILE=false

for arg in "$@"; do
    case $arg in
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --update-file)
            UPDATE_FILE=true
            shift
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Usage: $0 [--dry-run] [--update-file]"
            exit 1
            ;;
    esac
done

# Known provider hosts
declare -A HOSTS=(
    ["api.anthropic.com"]="Anthropic API"
    ["api.openai.com"]="OpenAI API"
    ["generativelanguage.googleapis.com"]="Google Generative AI"
    ["api.x.ai"]="xAI API"
    ["openrouter.ai"]="OpenRouter"
    ["api.brave.com"]="Brave Search API"
    ["api.tavily.com"]="Tavily API"
)

# Fetch pin for a host
fetch_pin() {
    local host="$1"
    local port="${2:-443}"
    
    # Get certificate and extract public key pin
    local pin
    pin=$(openssl s_client -servername "$host" -connect "$host:$port" </dev/null 2>/dev/null | \
          openssl x509 -pubkey -noout 2>/dev/null | \
          openssl pkey -pubin -outform der 2>/dev/null | \
          openssl dgst -sha256 -binary 2>/dev/null | \
          base64 2>/dev/null)
    
    if [[ -n "$pin" ]]; then
        echo "sha256/$pin"
    else
        echo "ERROR"
    fi
}

# Get current pin from curl_security.h
get_current_pin() {
    local host="$1"
    grep -A 20 "GetPinForHost" "$CURL_SECURITY_H" | \
        grep -E "if \(host == \"$host\"\)" | \
        sed -E 's/.*return "([^"]+)".*/\1/'
}

echo "Fetching certificate pins for known providers..."
echo ""

# Store new pins
declare -A NEW_PINS
declare -A CURRENT_PINS

for host in "${!HOSTS[@]}"; do
    echo -n "Checking ${HOSTS[$host]} ($host)... "
    
    current=$(get_current_pin "$host")
    CURRENT_PINS[$host]="$current"
    
    new=$(fetch_pin "$host")
    NEW_PINS[$host]="$new"
    
    if [[ "$new" == "ERROR" ]]; then
        echo -e "${RED}FAILED${NC}"
    elif [[ "$new" == "$current" ]]; then
        echo -e "${GREEN}UP TO DATE${NC} ($new)"
    else
        echo -e "${YELLOW}CHANGED${NC}"
        echo "  Current: $current"
        echo "  New:     $new"
    fi
done

echo ""
echo "Summary:"
echo "========"

CHANGED=false
for host in "${!HOSTS[@]}"; do
    current="${CURRENT_PINS[$host]}"
    new="${NEW_PINS[$host]}"
    
    if [[ "$new" == "ERROR" ]]; then
        echo -e "  ${HOSTS[$host]} ($host): ${RED}FAILED TO FETCH${NC}"
    elif [[ "$new" != "$current" ]]; then
        echo -e "  ${HOSTS[$host]} ($host): ${YELLOW}PIN CHANGED${NC}"
        CHANGED=true
    else
        echo -e "  ${HOSTS[$host]} ($host): ${GREEN}OK${NC}"
    fi
done

echo ""

if [[ "$CHANGED" == "false" ]]; then
    echo -e "${GREEN}All pins are up to date.${NC}"
    exit 0
fi

if [[ "$DRY_RUN" == "true" ]]; then
    echo "Dry run mode - no changes made."
    echo "Run with --update-file to apply changes."
    exit 0
fi

if [[ "$UPDATE_FILE" != "true" ]]; then
    echo "Run with --update-file to apply changes to $CURL_SECURITY_H"
    exit 1
fi

# Update the file
echo "Updating $CURL_SECURITY_H..."

# Create backup
cp "$CURL_SECURITY_H" "$CURL_SECURITY_H.bak"

# Update each pin
for host in "${!HOSTS[@]}"; do
    new="${NEW_PINS[$host]}"
    if [[ "$new" != "ERROR" ]]; then
        # Escape special characters for sed
        escaped_new=$(printf '%s\n' "$new" | sed 's/[[\.*^$()+?{|/]/\\&/g')
        # Update the line for this host
        sed -i "s|if (host == \"$host\") return \"[^\"]*\";|if (host == \"$host\") return \"$escaped_new\";|" "$CURL_SECURITY_H"
        echo "  Updated $host"
    fi
done

echo ""
echo -e "${GREEN}Certificate pins updated successfully!${NC}"
echo "Backup saved as $CURL_SECURITY_H.bak"
echo ""
echo "IMPORTANT: Rebuild the project after updating pins:"
echo "  make clean && make"