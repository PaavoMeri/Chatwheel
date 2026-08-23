#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
test_root="$(mktemp -d "${TMPDIR:-/tmp}/chatwheel-install-test.XXXXXX")"

cleanup() {
    case "$test_root" in
        "${TMPDIR:-/tmp}"/chatwheel-install-test.*)
            rm -rf -- "$test_root"
            ;;
        *)
            echo "installer test refused unsafe cleanup: $test_root" >&2
            return 1
            ;;
    esac
}
trap cleanup EXIT

fail() {
    echo "installer test failed: $*" >&2
    exit 1
}

mock_bin="${test_root}/mock-bin"
mkdir -p -- "$mock_bin"

cat >"$mock_bin/headsetcontrol" <<'EOF'
#!/usr/bin/env bash
exit 97
EOF

cat >"$mock_bin/sudo" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

[[ $# -eq 6 && "$1" == install && "$2" == -m && "$4" == -- ]] || {
    echo "sudo mock rejected unexpected arguments" >&2
    exit 97
}

case "$3:$5:$6" in
    "0755:chatwheel:/usr/local/bin/chatwheel")
        destination="${MOCK_SYSTEM_ROOT}/usr/local/bin/chatwheel"
        ;;
    "0644:systemd/chatwheel.service:/etc/systemd/user/chatwheel.service")
        destination="${MOCK_SYSTEM_ROOT}/etc/systemd/user/chatwheel.service"
        ;;
    *)
        echo "sudo mock rejected unexpected path" >&2
        exit 97
        ;;
esac

printf 'sudo' >>"$MOCK_SUDO_LOG"
printf ' %q' "$@" >>"$MOCK_SUDO_LOG"
printf '\n' >>"$MOCK_SUDO_LOG"
/usr/bin/mkdir -p -- "$(/usr/bin/dirname -- "$destination")"
/usr/bin/install -m "$3" -- "$5" "$destination"
EOF

cat >"$mock_bin/systemctl" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

