FROM centos:7

RUN                                                                                                                                                \
     yum -y update && yum install -y wget tar which gcc binutils binutils-devel gcc-c++ make                                                    && \
     wget https://github.com/Kitware/CMake/releases/download/v3.19.1/cmake-3.19.1-Linux-x86_64.sh                                               && \
     bash cmake-3.19.1-Linux-x86_64.sh --prefix=/usr --skip-license                                                                             && \
     rm -f cmake-3.19.1-Linux-x86_64.sh                                                                                                         && \
     yum -y install https://www.mirrorservice.org/sites/dl.fedoraproject.org/pub/epel/7/x86_64/Packages/p/p7zip-16.02-10.el7.x86_64.rpm         && \
     yum -y install https://www.mirrorservice.org/sites/dl.fedoraproject.org/pub/epel/7/x86_64/Packages/p/p7zip-plugins-16.02-10.el7.x86_64.rpm && \
     yum -y install https://packages.endpoint.com/rhel/7/os/x86_64/endpoint-repo-1.7-1.x86_64.rpm                                               && \
     yum install -y git                                                                                                                         && \
     echo "DONE!!!"

COPY entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
