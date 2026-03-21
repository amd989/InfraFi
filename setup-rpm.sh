#!/bin/bash
set -e
echo "Adding InfraFi YUM/DNF repository..."
rpm --import https://amd989.github.io/InfraFi/gpg.key
cat > /etc/yum.repos.d/infrafid.repo <<REPOEOF
[infrafid]
name=infrafid
baseurl=https://amd989.github.io/InfraFi/rpm/
enabled=1
gpgcheck=1
gpgkey=https://amd989.github.io/InfraFi/gpg.key
REPOEOF
echo "Done! You can now run: sudo dnf install infrafid"