if [[ $# -eq 2 && "$1" == --user && "$2" == daemon-reload ]]; then
    printf '%s\n' daemon-reload >>"$MOCK_SYSTEMCTL_LOG"
elif [[ $# -eq 3 && "$1" == --user && "$2" == enable &&
        "$3" == chatwheel ]]; then
    printf '%s\n' 'enable chatwheel' >>"$MOCK_SYSTEMCTL_LOG"
elif [[ $# -eq 3 && "$1" == --user && "$2" == restart &&
        "$3" == chatwheel ]]; then
    printf '%s\n' 'restart chatwheel' >>"$MOCK_SYSTEMCTL_LOG"
    [[ "${MOCK_RESTART_FAILURE:-0}" != 1 ]] || exit 23
else
    echo "systemctl mock rejected unexpected arguments" >&2
    exit 97
fi
EOF

chmod 755 -- "$mock_bin/headsetcontrol" "$mock_bin/sudo" \
    "$mock_bin/systemctl"

new_scenario() {
    local name=$1

    scenario_root="${test_root}/${name}"
    fixture="${scenario_root}/fixture"
    system_root="${scenario_root}/system-root"
    scenario_xdg="${scenario_root}/xdg"
    scenario_home="${scenario_root}/home"
    sudo_log="${scenario_root}/sudo.log"
    systemctl_log="${scenario_root}/systemctl.log"
    output_log="${scenario_root}/output.log"

    mkdir -p -- "$fixture/config" "$fixture/systemd"
    printf '%s\n' default-config >"$fixture/config/chatwheel.conf"
    printf '%s\n' new-binary >"$fixture/chatwheel"
    printf '%s\n' new-unit >"$fixture/systemd/chatwheel.service"
    : >"$sudo_log"
    : >"$systemctl_log"
    : >"$output_log"
}

run_installer() {
    local xdg=$1
    local home=$2

    (
        cd -- "$fixture"
        PATH="${mock_bin}:/usr/bin:/bin" \
            XDG_CONFIG_HOME="$xdg" \
            HOME="$home" \
            MOCK_SYSTEM_ROOT="$system_root" \
            MOCK_SUDO_LOG="$sudo_log" \
            MOCK_SYSTEMCTL_LOG="$systemctl_log" \
            MOCK_RESTART_FAILURE="${MOCK_RESTART_FAILURE:-0}" \
            /bin/bash "$repo_root/scripts/install.sh"
    ) >"$output_log" 2>&1
}

capture_installer_status() {
    if run_installer "$@"; then
        installer_status=0
    else
        installer_status=$?
    fi
}

assert_content() {
    local expected=$1
    local path=$2

    printf '%s\n' "$expected" | cmp - "$path" ||
        fail "$path has unexpected content"
}

assert_systemctl_calls() {
    printf '%s\n' daemon-reload 'enable chatwheel' 'restart chatwheel' |
        cmp - "$systemctl_log" || fail "unexpected systemctl calls"
}

assert_no_mutations() {
    local config_path=$1

    [[ ! -e "$config_path" && ! -L "$config_path" ]] ||
        fail "preflight changed the user config path"
    [[ ! -e "$system_root" ]] || fail "preflight changed the system root"
    [[ ! -s "$sudo_log" && ! -s "$systemctl_log" ]] ||
        fail "preflight invoked sudo or systemctl"
}

# Mocks reject operations outside the installer's exact command contract.
if MOCK_SYSTEM_ROOT="$test_root/reject" MOCK_SUDO_LOG="$test_root/reject.log" \
    "$mock_bin/sudo" rm -rf -- / >/dev/null 2>&1; then
    fail "sudo mock accepted an unexpected command"
fi
if MOCK_SYSTEMCTL_LOG="$test_root/reject.log" \
    "$mock_bin/systemctl" --user start chatwheel >/dev/null 2>&1; then
    fail "systemctl mock accepted start"
fi

# Fresh XDG install creates mode-0644 config and updates binary and unit.
new_scenario fresh
mkdir -p -- "$system_root/usr/local/bin" "$system_root/etc/systemd/user"
printf '%s\n' old-binary >"$system_root/usr/local/bin/chatwheel"
printf '%s\n' old-unit >"$system_root/etc/systemd/user/chatwheel.service"
run_installer "$scenario_xdg" "$scenario_home"
user_config="${scenario_xdg}/chatwheel/chatwheel.conf"
assert_content default-config "$user_config"
[[ "$(stat -c %a "$user_config")" == 644 ]] || fail "wrong config mode"
assert_content new-binary "$system_root/usr/local/bin/chatwheel"
assert_content new-unit "$system_root/etc/systemd/user/chatwheel.service"
[[ ! -e "$scenario_home/.config/chatwheel/chatwheel.conf" ]] ||
    fail "installer ignored XDG_CONFIG_HOME"
[[ ! -e "$system_root/etc/chatwheel" ]] || fail "system config was installed"
assert_systemctl_calls

# Existing regular config is preserved byte-for-byte and metadata-for-metadata.
new_scenario preserve
user_config="${scenario_xdg}/chatwheel/chatwheel.conf"
mkdir -p -- "$(dirname -- "$user_config")"
printf '%s\n' sentinel >"$user_config"
chmod 0600 -- "$user_config"
touch -t 202001020304.05 -- "$user_config"
mode_before="$(stat -c %a "$user_config")"
mtime_before="$(stat -c %Y "$user_config")"
run_installer "$scenario_xdg" "$scenario_home"
assert_content sentinel "$user_config"
[[ "$(stat -c %a "$user_config")" == "$mode_before" ]] ||
    fail "existing config mode changed"
[[ "$(stat -c %Y "$user_config")" == "$mtime_before" ]] ||
    fail "existing config mtime changed"

# Other existing `-e` target types are also preserved.
for existing_type in empty directory valid-symlink; do
    new_scenario "existing-${existing_type}"
    user_config="${scenario_xdg}/chatwheel/chatwheel.conf"
    mkdir -p -- "$(dirname -- "$user_config")"
    case "$existing_type" in
        empty) : >"$user_config" ;;
        directory) mkdir -- "$user_config" ;;
        valid-symlink)
            printf '%s\n' link-target >"${scenario_root}/link-target"
            ln -s -- "${scenario_root}/link-target" "$user_config"
            ;;
    esac
    run_installer "$scenario_xdg" "$scenario_home"
    case "$existing_type" in
        empty) [[ ! -s "$user_config" ]] || fail "empty config changed" ;;
        directory) [[ -d "$user_config" ]] || fail "config directory changed" ;;
        valid-symlink)
            [[ -L "$user_config" ]] || fail "valid config symlink changed"
            assert_content link-target "${scenario_root}/link-target"
            ;;
    esac
