# Development shortcuts. Each target wraps tools/; see CONTRIBUTING.md.
#
#   make test                          build and test on PHP 8.5 NTS in Docker
#   make test PHP=8.2 VARIANT=zts      another PHP build
#   make test TESTS=tests/xhprof_007.phpt
#
# This only drives the Docker tooling. Building the extension itself is
# unchanged: phpize, configure and make inside extension/.

PHP ?= 8.5
VARIANT ?= cli
# 0 skips gdb, valgrind and graphviz; needed for PHP 7.2-8.0 images.
DEBUG_TOOLS ?= 1
TESTS ?=

COMPOSE = PHP_VERSION=$(PHP) PHP_VARIANT=$(VARIANT) DEBUG_TOOLS=$(DEBUG_TOOLS) \
	docker compose -f tools/docker/compose.yaml
RUN = $(COMPOSE) run --rm --build php

.PHONY: test test-opcache test-valgrind test-all test-legacy test-host shell ui image

# Build and run the tests in the dev image.
test:
	$(RUN) tools/test.sh $(TESTS)

# Same, with opcache enabled for the CLI.
test-opcache:
	$(RUN) tools/test.sh --opcache $(TESTS)

# Same, under valgrind (slow).
test-valgrind:
	$(RUN) tools/test.sh --valgrind $(TESTS)

# PHP 8.2-8.5, NTS and ZTS.
test-all:
	tools/docker/test-all.sh $(TESTS)

# PHP 7.2-8.1 NTS, which composer.json still allows.
test-legacy:
	PHP_VERSIONS="7.2 7.3 7.4 8.0 8.1" PHP_VARIANTS=cli DEBUG_TOOLS=0 tools/docker/test-all.sh $(TESTS)

# Build and test with the PHP on this machine, without Docker.
test-host:
	tools/test.sh $(TESTS)

# Bash in the dev image, with the repository at /src.
shell:
	$(COMPOSE) run --rm --build php bash

# Report UI on http://localhost:8080.
ui:
	$(COMPOSE) up --build ui

# Build the dev image only.
image:
	$(COMPOSE) build php
