#!/bin/bash
# Fetch SPKI pins for all configured providers
# Run when certs rotate (~90 days) or when adding new providers

set -euo pipefail

HOSTS=(
    "api.anthropic.com"
    "api.openai.com"
    "generativelanguage.googleapis.com"
    "api.x.ai"
    "openrouter.ai"
    "api.brave.com"
    "api.tavily.com"
)

echo "// Auto-generated pins - $(date)"
echo "// Update curl_security.h with these values"
echo ""

for host in "${HOSTS[@]}"; do
    pin=$(openssl s_client -servername "$host" -connect "$host:443" < /dev/null 2>/dev/null \
        | openssl x509 -pubkey -noout 2>/dev/null \
        | openssl pkey -pubin -outform der 2>/dev/null \
        | openssl dgst -sha256 -binary 2>/dev/null \
        | base64 2>/dev/null)
    if [[ -n "$pin" ]]; then
        echo "if (host == \"$host\") return \"sha256/$pin\";"
    else
        echo "// FAILED: $host" >&2
    fi
done
