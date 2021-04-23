################
FROM ubuntu:20.04 AS painless_base
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt install -y openssh-server iproute2 openmpi-bin openmpi-common iputils-ping \
    && mkdir /var/run/sshd \
    && sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd \
    && setcap CAP_NET_BIND_SERVICE=+eip /usr/sbin/sshd \
    && useradd -ms /bin/bash painless \
    && chown -R painless /etc/ssh/ \
    && su - painless -c \
        'ssh-keygen -q -t rsa -f ~/.ssh/id_rsa -N "" \
        && cp ~/.ssh/id_rsa.pub ~/.ssh/authorized_keys \
        && cp /etc/ssh/sshd_config ~/.ssh/sshd_config \
        && sed -i "s/UsePrivilegeSeparation yes/UsePrivilegeSeparation no/g" ~/.ssh/sshd_config \
        && printf "Host *\n\tStrictHostKeyChecking no\n" >> ~/.ssh/config'
WORKDIR /home/painless
ENV NOTVISIBLE "in users profile"
RUN echo "export VISIBLE=now" >> /etc/profile
EXPOSE 22
################
FROM ubuntu:20.04 AS builder
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt install -y cmake build-essential zlib1g-dev libopenmpi-dev git wget unzip build-essential zlib1g-dev iproute2 cmake python3 python3-pip build-essential gfortran wget curl

ADD painless painless
RUN cd painless && make -j 4
################
FROM painless_base AS painless_liaison
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt install -y awscli python3 mpi
COPY --from=builder /painless /painless
ADD make_combined_hostfile.py supervised-scripts/make_combined_hostfile.py
RUN chmod 755 supervised-scripts/make_combined_hostfile.py
ADD mpi-run.sh supervised-scripts/mpi-run.sh
USER painless
CMD supervised-scripts/mpi-run.sh


