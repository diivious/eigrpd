#!/usr/bin/env bash
# SPDX-License-Identifier: ISC
#
# Copyright (C) 2026 Donnie V. Savage
#
# EIGRP project unit/topotest runner.
#
# This script is intended to be run from an EIGRP checkout or from
# frr/eigrpd after the EIGRP repository has been copied into FRR.

set -euo pipefail

script_name="$(basename "$0")"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
eigrpd_root="$(cd "$script_dir/.." && pwd)"

run_portable=0
run_frr=0
install_tests=1
install_only=0
list_only=0
sudo_mode="auto"
frr_root=""
pytest_args=()
topotest_cases=()
missing_groups=()

usage() {
	cat <<USAGE
usage: $script_name [options] [-- pytest-args]

primary options:
  -all                 Run all currently implemented EIGRP tests.
  -packet              Run portable packet tests.
  -portable            Run all portable tests.
  -cli                 Run FRR topotest: eigrp_cli_basic.
  -neighbors           Run FRR topotest: eigrp_neighbor_basic.
  -topo                Run implemented FRR topotests matching eigrp_topo_*.
  -redist              Run implemented FRR topotests matching eigrp_redist_*.
  -install             Copy EIGRP FRR testcases into the FRR tests tree and exit.
  -list                List available EIGRP testcases and exit.

path/options:
  -frr-root PATH       FRR checkout root. Default: parent of eigrpd when
                       this script is run from frr/eigrpd/tools.
  -no-install          Do not copy testcases before running FRR topotests.
  -sudo auto|yes|no    Topotest sudo behavior. Default: auto.
  -h, -help, --help    Show this help.

examples:
  cd /path/to/frr/eigrpd/tools
  ./unittest.sh -all
  ./unittest.sh -cli
  ./unittest.sh -neighbors -- -k neighbor
  ./unittest.sh -frr-root /path/to/frr -cli

notes:
  FRR topotests are run from /path/to/frr/tests/topotests as required by FRR.
  Portable tests are run from eigrpd/testcases.
USAGE
}

fail() {
	echo "error: $*" >&2
	exit 1
}

warn() {
	echo "warning: $*" >&2
}

