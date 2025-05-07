#!/bin/bash
set -e  # Exit on error

# Check if headsetcontrol is installed
if ! command -v headsetcontrol &> /dev/null; then
    echo "Error: HeadsetControl is not installed"
    exit 1
fi

# Create directories
sudo mkdir -p /etc/chatwheel
mkdir -p ~/.config/chatwheel

# Install binary
sudo install -m 755 chatwheel /usr/local/bin/chatwheel

# Install config files only if they exist
if [ -f config/chatwheel.conf ]; then
    install -m 644 config/chatwheel.conf ~/.config/chatwheel/chatwheel.conf
    sudo install -m 644 config/chatwheel.conf /etc/chatwheel/chatwheel.conf
fi

# Install systemd service
sudo install -m 644 systemd/chatwheel.service /etc/systemd/user/

# Reload systemd and enable service
systemctl --user daemon-reload
systemctl --user enable chatwheel
systemctl --user start chatwheel

echo "Installation complete. Service started."
