#!/usr/bin/env bash
set -euo pipefail

usage() {
	cat <<'USAGE'
Usage:
  ./backup.sh -z <zipfile name>
  ./backup.sh -zip <zipfile name>

Examples:
  ./backup.sh -z eigrpd.zip
  ./backup.sh -zip eigrpd-active-code.zip

Creates a clean copy of the eigrpd project at ~/Downloads/eigrpd,
removes .o files from that copy, then creates the requested zip file
in ~/Download unless an absolute path is provided.
USAGE
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd "$script_dir/.." && pwd)"
project_name="$(basename "$project_root")"

download_dir="$HOME/Downloads"
backup_dir="$download_dir/$project_name"
zip_name=""

while [[ $# -gt 0 ]]; do
	case "$1" in
		-z|-zip)
			shift
			if [[ $# -eq 0 || "${1:-}" == -* ]]; then
				echo "error: missing zip file name after -z/-zip" >&2
				usage >&2
				exit 2
			fi
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

if [[ -z "$zip_name" ]]; then
	echo "error: zip file name is required" >&2
	usage >&2
	exit 2
fi

if [[ "$zip_name" != *.zip ]]; then
	zip_name="${zip_name}.zip"
fi

if [[ "$zip_name" = /* ]]; then
	zip_path="$zip_name"
else
	zip_path="$download_dir/$zip_name"
fi

mkdir -p "$download_dir"

# Refuse to delete the active project if someone runs this from ~/Download/eigrpd/tools.
if [[ "$(cd "$project_root" && pwd)" == "$(mkdir -p "$backup_dir" && cd "$backup_dir" && pwd)" ]]; then
	echo "error: source project and backup target are the same path: $project_root" >&2
	exit 1
fi

rm -rf "$backup_dir"
cp -R "$project_root" "$backup_dir"

find "$backup_dir" -type f -name '*.o' -delete

rm -f "$zip_path"
(
	cd "$download_dir"
	zip -qr "$zip_path" "$project_name"
)

echo "created: $zip_path"
