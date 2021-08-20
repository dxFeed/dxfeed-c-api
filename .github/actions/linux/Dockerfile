FROM bitnami/minideb:stretch

ENV DEBIAN_FRONTEND noninteractive

RUN                                                                                                               \
    apt-get update  -y                                                                                         && \
    apt-get upgrade -y                                                                                         && \
    apt-get install -y --no-install-recommends apt-utils                                                       && \
    apt-get install -y --no-install-recommends p7zip-full                                                      && \
    apt-get install -y --no-install-recommends cmake                                                           && \
    apt-get install -y --no-install-recommends make gcc g++                                                    && \
    apt-get install -y --no-install-recommends mc gnupg                                                        && \
    apt-get install -y --no-install-recommends automake                                                        && \
    apt-get install -y --no-install-recommends autoconf                                                        && \
    apt-get install -y --no-install-recommends libtool                                                         && \
    apt-get install -y --no-install-recommends perl                                                            && \
    apt-get install -y --no-install-recommends curl zip git                                                    && \
    echo "DONE!!!"

COPY entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
