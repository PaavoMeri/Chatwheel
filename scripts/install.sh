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

# Create default config if it doesn't exist
if [ ! -f config/chatwheel.conf ]; then
    mkdir -p config
    cat > config/chatwheel.conf << EOL
# Game Applications (0)
FMOD Audio,0        # Most Unity games
steam_app,0         # Steam games
java,0             # Minecraft
wine,0             # Windows games via Wine/Proton
mpv,0              # Video player

# Chat Applications (1)
Discord,1          # Discord
TeamSpeak,1        # TeamSpeak
easyeffects,1      # Audio processing
firefox,1          # Browser-based chat
EOL
fi

# Install config
install -m 644 config/chatwheel.conf ~/.config/chatwheel/chatwheel.conf
sudo install -m 644 config/chatwheel.conf /etc/chatwheel/chatwheel.conf

# Install systemd service
sudo install -m 644 systemd/chatwheel.service /etc/systemd/user/

# Reload systemd and enable service
systemctl --user daemon-reload
systemctl --user enable chatwheel
systemctl --user start chatwheel

echo "Installation complete. Service started."
