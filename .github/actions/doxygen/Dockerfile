FROM bitnami/minideb:stretch

ENV DEBIAN_FRONTEND noninteractive

RUN                                                                                                               \
    apt-get update  -y                                                                                         && \
    apt-get upgrade -y                                                                                         && \
    apt-get install -y --no-install-recommends apt-utils                                                       && \
    apt-get install -y --no-install-recommends perl zip                                                        && \
    apt-get install -y --no-install-recommends doxygen                                                         && \
    apt-get install -y --no-install-recommends graphviz                                                        && \
    echo "DONE!!!"

COPY entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
