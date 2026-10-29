# Contributing

The extension lives in `extension/` (C, built with `phpize`); the report UI and its library are plain PHP in `xhprof_html/` and `xhprof_lib/`.

## Pull requests

- Run the tests on the PHP versions you touched, ideally `make test-all` (and `make test-legacy` for changes that could affect PHP 7.x).
- Add a `.phpt` regression test in `extension/tests/` for every extension fix.
- Keep code compatible with PHP 7.2, the oldest version `composer.json` allows: no PHP 8-only syntax or functions such as `str_contains()`.
- GitHub Actions runs the same tests on Linux (PHP 7.2–8.5, NTS and ZTS, and with opcache) and Windows.

## Development tooling

Everything in `tools/` (and the root `Makefile`) is for working on xhprof, not for using it. Both are marked `export-ignore` in `.gitattributes`, so they stay out of `git archive`, GitHub release tarballs and Composer/PIE dist downloads. PECL packages only contain the files listed in `package.xml`.

### Quick start

The root `Makefile` wraps the scripts and Docker commands below. Each target is a one-liner, so it's easy to see what it runs.

| Target | What it does |
|---|---|
| `make test` | Build and run the tests in the dev image |
| `make test-opcache` | The same, with opcache enabled for the CLI |
| `make test-valgrind` | The same, under valgrind (slow) |
| `make test-all` | PHP 8.2–8.5, NTS and ZTS |
| `make test-legacy` | PHP 7.2–8.1 NTS, which `composer.json` still allows |
| `make test-host` | Build and test with the PHP on this machine, without Docker |
| `make shell` | Bash in the dev image, with the repository at `/src` |
| `make ui` | Report UI on <http://localhost:8080> |
| `make image` | Build the dev image only |

Variables: `PHP` (default `8.5`), `VARIANT` (`cli` or `zts`), `DEBUG_TOOLS` (`0` for PHP 7.2–8.0) and `TESTS` (test files, relative to `extension/`):

```bash
make test PHP=8.2 VARIANT=zts TESTS=tests/xhprof_007.phpt
```

With Homebrew PHP, `make test-host` may need the pcre2 headers: `CFLAGS="-I$(brew --prefix pcre2)/include" make test-host`.

### Running the tests

`tools/test.sh` builds the extension and runs the `.phpt` tests. It copies the sources and tests to a scratch directory (`/tmp/xhprof-build`, or `$BUILD_DIR`) first. That keeps build output out of `extension/` and stops stale `phpize`/`configure` files in `extension/` from leaking into the build.

```bash
tools/test.sh                          # all tests
tools/test.sh tests/xhprof_007.phpt    # selected tests (paths relative to extension/)
tools/test.sh --opcache                # with opcache enabled for the CLI
tools/test.sh --valgrind               # under valgrind (slow)
```

It works anywhere `phpize` and `php` are on `PATH`. To get a predictable environment, run it in Docker instead.

### Docker

`tools/docker/Dockerfile` is a single image for every PHP version. It is based on the official `php` image, and two build args choose the PHP build:

| Arg | Default | Values |
|---|---|---|
| `PHP_VERSION` | `8.5` | any `php` image version, e.g. `8.2`, `8.3`, `8.4`, `8.5` |
| `PHP_VARIANT` | `cli` | `cli` (NTS) or `zts` |
| `DEBUG_TOOLS` | `1` | `0` skips gdb, valgrind and graphviz. Use it for PHP 7.2–8.0, whose Debian bases no longer install cleanly from apt. The tests don't need them. |

The image only contains the toolchain (`phpize` deps, gdb, valgrind, graphviz). The repository is mounted at `/src` at run time, so you only rebuild it when you change PHP versions. Each version and variant gets its own tag, `xhprof-dev:<version>-<variant>`.

Compose reads the same values from the environment:

```bash
docker compose -f tools/docker/compose.yaml run --rm php
```

```bash
PHP_VERSION=8.2 PHP_VARIANT=zts docker compose -f tools/docker/compose.yaml run --rm php
```

```bash
docker compose -f tools/docker/compose.yaml run --rm php tools/test.sh --opcache
```

```bash
docker compose -f tools/docker/compose.yaml run --rm php bash
```

To skip `-f` on every call, set `export COMPOSE_FILE=tools/docker/compose.yaml`.

#### Whole matrix

`tools/docker/test-all.sh` runs the tests for every supported version, NTS and ZTS, and prints a summary at the end. Any arguments are passed on to `tools/test.sh`.

```bash
tools/docker/test-all.sh
```

```bash
PHP_VERSIONS="8.4 8.5" PHP_VARIANTS=cli tools/docker/test-all.sh --opcache
```

Older versions that `composer.json` still claims (`>=7.2.0`):

```bash
PHP_VERSIONS="7.2 7.3 7.4 8.0 8.1" PHP_VARIANTS=cli DEBUG_TOOLS=0 tools/docker/test-all.sh
```

#### Report UI

```bash
docker compose -f tools/docker/compose.yaml up ui
```

This serves `xhprof_html` on <http://localhost:8080>. Runs are read from `XHPROF_OUTPUT_DIR` (`/tmp/xhprof`, a named volume that the `php` service shares).

#### Debugging a crash

```bash
docker compose -f tools/docker/compose.yaml run --rm php bash
```

Inside the container:

```bash
tools/test.sh tests/xhprof_007.phpt
cd /tmp/xhprof-build && gdb --args php -n -d extension=$PWD/modules/xhprof.so tests/xhprof_007.php
```

`tools/test.sh --valgrind` runs the tests under valgrind, with `USE_ZEND_ALLOC=0` set by `run-tests.php`.
