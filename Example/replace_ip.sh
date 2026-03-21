#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 <new_ip_address>"
    exit 1
fi

NEW_IP="$1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

find "$SCRIPT_DIR" -type f -name "build.sh" -exec grep -l "scp -O.*root@" {} \; | while read -r file; do
    sed -i "s|root@[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}:/home/root/|root@${NEW_IP}:/home/root/|g" "$file"
    echo "Updated: $file"
done

echo "IP address replaced to: $NEW_IP"
