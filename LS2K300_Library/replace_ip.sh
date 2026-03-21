#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

find "$SCRIPT_DIR" -type f -name "build.sh" | while read -r file; do
    if [ $# -eq 0 ]; then
        sed -i '/scp -O/d' "$file"
        echo "Removed scp from: $file"
    else
        NEW_IP="$1"
        if grep -q "scp -O" "$file"; then
            sed -i "s|\${REMOTE_IP}|${NEW_IP}|g" "$file"
            echo "Updated: $file"
        fi
    fi
done

if [ $# -eq 0 ]; then
    echo "All scp commands removed"
else
    echo "IP address replaced to: $1"
fi