done

# A dangling symlink counts as an existing config and remains untouched.
new_scenario dangling
user_config="${scenario_xdg}/chatwheel/chatwheel.conf"
missing_target="${scenario_root}/missing-target"
mkdir -p -- "$(dirname -- "$user_config")"
ln -s -- "$missing_target" "$user_config"
run_installer "$scenario_xdg" "$scenario_home"
[[ -L "$user_config" && "$(readlink -- "$user_config")" == "$missing_target" ]] ||
    fail "dangling config symlink changed"
[[ ! -e "$missing_target" ]] || fail "dangling symlink target was created"

# Empty XDG_CONFIG_HOME falls back to absolute HOME/.config.
new_scenario home-fallback
run_installer "" "$scenario_home"
assert_content default-config \
    "$scenario_home/.config/chatwheel/chatwheel.conf"

# Each mandatory preflight failure happens before the first mutation.
new_scenario missing-headsetcontrol
empty_path="${scenario_root}/empty-path"
mkdir -p -- "$empty_path"
if (cd -- "$fixture" && PATH="$empty_path" XDG_CONFIG_HOME="$scenario_xdg" \
    HOME="$scenario_home" /bin/bash "$repo_root/scripts/install.sh") \
    >"$output_log" 2>&1; then
    fail "missing HeadsetControl succeeded"
fi
assert_no_mutations "$scenario_xdg/chatwheel"

for missing_source in chatwheel systemd/chatwheel.service; do
    new_scenario "missing-${missing_source//\//-}"
    rm -f -- "$fixture/$missing_source"
    capture_installer_status "$scenario_xdg" "$scenario_home"
    [[ "$installer_status" != 0 ]] || fail "missing $missing_source succeeded"
    assert_no_mutations "$scenario_xdg/chatwheel"
done

new_scenario relative-config-home
capture_installer_status relative-config "$scenario_home"
[[ "$installer_status" != 0 ]] || fail "relative XDG_CONFIG_HOME succeeded"
assert_no_mutations "$fixture/relative-config/chatwheel"

new_scenario missing-config-home
if (cd -- "$fixture" && env -u XDG_CONFIG_HOME -u HOME \
    PATH="${mock_bin}:/usr/bin:/bin" MOCK_SYSTEM_ROOT="$system_root" \
    MOCK_SUDO_LOG="$sudo_log" MOCK_SYSTEMCTL_LOG="$systemctl_log" \
    /bin/bash "$repo_root/scripts/install.sh") >"$output_log" 2>&1; then
    fail "missing HOME and XDG_CONFIG_HOME succeeded"
fi
assert_no_mutations "$scenario_xdg/chatwheel"

# The optional source config may be absent without blocking other updates.
new_scenario missing-source-config
rm -f -- "$fixture/config/chatwheel.conf"
run_installer "$scenario_xdg" "$scenario_home"
[[ ! -e "$scenario_xdg/chatwheel/chatwheel.conf" ]] ||
    fail "missing source created a user config"
assert_content new-binary "$system_root/usr/local/bin/chatwheel"
assert_content new-unit "$system_root/etc/systemd/user/chatwheel.service"
assert_systemctl_calls

# Restart failure propagates after the exact expected systemctl sequence.
new_scenario restart-failure
MOCK_RESTART_FAILURE=1 capture_installer_status "$scenario_xdg" "$scenario_home"
[[ "$installer_status" == 23 ]] ||
    fail "restart failure exited with $installer_status"
assert_systemctl_calls
if grep -Fqx \
    'Installation complete. Service is running with the installed binary.' \
    "$output_log"; then
    fail "success was reported after restart failure"
fi

echo "installer tests passed"
