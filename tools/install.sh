#!/usr/bin/env bash
# SPDX-License-Identifier: ISC
#
# Copyright (C) 2026 Donnie V. Savage
#
# Stage this EIGRP project into an FRR checkout.

set -euo pipefail

script_name="$(basename "$0")"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
eigrp_root="$(cd "$script_dir/.." && pwd)"

frr_root=""
install_eigrpd=1
install_tests=1
dry_run=0

usage() {
	cat <<USAGE
usage: $script_name [options]

options:
  -frr-root PATH       FRR checkout root. Default inference order:
                       ../frr, ~/devel/frr, or parent when run from frr/eigrpd/tools.
  -no-eigrpd           Do not copy eigrpd/ into FRR.
  -no-tests            Do not copy test/frr/ into FRR tests/eigrpd/.
  -dry-run             Print the actions without copying files.
  -h, -help, --help    Show this help.

behavior:
  Installing eigrpd first removes the existing frr/eigrpd directory, then
  copies this project's eigrpd/ tree into place.

installs:
  eigrpd/      -> frr/eigrpd/
  test/frr/    -> frr/tests/eigrpd/
USAGE
}

fail() {
	echo "error: $*" >&2
	exit 1
}

require_command() {
	command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

is_abs_path() {
	case "$1" in
		/*) return 0 ;;
		*) return 1 ;;
	esac
}

resolve_existing_path() {
	local path="$1"
	if is_abs_path "$path"; then
		cd "$path" && pwd
	else
		cd "$PWD" && cd "$path" && pwd
	fi
}

infer_frr_root() {
	local candidate

	for candidate in \
		"$eigrp_root/../frr" \
		"$HOME/devel/frr" \
		"$eigrp_root/.."; do
		if [[ -d "$candidate/tests" && -f "$candidate/bootstrap.sh" ]]; then
			cd "$candidate" && pwd
			return 0
		fi
	done

	return 1
}

copy_tree() {
	local src="$1"
	local dst="$2"

	[[ -d "$src" ]] || fail "source directory not found: $src"

	if [[ "$dry_run" -eq 1 ]]; then
		echo "would sync: $src/ -> $dst/"
		return 0
	fi

	mkdir -p "$dst"
	rsync -a --delete \
		--exclude '.git/' \
		--exclude '.pytest_cache/' \
		--exclude '__pycache__/' \
		--exclude '__MACOSX/' \
		--exclude '.DS_Store' \
		--exclude '*.o' \
		--exclude '*.lo' \
		--exclude '*.la' \
		--exclude '*~' \
		--exclude 'obj/' \
		--exclude 'logs/' \
		"$src"/ "$dst"/
}

remove_destination_tree() {
	local dst="$1"

	[[ -n "$dst" ]] || fail "refusing to remove empty destination path"
	[[ -n "$frr_root" ]] || fail "FRR root is not set"

	case "$dst" in
		"$frr_root"|"$frr_root/")
			fail "refusing to remove FRR root: $dst"
			;;
		"$frr_root"/*)
			;;
		*)
			fail "refusing to remove path outside FRR root: $dst"
			;;
	esac

	if [[ "$dry_run" -eq 1 ]]; then
		echo "would remove: $dst"
		return 0
	fi

	rm -rf -- "$dst"
}

install_tests_subdir_include() {
	local tests_subdir="$frr_root/tests/subdir.am"
	local include_line='include tests/eigrpd/subdir.am'
	local eigrp_subdir="$frr_root/tests/eigrpd/subdir.am"

	[[ -f "$eigrp_subdir" ]] || return 0
	[[ -f "$tests_subdir" ]] || fail "FRR tests/subdir.am not found: $tests_subdir"

	if grep -qxF "$include_line" "$tests_subdir"; then
		return 0
	fi

	if [[ "$dry_run" -eq 1 ]]; then
		echo "would append to $tests_subdir: $include_line"
		return 0
	fi

	printf '\n%s\n' "$include_line" >> "$tests_subdir"
}

while [[ "$#" -gt 0 ]]; do
	case "$1" in
		-frr-root)
			[[ "$#" -ge 2 ]] || fail "-frr-root requires a path"
			frr_root="$(resolve_existing_path "$2")"
			shift 2
			;;
		-no-eigrpd)
			install_eigrpd=0
			shift
			;;
		-no-tests)
			install_tests=0
			shift
			;;
		-dry-run)
			dry_run=1
			shift
			;;
		-h|-help|--help)
			usage
			exit 0
			;;
		*)
			fail "unknown option: $1"
			;;
	esac
done

require_command rsync

if [[ -z "$frr_root" ]]; then
	if ! frr_root="$(infer_frr_root)"; then
		fail "FRR root could not be inferred; use -frr-root /path/to/frr"
	fi
fi

[[ -d "$frr_root" ]] || fail "FRR root does not exist: $frr_root"
[[ -f "$frr_root/bootstrap.sh" ]] || fail "not an FRR checkout root: $frr_root"
[[ -d "$eigrp_root/eigrpd" ]] || fail "missing project eigrpd directory: $eigrp_root/eigrpd"

if [[ "$install_eigrpd" -eq 1 ]]; then
	remove_destination_tree "$frr_root/eigrpd"
	copy_tree "$eigrp_root/eigrpd" "$frr_root/eigrpd"
	if [[ "$dry_run" -eq 1 ]]; then
		echo "would install: eigrpd/ -> $frr_root/eigrpd/"
	else
		echo "installed: eigrpd/ -> $frr_root/eigrpd/"
	fi
fi

if [[ "$install_tests" -eq 1 ]]; then
	if [[ -d "$eigrp_root/test/frr" ]]; then
		copy_tree "$eigrp_root/test/frr" "$frr_root/tests/eigrpd"
		install_tests_subdir_include
		if [[ "$dry_run" -eq 1 ]]; then
			echo "would install: test/frr/ -> $frr_root/tests/eigrpd/"
		else
			echo "installed: test/frr/ -> $frr_root/tests/eigrpd/"
		fi
	else
		echo "warning: no FRR test payload exists at $eigrp_root/test/frr" >&2
	fi
fi
