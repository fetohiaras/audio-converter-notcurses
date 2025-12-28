# =============================================================================
# Builder stage: compile all targets with the required multimedia and TUI deps
# =============================================================================
FROM debian:bookworm-slim AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git \
    libavcodec-dev \
    libavformat-dev \
    libswresample-dev \
    libavutil-dev \
    libopus-dev \
    libmp3lame-dev \
    libnotcurses-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work

COPY . /work

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build --config Release

# =============================================================================
# Runtime stage: ship only what we need to run the built binaries
# =============================================================================
FROM debian:bookworm-slim AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libavcodec60 \
    libavformat60 \
    libswresample4 \
    libavutil58 \
    libopus0 \
    libmp3lame0 \
    libnotcurses6 \
    libnotcurses++2 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /work/build/audio_converter_tui /app/audio_converter_tui
COPY --from=builder /work/build/nc_hello /app/nc_hello

# Default entrypoint runs the TUI; override with `nc_hello` for quick demo.
ENTRYPOINT ["/app/audio_converter_tui"]
