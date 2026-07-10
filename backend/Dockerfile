# --- Build Stage ---
FROM ubuntu:24.04 AS builder

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

# Build and install Drogon from source (using -j2 to avoid OOM killer on low-RAM hosts)
WORKDIR /build-deps
RUN git clone https://github.com/drogonframework/drogon.git && \
    cd drogon && \
    git checkout tags/v1.9.8 -b v1.9.8-branch && \
    git submodule update --init && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF && \
    make -j2 && \
    make install

# Copy application source code
WORKDIR /app
COPY . .

# Build application (using -j2 to avoid OOM killer on low-RAM hosts)
RUN rm -rf build && mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j2

# --- Runtime Stage ---
FROM ubuntu:24.04 AS runner

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libjsoncpp-dev \
    libssl-dev \
    libpq-dev \
    libhiredis-dev \
    libargon2-dev \
    uuid-dev \
    zlib1g-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy application binaries and configurations
COPY --from=builder /app/build/banking_backend ./banking_backend
COPY --from=builder /app/config.json ./config.json

EXPOSE 8080

CMD ["./banking_backend"]
