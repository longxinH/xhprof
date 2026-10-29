#!/usr/bin/env bash
#
# Run the extension tests in Docker for every supported PHP version, NTS and ZTS.
# Extra arguments are passed to tools/test.sh, for example --opcache.
#
#   tools/docker/test-all.sh
#   PHP_VERSIONS="8.4 8.5" PHP_VARIANTS=cli tools/docker/test-all.sh --opcache

set -uo pipefail

cd "$(dirname "$0")"

versions=(${PHP_VERSIONS:-8.2 8.3 8.4 8.5})
variants=(${PHP_VARIANTS:-cli zts})

results=()
failed=0

for version in "${versions[@]}"; do
	for variant in "${variants[@]}"; do
		echo "=== PHP $version $variant"

		if PHP_VERSION="$version" PHP_VARIANT="$variant" docker compose run --rm --build php tools/test.sh "$@"; then
			results+=("PASS  $version $variant")
		else
			results+=("FAIL  $version $variant")
			failed=1
		fi
	done
done

echo
printf '%s\n' "${results[@]}"

exit "$failed"
