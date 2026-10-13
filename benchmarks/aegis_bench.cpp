/*
    Aegis' micro/macro benchmarks
    Reports:
        + Loopback RTT latency
        + Transport throughput
        + Serialization rate/size
        + Delta-snapshot bandwith cost/savings
        + AES-128-CTR & HMAC_SHA256 throughput
*/

#include "Transport/ReliableChannel.h"
#include "Serializer/BitWrite.h"
#include "Serializer/BitRead.h"
#include "Replication/SnapshotManagement.h"
#include "Packets/CryptoProvider.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>
#include <thread>

using namespace Aegis;
using Clock = std::chrono::steady_clock;

static double nowMicroSec() { return std::chrono::duration<double, std::micro>(Clock::now().time_since_epoch()).count(); }
static double nowSec() { return nowMicroSec() / 1e6; }
static double pct(std::vector<double>& v, double p) { std::sort(v.begin(), v.end()); return v[(size_t)((v.size() - 1) * p)]; }

struct Pair {
    Transport::ReliableChannel a, b;
    long aRecv = 0, bRecv = 0;
    bool aGot = false, bGot = false;
    Pair() {
        a.Init(0, [this](const Transport::IncomingPacket&) { ++aRecv; aGot = true; });
        b.Init(0, [this](const Transport::IncomingPacket&) { ++bRecv; bGot = true; });
        
        Transport::RemoteEndpoint ea{}, eb{};
        std::strcpy(ea.ip, "127.0.0.1"); ea.port = a.GetLocalPort();
        std::strcpy(eb.ip, "127.0.0.1"); eb.port = b.GetLocalPort();
        a.AddPeer(2, eb); b.AddPeer(1, ea);
    }   
};

static void Latency(uint8_t channel, const char* name)
{
    Pair p; uint8_t payload[64] = { 1 };
    const int W = 2000, N = 20000; 
    std::vector<double> rtt; rtt.reserve(N); int lost = 0;

    for(int i = 0; i < W + N; ++i)
    {
        p.aGot = p.bGot = false;
        double t0 = nowMicroSec();

        p.a.Send(2, channel, payload, sizeof payload);
        while (!p.bGot && nowMicroSec() - t0 < 50000) p.b.Tick(nowSec());
        
        if(!p.bGot) { ++lost; continue; }

        p.b.Send(1, channel, payload, sizeof payload);
        while (!p.aGot && nowMicroSec() - t0 < 100000) p.a.Tick(nowSec());
        
        if(!p.aGot) { ++lost; continue; }
        if(i >= W) rtt.push_back(nowMicroSec() - t0);
    }
    
    if(rtt.empty()) { std::printf("Latency %-10s no round trips completed (lost %d)\n", name, lost); return; }
    double p50 = pct(rtt, 0.50), p95 = pct(rtt, 0.95), p99 = pct(rtt, 0.99), mx = rtt.back();
    std::printf("Latency %-10s 64B round trip over loopback: p50 %.1f us | p95 %.1f us | p99 %.1f us | max %.1f us (n = %zu, lost %d)\n",
                name, p50, p95, p99, mx, rtt.size(), lost);
}

static void Throughput()
{
    Pair p; 
    uint8_t payload[1200]; 
    std::memset(payload, 0xAB, sizeof payload);

    const long total = 300000; const int BURST = 32; long sent = 0;
    double t0 = nowMicroSec();

    while (sent < total) {
        for (int i = 0; i < BURST && sent < total; ++i, ++sent) p.a.Send(2, 0, payload, sizeof payload);
            p.b.Tick(nowSec());
    }
    for (int i = 0; i < 200; ++i) p.b.Tick(nowSec());
    double sec = (nowMicroSec() - t0) / 1e6;
    std::printf("Throughput 1200B unreliable loopback: %.0f packets/s | %.0f MB/s | delivered %ld/%ld (%.2f%%)\n",
                p.bRecv / sec, p.bRecv * 1200.0 / sec / 1e6, p.bRecv, total, 100.0 * p.bRecv / total);
}

