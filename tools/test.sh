#!/usr/bin/env bash
#
# Build the extension from a clean copy of its sources and run the .phpt tests.
# Runs inside the dev image (tools/docker), in CI, or anywhere phpize and php are on PATH.
#
#   tools/test.sh                         all tests
#   tools/test.sh tests/xhprof_007.phpt   selected tests (paths relative to extension/)
#   tools/test.sh --opcache               with opcache enabled for the CLI
#   tools/test.sh --valgrind              under valgrind (slow)
#
# Only sources and tests are copied, so phpize/configure/make output left in
# extension/ by a host build never leaks into the build, and the source tree
# is never written to.

set -euo pipefail

src="$(cd "$(dirname "$0")/../extension" && pwd)"
build="${BUILD_DIR:-/tmp/xhprof-build}"

opcache=0
valgrind=0
tests=()

for arg in "$@"; do
	case "$arg" in
		--opcache) opcache=1 ;;
		--valgrind) valgrind=1 ;;
		*) tests+=("$arg") ;;
	esac
done

rm -rf "$build"
mkdir -p "$build/tests"
cp "$src"/config.m4 "$src"/*.c "$src"/*.h "$build"/
for file in "$src"/tests/*; do
	case "$file" in
		*.out|*.diff|*.exp|*.log|*.sh|*.mem) continue ;;
	esac

	# run-tests.php writes each test's script to <name>.php next to <name>.phpt.
	if [[ "$file" == *.php && -f "${file%.php}.phpt" ]]; then
		continue
	fi

	cp "$file" "$build/tests/"
done
cd "$build"

php -v | head -n 1

phpize > /dev/null
./configure --enable-xhprof > /dev/null
# Extra CFLAGS from the environment are kept, e.g. an include path for pcre2.h.
make -s -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)" CFLAGS="${CFLAGS:-} -Wall -Werror=implicit-function-declaration -g -O2"

test_args=(--show-diff)

if (( opcache )); then
	# PHP 8.5+ always has opcache built in; older versions ship it as a shared extension.
	# make test points extension_dir at modules/, so give the full path.
	php -n -m | grep -q 'Zend OPcache' || test_args+=(-d "zend_extension=$(php -r 'echo ini_get("extension_dir");')/opcache.so")
	test_args+=(-d opcache.enable_cli=1)
fi

if (( valgrind )); then
	test_args+=(-m)
fi

export NO_INTERACTION=1 REPORT_EXIT_STATUS=1

make test TESTS="${test_args[*]} ${tests[*]:-tests/}"
