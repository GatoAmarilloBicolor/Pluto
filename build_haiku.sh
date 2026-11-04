#!/bin/bash
HAIKU_HOST="${1:-192.168.1.34}"
HAIKU_USER="${2:-user}"
REMOTE_DIR="src/Pluto"

echo "Sincronizando fuentes con $HAIKU_USER@$HAIKU_HOST:$REMOTE_DIR..."
rsync -avz --exclude '.git' --exclude 'build*' ./ "$HAIKU_USER@$HAIKU_HOST:$REMOTE_DIR/"

echo "Ejecutando make ci en Haiku..."
ssh "$HAIKU_USER@$HAIKU_HOST" "cd $REMOTE_DIR && make clean && make ci"
