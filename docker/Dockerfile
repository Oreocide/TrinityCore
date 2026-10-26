# Runtime-only image for a TrinityCore build already compiled on the host
# (see /home/brandon/trinity-server). This does NOT compile TrinityCore -
# it just provides an environment with the exact shared libraries the
# prebuilt binaries were linked against.
FROM ubuntu:26.04

ARG USER_ID=1000
ARG GROUP_ID=1000

RUN apt-get update && apt-get install -y --no-install-recommends \
        libmysqlclient24 \
        libboost-filesystem1.90.0 \
        libboost-program-options1.90.0 \
        libboost-regex1.90.0 \
        libboost-locale1.90.0 \
        libboost-charconv1.90.0 \
        libboost-thread1.90.0 \
        libboost-atomic1.90.0 \
        libboost-container1.90.0 \
        libboost-chrono1.90.0 \
        libboost-date-time1.90.0 \
        libssl3 \
        libreadline8 \
        libicu78 \
        ca-certificates \
        mysql-client \
        tzdata \
    && rm -rf /var/lib/apt/lists/*

# ubuntu:26.04 already ships a uid/gid 1000 "ubuntu" user; reuse it under
# the trinity name instead of colliding with a new one.
RUN if getent passwd ${USER_ID} >/dev/null; then \
        existing=$(getent passwd ${USER_ID} | cut -d: -f1); \
        usermod -l trinity -d /home/trinity -m "$existing"; \
        groupmod -n trinity "$existing" 2>/dev/null || true; \
    else \
        getent group ${GROUP_ID} >/dev/null || groupadd -g ${GROUP_ID} trinity; \
        useradd -m -u ${USER_ID} -g ${GROUP_ID} trinity; \
    fi

# Binaries/libs are bind-mounted from the host build output (trinity-server/)
# at compose runtime rather than baked into the image, since the user wants
# volumes bound to the source checkout.
ENV LD_LIBRARY_PATH=/trinity/lib

USER trinity
WORKDIR /trinity
