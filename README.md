# Aegis


A cross-platform, engine-agnostic multiplayer networking library. A C++17 core handles transport, replication and encryption; a C# managed layer exposes it to Unity and .NET through a C ABI.

## Architecture

| Layer | Location | What it does |
|---|---|---|
| Transport | `cpp_core/Transport` | UDP socket, per-peer ack tracking, unreliable / reliable-ordered / genre-defined channels, retransmits |
| Serialization | `cpp_core/Serializer` | Bit-level reader/writer |
| Replication | `cpp_core/Replication` | Delta snapshots with priority-based bandwidth control, RPC dispatch |
| Security | `cpp_core/Packets` | AES-128-CTR + truncated HMAC-SHA256 via OpenSSL (toggle with `NETENGINE_USE_OPENSSL`) |
| Genre modules | `cpp_core/Genres` | FPS (lag compensation / hit-scan), RTS (lockstep turns, desync detection), MMO (area-of-interest cells, zone transfer) |
| C ABI | `cpp_core/EngineExports.cpp` | `Aegis_*` exported functions |
| Managed layer | `cs_core` | `netstandard2.1` P/Invoke bindings, `SessionManager`, security and matchmaking APIs, loopback examples |
| Validation | `UnityValidation` | Unity project that drives the library through `AegisNetworkBridge` |

## Build and test

Requirements: CMake >= 3.20, a C++17 compiler, OpenSSL development headers.

```bash
cmake -S Build -B build -DNETENGINE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

CMake options: `NETENGINE_BUILD_TESTS` (default OFF, fetches GoogleTest), `NETENGINE_BUILD_BENCHMARKS` (OFF), `NETENGINE_USE_OPENSSL` (ON), `NETENGINE_ENABLE_LOGGING` (ON).

C# tests (NUnit, .NET 10) need the native library built first at `Build/build`:

```bash
cmake -S Build -B Build/build && cmake --build Build/build
dotnet test cs_core/SharpTests/Aegis.Managed.SharpTests
```

### Docker

```bash
docker build -t aegis .   # builds the core and runs the C++ test suite during the build
docker run --rm aegis     # re-runs the tests
```

## Performance

`benchmarks/aegis_bench.cpp` measures the transport, serialization, replication and crypto paths:

```bash
cmake -S Build -B build-rel -DCMAKE_BUILD_TYPE=Release -DNETENGINE_BUILD_BENCHMARKS=ON
cmake --build build-rel --parallel && ./build-rel/bin/aegis_bench
```

Results from a 2-vCPU cloud VM (Linux, GCC, Release, loopback, single thread; three runs, ranges shown):

| Area | Result |
|---|---|
| Round-trip latency, 64 B, loopback | p50 5.7-6.1 us, p99 12-20 us (20,000 samples, 0 lost) |
| Throughput, 1,200 B unreliable | 305,000-365,000 packets/s (~370-440 MB/s), 100% delivered |
| Delta snapshots, 5% of objects changing per tick | ~95% less bandwidth than full snapshots (463 B vs 9,216 B at 256 objects) |
| Delta generation at 4,096 objects | ~16 ms -> 0.6-0.7 ms per tick after replacing an O(n^2) scan with hash lookups (>20x) |
| Delta generation at 1,000 objects | 0.86 ms -> 0.09-0.14 ms per tick (6-9x) |
| Bit-packed entity state | 8 B vs 18 B naive layout (56% smaller) |
| AES-128-CTR + HMAC-SHA256 (1,200 B, encrypt + decrypt) | ~130-175 MB/s, 8 B overhead per packet |

## Continuous integration

`.github/workflows/ci.yml` runs on every push and pull request:

- **native** job: configure, build and run the GoogleTest suite on Ubuntu, Windows and macOS; uploads the built library as an artifact.
- **managed** job: builds the native library on Linux and runs the NUnit suite against it.

## Known limitations

- Per-packet IV derivation in `CryptoProvider::DeriveIV` is a lightweight scheme; a production deployment should use an AEAD construction such as ChaCha20-Poly1305 (noted in the source).
- `CryptoProvider` is implemented and unit-tested, but is not yet called from the `ReliableChannel` send/receive path.
- Reliable channel retransmits unacknowledged packets with exponential backoff, but there is no ack-only packet, so one-way reliable traffic is only acknowledged when the receiver sends data back.
- Matchmaking is a stub API.
