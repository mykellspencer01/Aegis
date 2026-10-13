# Reproducible build + test environment

# docker build -t aegis     # builds library and runs test suite
# docker run --rm aegis     # re-runs tests

FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY cpp_core/ cpp_core/
COPY Build/CMakeLists.txt Build/CMakeLists.txt

RUN cmake -S Build -B /out -DCMAKE_BUILD_TYPE=Release \
        -DNETENGINE_BUILD_TESTS=ON -DNETENGINE_ENABLE_LOGGING=OFF \
    && cmake --build /out --parallel \
    && ctest --test-dir /out --output-on-failure

FROM ubuntu:24.04 AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends libssl3 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /out/lib/libAegis_Core.so /usr/local/lib/
COPY --from=build /out/bin/NetEngineTests /usr/local/bin/
RUN ldconfig
CMD ["NetEngineTests"]