static void Cryptography()
{
    Crypto::CryptoProvider c; 

    uint8_t key[16]; 
    for (int i = 0; i < 16; ++i) 
        key[i] = (uint8_t)(i * 7 + 3); c.SetSeshKey(key);
    
    uint8_t plain[1200], enc[1400], dec[1400]; 
    for (int i = 0; i < 1200; ++i) 
        plain[i] = (uint8_t)i;
    
    uint32_t el = 0, dl = 0; 
    const int N = 200000; 
    bool ok = true;
    double t0 = nowMicroSec();
    
    for (int i = 0; i < N; ++i) {
        c.Encrypt((uint16_t)i, plain, sizeof plain, enc, el);
        c.Decrypt((uint16_t)i, enc, el, dec, dl);
    }
    double sec = (nowMicroSec() - t0) / 1e6;
    ok = (dl == sizeof plain) && std::memcmp(plain, dec, sizeof plain) == 0;
    std::printf("Crypto AES-128-CTR & HMAC-SHA256 encrypt/decrypt 1200B: %.0f MB/s | %.2f us per round trip | roundtrip %s | overhead %u B/packet\n",
                N * 1200.0 / sec / 1e6, sec / N * 1e6, ok ? "OK" : "FAILED", el - 1200);
}

static void BitPacking()
{
    uint8_t buf[64]; 
    const int N = 2000000;
    Serializer::BitWriter probe(buf, sizeof buf);

    for (int i = 0; i < 3; ++i) probe.WriteBits(1234, 16);

    probe.WriteBits(100, 7); 
    for (int i = 0; i < 4; ++i) probe.WriteBool(true); probe.WriteBits(5, 3); probe.Flush();
    uint32_t packed = probe.GetBytesWritten();

    double t0 = nowMicroSec(); 
    uint64_t sink = 0;

    for (int i = 0; i < N; ++i) {
        Serializer::BitWriter w(buf, sizeof buf);

        for (int k = 0; k < 3; ++k) w.WriteBits((uint32_t)(i + k) & 0xFFFF, 16);
        w.WriteBits(i & 127, 7); for (int k = 0; k < 4; ++k) w.WriteBool((i >> k) & 1); w.WriteBits(i & 7, 3); w.Flush();
        
        Serializer::BitReader r(buf, w.GetBytesWritten());
        sink += r.ReadBits(16) + r.ReadBits(16) + r.ReadBits(16) + r.ReadBits(7);
    }
    double sec = (nowMicroSec() - t0) / 1e6;
    std::printf("BitPacking entity state (3x16-bit coords, 7-bit hp, 4 flags, 3-bit team): %u B packed vs 18 B naive (%.0f%% smaller) | %.1f M write+read ops/s (sink %llu)\n",
                packed, 100.0 * (18 - packed) / 18, N / sec / 1e6, (unsigned long long)sink);
}

struct Ent : Replication::Hydra {
    float v[8] = {0};
    void Serialize(Serializer::BitWriter& w) const override { for (float f : v) w.WriteFloat(f); }
    void Deserialize(Serializer::BitReader& r) const override { for (int i = 0; i < 8; ++i) r.ReadFloat(); }
};

static void Snapshots(int objects, double changeRate)
{
    std::vector<Ent> ents(objects); 
    Replication::SnapshotManager m;

    for (auto& e : ents) m.RegisterObject(&e, 255);

    uint32_t tick = 1; m.CaptureSnapshot(tick);
    const int TICKS = 300; 
    double captureUs = 0, deltaUs = 0; 
    double fullB = 0, deltaB = 0; 
    uint32_t seed = 12345;

    for (int t = 0; t < TICKS; ++t) {
        for (auto& e : ents) 
        { 
            seed = seed * 1664525u + 1013904223u; 
            if ((seed >> 8) % 1000 < changeRate * 1000) e.v[0] += 1.0f; 
        }

        uint32_t base = tick; ++tick;
        double a = nowMicroSec(); m.CaptureSnapshot(tick); double b = nowMicroSec();

        Replication::DeltaPacket dp; 
        m.CreateDelta(base, tick, dp); 
        double c = nowMicroSec();
        captureUs += b - a; deltaUs += c - b;
        const Replication::Snapshooter* s = m.FindSnapshot(tick);

        for (auto& o : s->objects) fullB += 4 + o.dataLength;
        for (auto& d : dp.deltas) deltaB += 4 + d.dataLength;
    }
    std::printf("SNAPSHOT %4d objects, %2.0f%% changing/tick: delta %.0f B vs full %.0f B per tick (%.0f%% less bandwidth) | capture %.2f ms + delta %.2f ms per tick\n",
                objects, changeRate * 100, deltaB / TICKS, fullB / TICKS, 100.0 * (1 - deltaB / fullB), captureUs / TICKS / 1000, deltaUs / TICKS / 1000);
}

int main()
{
    std::printf("Aegis benchmarks (single machine, loopback, single thread)\n");
    Latency(0, "unreliable"); Latency(0, "reliable");
    
    Throughput();
    Cryptography();
    BitPacking();

    Snapshots(256, 0.05); 
    Snapshots(1000, 0.05); 
    Snapshots(4096, 0.05);

    return 0;
}