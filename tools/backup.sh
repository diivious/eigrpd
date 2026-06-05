#!/usr/bin/env bash
# SPDX-License-Identifier: ISC
#
# Copyright (C) 2026 Donnie V. Savage
#
# Create a clean project zip without local VCS/build/cache noise.

set -euo pipefail

script_name="$(basename "$0")"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd "$script_dir/.." && pwd)"
project_name="$(basename "$project_root")"

download_dir="$HOME/Downloads"
zip_name=""

usage() {
	cat <<USAGE
usage: $script_name -z ZIPFILE

examples:
  tools/backup.sh -z eigrp.zip
  tools/backup.sh -zip eigrp-active-code.zip

Creates a clean copy of the project at ~/Downloads/$project_name and writes the
requested zip file to ~/Downloads unless ZIPFILE is an absolute path.
USAGE
}

while [[ "$#" -gt 0 ]]; do
	case "$1" in
		-z|-zip)
			shift
			[[ "$#" -gt 0 && "${1:-}" != -* ]] || { echo "error: missing zip file name" >&2; usage >&2; exit 2; }
			zip_name="$1"
			shift
			;;
		-h|-help|--help)
			usage
			exit 0
			;;
		*)
			echo "error: unknown option: $1" >&2
			usage >&2
			exit 2
			;;
	esac
done

[[ -n "$zip_name" ]] || { echo "error: zip file name is required" >&2; usage >&2; exit 2; }
[[ "$zip_name" == *.zip ]] || zip_name="$zip_name.zip"

if [[ "$zip_name" = /* ]]; then
	zip_path="$zip_name"
else
	zip_path="$download_dir/$zip_name"
fi

backup_dir="$download_dir/$project_name"
mkdir -p "$download_dir"

project_root_abs="$(cd "$project_root" && pwd)"
backup_dir_abs="$(mkdir -p "$backup_dir" && cd "$backup_dir" && pwd)"

if [[ "$project_root_abs" == "$backup_dir_abs" ]]; then
	echo "error: source project and backup target are the same path: $project_root" >&2
	exit 1
fi

zip_dir="$(dirname "$zip_path")"
mkdir -p "$zip_dir"
zip_path="$(cd "$zip_dir" && pwd)/$(basename "$zip_path")"

case "$zip_path" in
	"$backup_dir_abs"/*)
		echo "error: zip file must not be written inside temporary backup directory: $backup_dir_abs" >&2
		exit 1
		;;
esac

cleanup_backup_dir() {
	if [[ -n "${backup_dir_abs:-}" && "$backup_dir_abs" != "/" && "$backup_dir_abs" != "$project_root_abs" && -d "$backup_dir_abs" ]]; then
		rm -rf "$backup_dir_abs"
	fi
}
trap cleanup_backup_dir EXIT

rm -rf "$backup_dir_abs"
cp -R "$project_root_abs" "$backup_dir_abs"

find "$backup_dir" \
	-name .git -o \
	-name .pytest_cache -o \
	-name __pycache__ -o \
	-name __MACOSX -o \
	-name .DS_Store -o \
	-name '*.o' -o \
	-name '*.lo' -o \
	-name '*.la' -o \
	-name '*~' | while IFS= read -r path; do
		rm -rf "$path"
	done

rm -rf "$backup_dir/build/obj" "$backup_dir/build/logs"
rm -f "$zip_path"
(
	cd "$download_dir"
	zip -qr "$zip_path" "$project_name"
)

echo "created: $zip_path"
