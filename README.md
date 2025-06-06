# chatwheel

Volume control for games and chat applications based on your headset's chatmix wheel position.

## Requirements

- Linux with PulseAudio or PipeWire
- HeadsetControl (for Chatmix wheel value)
- systemd (for auto-start service)

## Compatibility

This tool is designed for SteelSeries Arctis Nova series headsets with a chatmix wheel.
Tested only with:
- SteelSeries Arctis Nova 7 Wireless

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

# Add with wildcard patterns for games with version numbers
chatwheel --add "MyGame*,game"    # Matches "MyGame v1.2.3", "MyGame Beta", etc.
chatwheel --add "Counter-Strike*,game"  # Matches any Counter-Strike variant

# Remove an application (requires service restart)
chatwheel --remove Firefox
```

## Configuration

Configuration is stored in:
- User config: `~/.config/chatwheel/chatwheel.conf`
- System config: `/etc/chatwheel/chatwheel.conf`

### Pattern Matching

chatwheel supports wildcard patterns in application names to handle games that include version numbers or variable suffixes:

- `*` matches any number of characters
- `?` matches exactly one character
- Pattern matching is case-insensitive

Examples:
- `"MyGame*"` matches "MyGame v1.2.3", "MyGame Beta", "MyGame - Special Edition"
- `"Counter-Strike*"` matches "Counter-Strike 2", "Counter-Strike: Global Offensive"
- `"Discord*"` matches "Discord", "Discord PTB", "Discord Canary"

## How it Works

chatwheel monitors your headset's chatmix wheel position:
- Top (0): Game audio 100%, Chat audio 0%
- Middle (64): Both at 50%
- Bottom (128): Game audio 0%, Chat audio 100%

Volume changes are applied using a logarithmic scale to match human hearing perception. This provides more natural volume control with:
- More precise adjustments at lower volumes
- More gradual changes at higher volumes

## Uninstall

```bash
make uninstall
```
