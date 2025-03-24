# chatwheel

Automatic volume control for games and chat applications based on your headset's chatmix wheel position.

## Requirements

- Linux with PulseAudio or PipeWire
- HeadsetControl (for SteelSeries headsets)
- systemd (for auto-start service)

## Compatibility

This tool is designed for SteelSeries Arctis Nova series headsets with a chatmix wheel.
Currently tested only with:
- SteelSeries Arctis Nova 7 Wireless

Other Arctis Nova models might work but are untested. Please report your experience with other models.

## Installation

```bash
# Build
make clean && make

# Make install script executable
chmod +x scripts/install.sh

# Install
make install
```

## Usage

The service starts automatically after installation. To manage applications:

```bash
# List running applications that can be controlled
chatwheel --list-new

# List currently configured applications
chatwheel --list

# Add an application (requires service restart)
chatwheel --add "Firefox,game"    # For game audio
chatwheel --add "Discord,chat"    # For chat audio

# Remove an application (requires service restart)
chatwheel --remove Firefox

# Restart service to apply changes
chatwheel --restart
```

**Note:** After adding or removing applications, the service needs to be restarted
for changes to take effect.

## Configuration

Configuration is stored in:
- User config: `~/.config/chatwheel/chatwheel.conf`
- System config: `/etc/chatwheel/chatwheel.conf`

Format:
```
application_name,type
```
Where type is:
- 0: Game audio (full volume at chatmix top)
- 1: Chat audio (full volume at chatmix bottom)

## How it Works

chatwheel monitors your headset's chatmix wheel position:
- Top (0): Game audio 100%, Chat audio 0%
- Middle (64): Both at 50%
- Bottom (128): Game audio 0%, Chat audio 100%

## Troubleshooting

1. Check service status:
```bash
systemctl --user status chatwheel
```

2. View application list:
```bash
chatwheel --list
```

3. Test volume control:
```bash
make test
./test
```

## Uninstall

```bash
make uninstall
```

## Warranty Disclaimer

This software is provided "as is" and without any express or implied warranties, including, without limitation, the implied warranties of merchantability and fitness for a particular purpose. The authors assume no liability for damages, direct or consequential, which may result from the use of this software.

This is free software, and you are welcome to redistribute it under the terms of the GNU General Public License v3.0.