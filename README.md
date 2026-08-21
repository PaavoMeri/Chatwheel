# Chatwheel

Chatwheel brings the SteelSeries ChatMix wheel to Linux by adjusting the volumes of configured game and chat audio streams.

The project is under active development. The current version provides a command-line daemon and uses PulseAudio-compatible sink inputs. A graphical interface is planned but not yet implemented.

## Requirements

- Linux
- PulseAudio or PipeWire-Pulse
- [HeadsetControl](https://github.com/Sapd/HeadsetControl) available in `PATH`
- A headset whose ChatMix value is supported by HeadsetControl
- systemd for the included user service
- GCC, GNU Make, `pkg-config`, and libpulse development files for building

## Compatibility

Chatwheel is intended for SteelSeries headsets with a hardware ChatMix wheel. It has currently been tested only with:

- SteelSeries Arctis Nova 7 Wireless
- PipeWire-Pulse

Other headsets and native PulseAudio installations may work but have not yet been verified.

## Build and installation

Build the program from source:

```sh
make clean
make
```

Run the tests:

```sh
make test
```

Install the binary and systemd user service using the current installation script:

```sh
chmod +x scripts/install.sh
make install
```

The installer places the executable at `/usr/local/bin/chatwheel` and enables the systemd user service. The installation process is currently intended for development and has not yet been packaged for Linux distributions.

## Configuration

Chatwheel reads its configuration from:

```text
$XDG_CONFIG_HOME/chatwheel/chatwheel.conf
```

If `XDG_CONFIG_HOME` is not set, it uses:

```text
$HOME/.config/chatwheel/chatwheel.conf
```

There is currently no implemented fallback to `/etc/chatwheel/chatwheel.conf`.

Each line contains an application pattern and its group:

```text
PATTERN,GROUP
```

The group value is:

- `0` for Game
- `1` for Chat

For example:

```text
Firefox,0
Discord,1
Counter-Strike*,0
```

The current configuration supports at most 32 entries. The daemon loads the configuration when it starts, so changes require a service restart.

## Usage

Show all available commands:

```sh
chatwheel --help
```

List configured application patterns:

```sh
chatwheel --list
```

Add or update an application pattern:

```sh
chatwheel --add "Firefox,0"
chatwheel --add "Discord,1"
```

Remove an application pattern:

```sh
chatwheel --remove Firefox
```

Restart the service after changing the configuration:

```sh
chatwheel --restart
```

Inspect the individual sink inputs and their raw identity properties:

```sh
chatwheel --list-streams
```

Inspect the logical applications derived from those streams and the stream indexes grouped into each application:

```sh
chatwheel --list-active
```

Both commands are read-only diagnostics and do not change stream volumes. `--list-streams` shows individual sink inputs and their raw `application.id`, `application.name`, `application.process.binary`, and `node.name` properties. `--list-active` shows the derived canonical identity, display name, stream indexes, and current Game, Chat, or Unassigned classification from the loaded configuration.

The legacy command below performs a separate, one-time listing:

```sh
chatwheel --list-new
```

It currently has known limitations: it does not load the configuration before comparing names and only inspects `application.name`. Its output should therefore be treated as diagnostic rather than an accurate list of unconfigured applications.

## Pattern matching

Application patterns are matched case-insensitively against `application.name` and `application.process.binary`.

- `*` matches any number of characters
- `?` matches exactly one character

Examples:

```text
MyGame*
Counter-Strike*
Discord?
```

The first matching configuration entry determines whether a newly handled stream belongs to the Game or Chat group.

## How it works

When running, Chatwheel repeatedly reads the headset's ChatMix value by executing:

```sh
headsetcontrol --output json
```

The raw value is expected to be between 0 and 128. Chatwheel converts it into opposite Game and Chat weights:

| ChatMix value | Game weight | Chat weight |
|---:|---:|---:|
| 0 | 100% | 0% |
| 64 | 50% | 50% |
| 128 | 0% | 100% |

Before being applied to a PulseAudio stream, each weight is converted using a logarithmic curve:

```text
(10^weight - 1) / 9
```

Because of this conversion, the center position produces approximately 24% PulseAudio volume for both groups, not 50% absolute PulseAudio volume.

Chatwheel sets an absolute volume on every matching stream and applies the same value to all of its channels. It does not currently preserve a stream's previous volume or channel balance.

The daemon keeps in-memory inventories of active sink inputs and derived logical applications. It takes an initial snapshot when connecting to PulseAudio and then tracks new, changed, and removed streams. The inventories are not persisted to disk and are exposed through the diagnostic `--list-streams` and `--list-active` commands.

## Current limitations

- no graphical interface
- no automatic PulseAudio reconnect after a running connection is lost
- no structured daemon control API
- no live configuration reload
- no persistent application inventory
- application identification depends on optional PulseAudio properties
- only `application.name` and `application.process.binary` are currently used for matching
- `--status` is shown in the help output but is not yet implemented
- HeadsetControl is launched as a subprocess for each poll

## Uninstall

```sh
make uninstall
```

The current uninstall process removes the Chatwheel user configuration directory. Back up your configuration before uninstalling if you want to keep it.

## License

Chatwheel is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE).
