FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Install core build dependencies and libraries
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libjsoncpp-dev \
    uuid-dev \
    zlib1g-dev \
    libssl-dev \
    libpq-dev \
    libhiredis-dev \
    libgtest-dev \
    libargon2-dev \
    && rm -rf /var/lib/apt/lists/*

# Build and install Drogon from source
WORKDIR /build-deps
RUN git clone https://github.com/drogonframework/drogon.git && \
    cd drogon && \
    git checkout tags/v1.9.8 -b v1.9.8-branch && \
    git submodule update --init && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF && \
    make -j$(nproc) && \
    make install

# Copy application source code
WORKDIR /app
COPY . .

# Build application
RUN rm -rf build && mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

EXPOSE 8080

CMD ["./build/banking_backend"]
