# Aegis

![Logo](Assets/Images/Aegis%20Logo.png)

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

CMake options: `NETENGINE_BUILD_TESTS` (default OFF, fetches GoogleTest), `NETENGINE_USE_OPENSSL` (ON), `NETENGINE_ENABLE_LOGGING` (ON).

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

## Continuous integration

`.github/workflows/ci.yml` runs on every push and pull request:

- **native** job: configure, build and run the GoogleTest suite on Ubuntu, Windows and macOS; uploads the built library as an artifact.
- **managed** job: builds the native library on Linux and runs the NUnit suite against it.

## Known limitations

- Per-packet IV derivation in `CryptoProvider::DeriveIV` is a lightweight scheme; a production deployment should use an AEAD construction such as ChaCha20-Poly1305 (noted in the source).
- Matchmaking is a stub API.
