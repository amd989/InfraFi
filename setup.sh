#!/bin/bash
set -e
echo "Adding InfraFi APT repository..."
curl -fsSL https://amd989.github.io/InfraFi/gpg.key | gpg --dearmor -o /usr/share/keyrings/infrafid.gpg
echo "deb [signed-by=/usr/share/keyrings/infrafid.gpg] https://amd989.github.io/InfraFi stable main" > /etc/apt/sources.list.d/infrafid.list
apt-get update
echo "Done! You can now run: sudo apt install infrafid"
