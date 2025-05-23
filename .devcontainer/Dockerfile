FROM ubuntu:22.04

ENV TOOLCHAIN="arm-zephyr-eabi"
ENV ZSDK_VERSION="0.17.0"
ARG WGET_ARGS="-q --show-progress --progress=bar:force:noscroll"
ARG ZEPHYR_SDK_DIR="/opt"
ARG UID=1000
ARG GID=1000
ARG USERNAME="user"
ENV ZEPHYR_SDK=${ZEPHYR_SDK_DIR}/zephyr-sdk-${ZSDK_VERSION}
ENV DEBIAN_FRONTEND=noninteractive

SHELL ["/bin/bash", "-c"]

RUN dpkg --add-architecture i386 && \
	apt-get -y update && \
	apt-get -y upgrade && \
	apt-get --no-install-recommends -y install \
		autoconf \
		automake \
		bison \
		build-essential \
		ca-certificates \
		ccache \
		chrpath \
		cmake \
		cpio \
		device-tree-compiler \
		dfu-util \
		diffstat \
		dos2unix \
		doxygen \
		file \
		flex \
		g++ \
		g++-multilib \
		gawk \
		gcc \
		gcc-multilib \
		gcovr \
		gdb \
		git \
		gnupg \
		gperf \
		gtk-sharp2 \
		help2man \
		iproute2 \
		lcov \
		libc6-dbg:i386 \
		libcairo2-dev \
		libfontconfig1 \
		libfreetype6 \
		libfuse-dev:i386 \
		libglib2.0-0 \
		libglib2.0-dev \
		libgtk2.0-0 \
		libice6 \
		liblocale-gettext-perl \
		libncurses5-dev \
		libpcap-dev \
		libpopt0 \
		libsdl1.2-dev \
		libsdl2-dev \
		libsdl2-dev:i386 \
		libsm6 \
		libssl-dev \
		libtool \
		libtool-bin \
		libx11-xcb1 \
		libxcb-icccm4 \
		libxcb-image0 \
		libxcb-keysyms1 \
		libxcb-randr0 \
		libxcb-render-util0 \
		libxcb-shape0 \
		libxcb-sync1 \
		libxcb-xfixes0 \
		libxkbcommon-x11-0 \
		libxrender1 \
		locales \
		lsb-release \
		make \
		net-tools \
		ninja-build \
		openssh-client \
		ovmf \
		parallel \
		picocom \
		pkg-config \
		python-is-python3 \
		python3-dev \
		python3-pip \
		python3-ply \
		python3-setuptools \
		qemu \
		rsync \
		socat \
		software-properties-common \
		srecord \
		texinfo \
		thrift-compiler \
		udev git wget \
		unzip \
		valgrind \
		wget \
		xz-utils \
	&& \
	apt-get clean -y && \
	apt-get autoremove --purge -y && \
	rm -rf /var/lib/apt/lists/*

# Initialise system locale
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8
RUN locale-gen $LANG

# Install Python dependencies
RUN python3 -m pip install -U --no-cache-dir pip && \
	pip3 install -U --no-cache-dir wheel setuptools && \
	pip3 install --no-cache-dir pygobject && \
	pip3 install --no-cache-dir \
		-r https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/scripts/requirements.txt \
		-r https://raw.githubusercontent.com/zephyrproject-rtos/mcuboot/main/scripts/requirements.txt \
		GitPython imgtool junitparser junit2html numpy protobuf PyGithub \
		pylint sh statistics west \
		nrf-regtool~=7.0.0 && \
	pip3 check


# Hack udev to run inside a container
RUN sed -i.bak -e '/if \[ ! -w \/sys \]/,+3 s/^/#/' /etc/init.d/udev

# Install Jlink
RUN wget ${WGET_ARGS} --post-data "accept_license_agreement=accepted" https://www.segger.com/downloads/jlink/JLink_Linux_x86_64.deb && \
	dpkg -i ./JLink_Linux_x86_64.deb && \
	rm ./JLink_Linux_x86_64.deb

# install minimal Zephyr SDK
WORKDIR "${ZEPHYR_SDK_DIR}"
RUN wget ${WGET_ARGS} -qO- https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZSDK_VERSION}/zephyr-sdk-${ZSDK_VERSION}_linux-x86_64_minimal.tar.xz | tar xJ

# install toolchain and host tools
WORKDIR "${ZEPHYR_SDK}"
RUN ./setup.sh -t all -h

# install OpenOCD udev rules
ADD https://raw.githubusercontent.com/espressif/openocd-esp32/refs/heads/master/contrib/60-openocd.rules /lib/udev/rules.d/60-openocd.rules

# Create 'user' account
RUN groupadd -g 18 -o dialout2
RUN groupadd -g $GID -o ${USERNAME}
RUN useradd -u $UID -m -g ${USERNAME} -G plugdev ${USERNAME}
RUN usermod -a -G dialout2 ${USERNAME}

USER ${USERNAME}
WORKDIR /home/${USERNAME}/workspace

RUN	${ZEPHYR_SDK}/setup.sh -c && \
	chown -R ${USERNAME}:${USERNAME} /home/${USERNAME}/.cmake

RUN west init && \
	west update && \
    python3 -m pip install -r zephyr/scripts/requirements.txt

# RUN git clone -n --depth=1 --filter=tree:0 https://github.com/cmsis-svd/cmsis-svd-data.git && \
# 	cd cmsis-svd-data && \
# 	git sparse-checkout set --no-cone data/STMicro && \
# 	git checkout
# ENV SVD_DIR=/home/${USERNAME}/workspace/cmsis-svd-data/data/STMicro


ENV XDG_CACHE_HOME=/home/${USERNAME}/.cache
ENV ZEPHYR_BASE=/home/${USERNAME}/workspace/zephyr
ENV PATH="${ZEPHYR_BASE}/scripts:${PATH}"
# add openocd to PATH
ENV PATH="${ZEPHYR_SDK}/sysroots/x86_64-pokysdk-linux/usr/bin:${PATH}"
# add toolchain to PATH
ENV PATH="${ZEPHYR_SDK}/${TOOLCHAIN}/bin:${PATH}"
