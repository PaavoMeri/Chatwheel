#!/usr/bin/env bash
set -euo pipefail

if ! command -v headsetcontrol >/dev/null 2>&1; then
    echo "Error: HeadsetControl is not installed" >&2
    exit 1
fi

if [[ ! -f "chatwheel" ]]; then
    echo "Error: chatwheel binary is missing or is not a regular file" >&2
    exit 1
fi

if [[ ! -f "systemd/chatwheel.service" ]]; then
    echo "Error: systemd/chatwheel.service is missing or is not a regular file" >&2
    exit 1
fi

if [[ -n "${XDG_CONFIG_HOME:-}" ]]; then
    config_home=$XDG_CONFIG_HOME
elif [[ -n "${HOME:-}" ]]; then
    config_home="${HOME}/.config"
else
    echo "Error: HOME must be set when XDG_CONFIG_HOME is not set" >&2
    exit 1
fi

if [[ "$config_home" != /* ]]; then
    echo "Error: XDG_CONFIG_HOME or HOME must select an absolute config path" >&2
    exit 1
fi

readonly config_home
readonly user_config_dir="${config_home}/chatwheel"
readonly user_config_file="${user_config_dir}/chatwheel.conf"

mkdir -p -- "$user_config_dir"

if [[ -f "config/chatwheel.conf" ]]; then
    if [[ -e "$user_config_file" || -L "$user_config_file" ]]; then
        echo "Preserving existing user configuration: $user_config_file"
    else
        install -m 0644 -- "config/chatwheel.conf" "$user_config_file"
        echo "Created default user configuration: $user_config_file"
    fi
fi

sudo install -m 0755 -- "chatwheel" "/usr/local/bin/chatwheel"
sudo install -m 0644 -- \
    "systemd/chatwheel.service" "/etc/systemd/user/chatwheel.service"

systemctl --user daemon-reload
systemctl --user enable chatwheel
systemctl --user restart chatwheel

echo "Installation complete. Service is running with the installed binary."
