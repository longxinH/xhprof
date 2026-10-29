# Development and test image for the xhprof extension.
#
#   docker build --build-arg PHP_VERSION=8.3 --build-arg PHP_VARIANT=zts -t xhprof-dev:8.3-zts tools/docker
#
# PHP_VARIANT is "cli" (NTS) or "zts". The source is mounted at /src at run time,
# so the image only holds the toolchain and is reused across code changes.
ARG PHP_VERSION=8.5
ARG PHP_VARIANT=cli

FROM php:${PHP_VERSION}-${PHP_VARIANT}

# The official images keep $PHPIZE_DEPS (autoconf, gcc, make, ...) installed,
# which is all the tests need. gdb and valgrind are for debugging crashes,
# graphviz for the UI call graph. Set DEBUG_TOOLS=0 for PHP 7.2–8.0, whose
# Debian bases (buster, bullseye) no longer install cleanly from apt.
ARG DEBUG_TOOLS=1

RUN if [ "$DEBUG_TOOLS" = "1" ]; then \
        apt-get update \
        && apt-get install -y --no-install-recommends gdb valgrind graphviz \
        && rm -rf /var/lib/apt/lists/*; \
    fi

ENV NO_INTERACTION=1 \
    REPORT_EXIT_STATUS=1 \
    XHPROF_OUTPUT_DIR=/tmp/xhprof

WORKDIR /src

CMD ["tools/test.sh"]