is_abs_path() {
	case "$1" in
		/*) return 0 ;;
		*) return 1 ;;
	esac
}

resolve_path() {
	local path="$1"
	if is_abs_path "$path"; then
		printf '%s\n' "$path"
	else
		cd "$PWD" && cd "$path" && pwd
	fi
}

infer_frr_root() {
	local candidate
	candidate="$(cd "$eigrpd_root/.." && pwd)"
	if [[ -d "$candidate/tests/topotests" ]]; then
		printf '%s\n' "$candidate"
		return 0
	fi
	return 1
}

require_command() {
	command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

add_topotest_case() {
	local case_name="$1"
	topotest_cases+=("$case_name")
	run_frr=1
}

add_topotest_group() {
	local group_name="$1"
	local pattern="$2"
	local src_dir="$eigrpd_root/testcases/frr/tests/topotests"
	local found=0
	local path

	shopt -s nullglob
	for path in "$src_dir"/$pattern; do
		[[ -d "$path" ]] || continue
		topotest_cases+=("$(basename "$path")")
		found=1
	done
	shopt -u nullglob

	if [[ "$found" -eq 0 ]]; then
		missing_groups+=("$group_name ($pattern)")
	fi

	run_frr=1
}

make_unique_cases() {
	local seen=" "
	local case_name
	local unique=()

	for case_name in "${topotest_cases[@]}"; do
		if [[ "$seen" != *" $case_name "* ]]; then
			unique+=("$case_name")
			seen+="$case_name "
		fi
	done

	topotest_cases=("${unique[@]}")
}

print_available_tests() {
	echo "portable tests:"
	if [[ -d "$eigrpd_root/testcases/portable" ]]; then
		find "$eigrpd_root/testcases/portable" -mindepth 1 -maxdepth 3 -type d | sed "s#^$eigrpd_root/testcases/##" | sort
	else
		echo "  none"
	fi

	echo
	echo "frr topotests:"
	if [[ -d "$eigrpd_root/testcases/frr/tests/topotests" ]]; then
		find "$eigrpd_root/testcases/frr/tests/topotests" -mindepth 1 -maxdepth 1 -type d -name 'eigrp_*' | while IFS= read -r path; do echo "  $(basename "$path")"; done | sort
	else
		echo "  none"
	fi
}

install_frr_tests() {
	local root="$1"
	local src_tests="$eigrpd_root/testcases/frr/tests"
	local src_topotests="$src_tests/topotests"
	local src_eigrpd="$src_tests/eigrpd"
	local dst_topotests="$root/tests/topotests"
	local dst_eigrpd="$root/tests/eigrpd"
	local path
	local base

	[[ -d "$src_tests" ]] || fail "missing testcase source: $src_tests"
	[[ -d "$root/tests" ]] || fail "FRR tests directory not found: $root/tests"
	[[ -d "$dst_topotests" ]] || fail "FRR topotests directory not found: $dst_topotests"

	if [[ -d "$src_eigrpd" ]]; then
		mkdir -p "$dst_eigrpd"
		if compgen -G "$src_eigrpd/*" >/dev/null; then
			cp -R "$src_eigrpd"/. "$dst_eigrpd"/
		fi
	fi

	shopt -s nullglob
	for path in "$src_topotests"/eigrp_*; do
		[[ -d "$path" ]] || continue
		base="$(basename "$path")"
		rm -rf "$dst_topotests/$base"
		cp -R "$path" "$dst_topotests/$base"
	done
	shopt -u nullglob

	echo "installed EIGRP testcases into: $root/tests"
}

run_portable_tests() {
	require_command python3
	(
		cd "$eigrpd_root/testcases"
		python3 -m pytest portable "${pytest_args[@]}"
	)
}

run_frr_topotests() {
	local root="$1"
	local topotests_dir="$root/tests/topotests"
	local pytest_cmd=()

	[[ -d "$topotests_dir" ]] || fail "FRR topotests directory not found: $topotests_dir"
	make_unique_cases

	if [[ "${#topotest_cases[@]}" -eq 0 ]]; then
		fail "no FRR topotests selected"
	fi

	if [[ "${#missing_groups[@]}" -gt 0 ]]; then
		local group
		for group in "${missing_groups[@]}"; do
			warn "no testcase exists yet for $group"
		done
	fi

	case "$sudo_mode" in
		auto)
			if [[ "$(id -u)" -eq 0 ]]; then
				pytest_cmd=(python3 -m pytest)
			else
				require_command sudo
				pytest_cmd=(sudo -E python3 -m pytest)
			fi
			;;
		yes)
			require_command sudo
			pytest_cmd=(sudo -E python3 -m pytest)
			;;
		no)
			pytest_cmd=(python3 -m pytest)
			;;
		*)
			fail "invalid sudo mode: $sudo_mode"
			;;
	esac

	(
		cd "$topotests_dir"
		"${pytest_cmd[@]}" -s -v "${topotest_cases[@]}" "${pytest_args[@]}"
	)
}

while [[ "$#" -gt 0 ]]; do
	case "$1" in
		-all)
			run_portable=1
			add_topotest_group "all frr topotests" "eigrp_*"
			shift
			;;
		-packet|-portable)
			run_portable=1
			shift
			;;
		-cli)
			add_topotest_case "eigrp_cli_basic"
			shift
			;;
		-neighbors|-neighbor)
			add_topotest_case "eigrp_neighbor_basic"
			shift
			;;
		-topo)
			add_topotest_group "topology tests" "eigrp_topo_*"
			shift
			;;
		-redist|-redistribution)
			add_topotest_group "redistribution tests" "eigrp_redist_*"
			shift
			;;
		-install)
			install_only=1
			shift
			;;
		-list)
			list_only=1
			shift
			;;
		-frr-root)
			[[ "$#" -ge 2 ]] || fail "-frr-root requires a path"
			frr_root="$(resolve_path "$2")"
			shift 2
			;;
		-no-install)
			install_tests=0
			shift
			;;
		-sudo)
			[[ "$#" -ge 2 ]] || fail "-sudo requires auto, yes, or no"
			sudo_mode="$2"
			shift 2
			;;
		-h|-help|--help)
			usage
			exit 0
			;;
		--)
			shift
			pytest_args+=("$@")
			break
			;;
		*)
			fail "unknown option: $1"
			;;
	esac
done

if [[ "$list_only" -eq 1 ]]; then
	print_available_tests
	exit 0
fi

if [[ "$run_portable" -eq 0 && "$run_frr" -eq 0 && "$install_only" -eq 0 ]]; then
	usage
	exit 2
fi

if [[ "$run_frr" -eq 1 || "$install_only" -eq 1 ]]; then
	if [[ -z "$frr_root" ]]; then
		if ! frr_root="$(infer_frr_root)"; then
			fail "FRR root could not be inferred; use -frr-root /path/to/frr"
		fi
	fi

	[[ -d "$frr_root" ]] || fail "FRR root does not exist: $frr_root"

	if [[ "$install_tests" -eq 1 || "$install_only" -eq 1 ]]; then
		install_frr_tests "$frr_root"
	fi
fi

if [[ "$install_only" -eq 1 ]]; then
	exit 0
fi

if [[ "$run_portable" -eq 1 ]]; then
	run_portable_tests
fi

if [[ "$run_frr" -eq 1 ]]; then
	run_frr_topotests "$frr_root"
fi
