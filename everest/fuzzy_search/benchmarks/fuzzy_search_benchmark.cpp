// fuzzy_search 基准测试：500 万名字，单进程、单线程、Windows Release。
// 输出 JSON，供 make_report.py 生成面向策划的 HTML 报告。
#include "system/fuzzy_search/fuzzy_search_system.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using System = metaverse::FuzzySearchSystem<std::uint64_t, true>;
using Clock = std::chrono::steady_clock;

constexpr std::uint32_t kNameCount = 5000000;
constexpr std::uint32_t kMaxStringLength = 8;
constexpr std::uint32_t kMinStringLength = 2;
constexpr std::size_t kMaxResults = 500;
constexpr int kRandomQueryCount = 100000;
constexpr int kHotQueryCount = 10000;
constexpr int kHotGramCount = 100;
constexpr std::uint64_t kSeed = 20261030;
constexpr std::uint32_t kRemainCount = 500000;  // 删除 10%

struct QueryResult {
    std::string label;
    std::uint64_t count = 0;
    double seconds = 0.0;
    double qps = 0.0;
    double p50Us = 0.0;
    double p95Us = 0.0;
    double p99Us = 0.0;
    double avgResults = 0.0;
    double cappedRatio = 0.0;
};

struct PhaseMemory {
    double workingSetMb = 0.0;
    double peakWorkingSetMb = 0.0;
    double estimatedIndexMb = 0.0;
};

std::uint64_t NowNs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
}

double SecondsSince(std::uint64_t startNs) {
    return static_cast<double>(NowNs() - startNs) / 1e9;
}

double BytesToMb(std::size_t bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

#ifdef _WIN32
std::size_t WorkingSetBytes() {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
        return counters.WorkingSetSize;
    }
    return 0;
}

std::size_t PeakWorkingSetBytes() {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
        return counters.PeakWorkingSetSize;
    }
    return 0;
}

std::size_t PrivateBytes() {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
        return counters.PrivateUsage;
    }
    return 0;
}

std::size_t TotalRamBytes() {
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return static_cast<std::size_t>(status.ullTotalPhys);
    }
    return 0;
}
#else
std::size_t WorkingSetBytes() { return 0; }
std::size_t PeakWorkingSetBytes() { return 0; }
std::size_t PrivateBytes() { return 0; }
std::size_t TotalRamBytes() { return 0; }
#endif

void AppendUtf8(std::string& out, char32_t codepoint) {
    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

std::size_t Utf8Length(unsigned char first) {
    if (first < 0x80) return 1;
    if ((first & 0xE0) == 0xC0) return 2;
    if ((first & 0xF0) == 0xE0) return 3;
    return 4;
}

std::vector<std::pair<std::size_t, std::size_t>> CodepointOffsets(const std::string& text) {
    std::vector<std::pair<std::size_t, std::size_t>> offsets;
    offsets.reserve(kMaxStringLength);
    std::size_t index = 0;
    while (index < text.size()) {
        const auto length = Utf8Length(static_cast<unsigned char>(text[index]));
        offsets.emplace_back(index, length);
        index += length;
        if (offsets.size() > kMaxStringLength) break;
    }
    return offsets;
}

bool IsAsciiLetter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

void MaybeFlipAsciiCase(std::string& text, std::mt19937_64& rng) {
    for (char& c : text) {
        if (IsAsciiLetter(c) && (rng() % 2 == 0)) {
            c = static_cast<char>(c ^ 0x20);
        }
    }
}

std::string MakeRandomSubstring(const std::string& name, int charCount, std::mt19937_64& rng) {
    auto offsets = CodepointOffsets(name);
    if (static_cast<int>(offsets.size()) < charCount) return {};
    const std::size_t start = rng() % (offsets.size() - charCount + 1);
    const std::size_t begin = offsets[start].first;
    const std::size_t end = offsets[start + charCount - 1].first + offsets[start + charCount - 1].second;
    std::string query = name.substr(begin, end - begin);
    MaybeFlipAsciiCase(query, rng);
    return query;
}

std::string EncodeCodepoint(char32_t codepoint) {
    std::string text;
    AppendUtf8(text, codepoint);
    return text;
}

std::string EncodeBigram(std::uint64_t key) {
    std::string text;
    AppendUtf8(text, static_cast<char32_t>(key >> 32));
    AppendUtf8(text, static_cast<char32_t>(key & 0xFFFFFFFFu));
    return text;
}

struct CountedGram {
    std::uint64_t count = 0;
    std::string text;
};

template <typename Key, typename Encoder>
std::vector<CountedGram> TopGrams(const std::unordered_map<Key, std::uint64_t>& counts,
                                  std::size_t limit, Encoder encoder) {
    std::vector<std::pair<std::uint64_t, Key>> items;
    items.reserve(counts.size());
    for (const auto& entry : counts) {
        items.emplace_back(entry.second, entry.first);
    }
    const std::size_t take = std::min(limit, items.size());
    std::partial_sort(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(take), items.end(),
                       [](const auto& left, const auto& right) { return left.first > right.first; });
    std::vector<CountedGram> result;
    result.reserve(take);
    for (std::size_t i = 0; i < take; ++i) {
        result.push_back(CountedGram{items[i].first, encoder(items[i].second)});
    }
    return result;
}

PhaseMemory MemoryNow() {
    PhaseMemory memory;
    memory.workingSetMb = BytesToMb(WorkingSetBytes());
    memory.peakWorkingSetMb = BytesToMb(PeakWorkingSetBytes());
    return memory;
}

System::Stats ToStats(const System& index) {
    return index.stats();
}

void WriteStats(std::ostream& out, const System::Stats& stats) {
    out << "{"
        << "\"live_documents\":" << stats.liveDocuments << ","
        << "\"total_documents\":" << stats.totalDocuments << ","
        << "\"tombstone_documents\":" << stats.tombstoneDocuments << ","
        << "\"name_bytes\":" << stats.nameBytes << ","
        << "\"base_gram_entries\":" << stats.baseGramEntries << ","
        << "\"base_posting_bytes\":" << stats.basePostingBytes << ","
        << "\"delta_gram_entries\":" << stats.deltaGramEntries << ","
        << "\"delta_postings\":" << stats.deltaPostings << ","
        << "\"estimated_bytes\":" << stats.estimatedBytes << ","
        << "\"unigram_enabled\":" << (stats.unigramEnabled ? "true" : "false")
        << "}";
}

void WriteQueryResult(std::ostream& out, const QueryResult& result) {
    out << "{"
        << "\"label\":\"" << result.label << "\","
        << "\"count\":" << result.count << ","
        << "\"seconds\":" << std::fixed << std::setprecision(6) << result.seconds << ","
        << "\"qps\":" << std::fixed << std::setprecision(3) << result.qps << ","
        << "\"p50_us\":" << std::fixed << std::setprecision(3) << result.p50Us << ","
        << "\"p95_us\":" << std::fixed << std::setprecision(3) << result.p95Us << ","
        << "\"p99_us\":" << std::fixed << std::setprecision(3) << result.p99Us << ","
        << "\"avg_results\":" << std::fixed << std::setprecision(3) << result.avgResults << ","
        << "\"capped_ratio\":" << std::fixed << std::setprecision(4) << result.cappedRatio
        << "}";
}

QueryResult RunQueries(const System& index, const std::vector<std::string>& queries, const std::string& label) {
    QueryResult result;
    result.label = label;
    result.count = queries.size();
    if (queries.empty()) return result;

    std::vector<std::uint64_t> ids;
    ids.reserve(kMaxResults);
    const std::size_t warmup = std::min<std::size_t>(queries.size(), 5000);
    for (std::size_t i = 0; i < warmup; ++i) {
        ids.clear();
        index.Search(queries[i], kMaxResults, ids);
    }

    std::vector<std::uint64_t> latencies;
    latencies.reserve(queries.size());
    std::uint64_t totalResults = 0;
    std::uint64_t capped = 0;
    const auto start = Clock::now();
    for (const std::string& query : queries) {
        ids.clear();
        const auto queryStart = Clock::now();
        const auto status = index.Search(query, kMaxResults, ids);
        const auto queryEnd = Clock::now();
        if (status != System::Status::kOk) {
            std::cerr << "Search failed: label=" << label << " status=" << static_cast<int>(status) << "\n";
            std::exit(2);
        }
        latencies.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(queryEnd - queryStart).count()));
        totalResults += ids.size();
        if (ids.size() == kMaxResults) ++capped;
    }
    const auto end = Clock::now();
    result.seconds = std::chrono::duration<double>(end - start).count();
    result.qps = result.seconds > 0.0 ? static_cast<double>(queries.size()) / result.seconds : 0.0;
    std::sort(latencies.begin(), latencies.end());
    const auto percentile = [&](double p) {
        const std::size_t index = static_cast<std::size_t>(p * static_cast<double>(latencies.size() - 1));
        return static_cast<double>(latencies[index]) / 1000.0;
    };
    result.p50Us = percentile(0.50);
    result.p95Us = percentile(0.95);
    result.p99Us = percentile(0.99);
    result.avgResults = static_cast<double>(totalResults) / static_cast<double>(queries.size());
    result.cappedRatio = static_cast<double>(capped) / static_cast<double>(queries.size());
    return result;
}

struct Dataset {
    std::vector<std::string> names;
    std::vector<std::string> q1;
    std::vector<std::string> q2;
    std::vector<std::string> q38;
    std::vector<std::string> hot1;
    std::vector<std::string> hot2;
    std::array<std::uint64_t, 9> lengthCounts{};
    std::vector<CountedGram> topUnigrams;
    std::vector<CountedGram> topBigrams;
    std::vector<std::string> sampleNames;
    double avgCodepoints = 0.0;
    double cjkRatio = 0.0;
    double asciiRatio = 0.0;
    std::size_t totalNameBytes = 0;
};

Dataset GenerateDataset(std::uint32_t minSearchLength) {
    Dataset dataset;
    dataset.names.reserve(kNameCount);
    std::unordered_set<std::string> seen;
    seen.reserve(static_cast<std::size_t>(kNameCount) * 2);
    std::unordered_map<char32_t, std::uint64_t> unigramFreq;
    std::unordered_map<std::uint64_t, std::uint64_t> bigramFreq;
    unigramFreq.reserve(4096);
    bigramFreq.reserve(3000000);

    std::mt19937_64 rng(kSeed);
    constexpr std::size_t kCjkCount = 3500;
    std::vector<char32_t> cjkCharacters;
    cjkCharacters.reserve(kCjkCount);
    std::vector<double> cjkWeights;
    cjkWeights.reserve(kCjkCount);
    for (std::size_t i = 0; i < kCjkCount; ++i) {
        cjkCharacters.push_back(static_cast<char32_t>(0x4E00 + i));
        cjkWeights.push_back(1.0 / std::pow(static_cast<double>(i + 1), 1.0));
    }
    std::discrete_distribution<std::size_t> cjkDistribution(cjkWeights.begin(), cjkWeights.end());
    constexpr std::string_view kAsciiLetters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    std::uint64_t cjkCharactersTotal = 0;
    std::uint64_t asciiCharactersTotal = 0;
    std::uint64_t codepointTotal = 0;

    while (dataset.names.size() < kNameCount) {
        const std::uint32_t length = 2 + static_cast<std::uint32_t>(rng() % 7);
        std::string name;
        name.reserve(length * 3);
        std::vector<char32_t> codepoints;
        codepoints.reserve(length);
        for (std::uint32_t i = 0; i < length; ++i) {
            if (rng() % 100 < 70) {
                const char32_t codepoint = cjkCharacters[cjkDistribution(rng)];
                codepoints.push_back(codepoint);
                AppendUtf8(name, codepoint);
                ++cjkCharactersTotal;
            } else {
                const char letter = kAsciiLetters[rng() % kAsciiLetters.size()];
                codepoints.push_back(static_cast<char32_t>(static_cast<unsigned char>(letter)));
                name.push_back(letter);
                ++asciiCharactersTotal;
            }
        }
        if (!seen.insert(name).second) {
            continue;
        }
        ++dataset.lengthCounts[length];
        for (char32_t codepoint : codepoints) {
            ++unigramFreq[codepoint];
        }
        for (std::size_t i = 0; i + 1 < codepoints.size(); ++i) {
            const std::uint64_t key = (static_cast<std::uint64_t>(codepoints[i]) << 32) |
                                      static_cast<std::uint64_t>(codepoints[i + 1]);
            ++bigramFreq[key];
        }
        dataset.totalNameBytes += name.size();
        codepointTotal += codepoints.size();
        dataset.names.push_back(std::move(name));
    }

    dataset.avgCodepoints = static_cast<double>(codepointTotal) / static_cast<double>(kNameCount);
    dataset.cjkRatio = static_cast<double>(cjkCharactersTotal) /
                       static_cast<double>(cjkCharactersTotal + asciiCharactersTotal);
    dataset.asciiRatio = 1.0 - dataset.cjkRatio;

    const auto topUnigrams = TopGrams(unigramFreq, kHotGramCount, [](char32_t codepoint) { return EncodeCodepoint(codepoint); });
    const auto topBigrams = TopGrams(bigramFreq, kHotGramCount, [](std::uint64_t key) { return EncodeBigram(key); });
    std::unordered_map<char32_t, std::uint64_t>().swap(unigramFreq);
    std::unordered_map<std::uint64_t, std::uint64_t>().swap(bigramFreq);
    std::unordered_set<std::string>().swap(seen);

    constexpr std::size_t kReportGramCount = 10;
    dataset.topUnigrams.assign(topUnigrams.begin(), topUnigrams.begin() + std::min(kReportGramCount, topUnigrams.size()));
    dataset.topBigrams.assign(topBigrams.begin(), topBigrams.begin() + std::min(kReportGramCount, topBigrams.size()));

    const std::size_t sampleStride = std::max<std::size_t>(1, dataset.names.size() / 20);
    for (std::size_t i = 0; i < 20 && i * sampleStride < dataset.names.size(); ++i) {
        dataset.sampleNames.push_back(dataset.names[i * sampleStride]);
    }

    dataset.q1.reserve(kRandomQueryCount);
    dataset.q2.reserve(kRandomQueryCount);
    dataset.q38.reserve(kRandomQueryCount);
    for (int i = 0; i < kRandomQueryCount; ++i) {
        const std::string& name = dataset.names[rng() % kNameCount];
        dataset.q1.push_back(MakeRandomSubstring(name, 1, rng));
        dataset.q2.push_back(MakeRandomSubstring(name, 2, rng));
        std::string q38;
        while (q38.empty()) {
            const std::string& longName = dataset.names[rng() % kNameCount];
            const int chars = 3 + static_cast<int>(rng() % 6);
            q38 = MakeRandomSubstring(longName, chars, rng);
        }
        dataset.q38.push_back(std::move(q38));
    }

    if (minSearchLength == 1) {
        dataset.hot1.reserve(kHotQueryCount);
        for (int i = 0; i < kHotQueryCount; ++i) {
            std::string query = topUnigrams[rng() % topUnigrams.size()].text;
            MaybeFlipAsciiCase(query, rng);
            dataset.hot1.push_back(std::move(query));
        }
    }
    dataset.hot2.reserve(kHotQueryCount);
    for (int i = 0; i < kHotQueryCount; ++i) {
        std::string query = topBigrams[rng() % topBigrams.size()].text;
        MaybeFlipAsciiCase(query, rng);
        dataset.hot2.push_back(std::move(query));
    }

    return dataset;
}

void WriteMemory(std::ostream& out, const PhaseMemory& memory) {
    out << "{"
        << "\"working_set_mb\":" << std::fixed << std::setprecision(2) << memory.workingSetMb << ","
        << "\"peak_working_set_mb\":" << std::fixed << std::setprecision(2) << memory.peakWorkingSetMb << ","
        << "\"estimated_index_mb\":" << std::fixed << std::setprecision(2) << memory.estimatedIndexMb
        << "}";
}

void WriteBatchQps(std::ostream& out, const std::vector<double>& batchQps) {
    out << "[";
    for (std::size_t i = 0; i < batchQps.size(); ++i) {
        if (i != 0) out << ",";
        out << std::fixed << std::setprecision(1) << batchQps[i];
    }
    out << "]";
}

std::string JsonEscape(std::string_view text) {
    std::string result;
    result.reserve(text.size() + 8);
    for (char c : text) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

void WriteNamesFile(const std::vector<std::string>& names, const std::string& path, std::size_t count) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "Cannot open names output: " << path << "\n";
        return;
    }
    const std::size_t target = std::min(count, names.size());
    const std::size_t stride = std::max<std::size_t>(1, names.size() / std::max<std::size_t>(1, target));
    std::size_t written = 0;
    for (std::size_t i = 0; i < names.size() && written < target; i += stride) {
        out << names[i] << '\n';
        ++written;
    }
    out.close();
    std::cout << "Names sample written: " << path << " (" << written << " lines)\n";
}

int Run(std::uint32_t minSearchLength, const std::string& outputPath, const std::string& namesOutput) {
#ifdef _MSC_VER
    constexpr int kCompilerVersion = _MSC_VER;
#else
    constexpr int kCompilerVersion = 0;
#endif
    std::cout << "Generating 5,000,000 names (minSearch=" << minSearchLength << ")...\n";
    Dataset dataset = GenerateDataset(minSearchLength);
    std::cout << "Dataset ready: avg codepoints=" << dataset.avgCodepoints
              << " cjk=" << dataset.cjkRatio << " ascii=" << dataset.asciiRatio << "\n";
    WriteNamesFile(dataset.names, namesOutput, 100000);

    const PhaseMemory baseline = MemoryNow();
    const std::size_t baselinePrivate = PrivateBytes();

    System::Options options;
    options.maxStringLength = kMaxStringLength;
    options.minSearchLength = minSearchLength;
    options.caseInsensitive = true;

    System::Status createStatus = System::Status::kOk;
    auto index = System::Create(options, &createStatus);
    if (!index) {
        std::cerr << "Create failed: " << static_cast<int>(createStatus) << "\n";
        return 3;
    }

    index->Reserve(kNameCount, dataset.totalNameBytes);

    std::cout << "Adding names...\n";
    std::vector<double> addBatchQps;
    std::uint64_t batchStartNs = NowNs();
    const std::uint64_t addStartNs = batchStartNs;
    for (std::uint32_t i = 0; i < kNameCount; ++i) {
        const auto status = index->Add(static_cast<std::uint64_t>(i), dataset.names[i]);
        if (status != System::Status::kOk) {
            std::cerr << "Add failed at " << i << ": " << static_cast<int>(status) << "\n";
            return 4;
        }
        if ((i + 1) % 100000 == 0) {
            const std::uint64_t now = NowNs();
            const double seconds = static_cast<double>(now - batchStartNs) / 1e9;
            addBatchQps.push_back(seconds > 0.0 ? 100000.0 / seconds : 0.0);
            batchStartNs = now;
        }
    }
    const double addSeconds = SecondsSince(addStartNs);
    const System::Stats afterAddStats = ToStats(*index);
    PhaseMemory afterAdd = MemoryNow();
    afterAdd.estimatedIndexMb = BytesToMb(afterAddStats.estimatedBytes);
    std::cout << "Add done: " << addSeconds << "s, qps=" << (kNameCount / addSeconds) << "\n";

    std::cout << "Compacting...\n";
    std::atomic<bool> sampling{true};
    std::atomic<std::size_t> compactPeak{0};
    std::thread sampler([&sampling, &compactPeak]() {
        while (sampling.load(std::memory_order_relaxed)) {
            const std::size_t current = WorkingSetBytes();
            std::size_t previous = compactPeak.load(std::memory_order_relaxed);
            while (current > previous &&
                   !compactPeak.compare_exchange_weak(previous, current, std::memory_order_relaxed)) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    const std::uint64_t compactStartNs = NowNs();
    index->Compact();
    const double compactSeconds = SecondsSince(compactStartNs);
    sampling.store(false, std::memory_order_relaxed);
    sampler.join();
    const System::Stats afterCompactStats = ToStats(*index);
    PhaseMemory afterCompact = MemoryNow();
    afterCompact.estimatedIndexMb = BytesToMb(afterCompactStats.estimatedBytes);
    const double compactPeakMb = BytesToMb(compactPeak.load());
    std::cout << "Compact done: " << compactSeconds << "s peak=" << compactPeakMb << "MB\n";

    std::vector<QueryResult> queryResults;
    if (minSearchLength == 1) {
        std::cout << "Querying 1 char (random)...\n";
        queryResults.push_back(RunQueries(*index, dataset.q1, "one_random"));
        std::cout << "Querying 1 char (hot)...\n";
        queryResults.push_back(RunQueries(*index, dataset.hot1, "one_hot"));
    }
    std::cout << "Querying 2 chars (random)...\n";
    queryResults.push_back(RunQueries(*index, dataset.q2, "two_random"));
    std::cout << "Querying 2 chars (hot)...\n";
    queryResults.push_back(RunQueries(*index, dataset.hot2, "two_hot"));
    std::cout << "Querying 3-8 chars (random)...\n";
    queryResults.push_back(RunQueries(*index, dataset.q38, "three_to_eight_random"));

    std::cout << "Removing 10%...\n";
    std::mt19937_64 removeRng(kSeed + 1);
    std::vector<std::uint64_t> allIds(kNameCount);
    std::iota(allIds.begin(), allIds.end(), std::uint64_t{0});
    std::shuffle(allIds.begin(), allIds.end(), removeRng);
    allIds.resize(kRemainCount);
    const std::uint64_t removeStartNs = NowNs();
    for (std::uint64_t id : allIds) {
        const auto status = index->Remove(id);
        if (status != System::Status::kOk) {
            std::cerr << "Remove failed: " << static_cast<int>(status) << "\n";
            return 5;
        }
    }
    const double removeSeconds = SecondsSince(removeStartNs);
    const System::Stats afterRemoveStats = ToStats(*index);
    PhaseMemory afterRemove = MemoryNow();
    afterRemove.estimatedIndexMb = BytesToMb(afterRemoveStats.estimatedBytes);
    std::cout << "Remove done: " << removeSeconds << "s\n";

    std::cout << "Compacting after remove...\n";
    sampling.store(true, std::memory_order_relaxed);
    compactPeak.store(0, std::memory_order_relaxed);
    std::thread sampler2([&sampling, &compactPeak]() {
        while (sampling.load(std::memory_order_relaxed)) {
            const std::size_t current = WorkingSetBytes();
            std::size_t previous = compactPeak.load(std::memory_order_relaxed);
            while (current > previous &&
                   !compactPeak.compare_exchange_weak(previous, current, std::memory_order_relaxed)) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    const std::uint64_t compact2StartNs = NowNs();
    index->Compact();
    const double compact2Seconds = SecondsSince(compact2StartNs);
    sampling.store(false, std::memory_order_relaxed);
    sampler2.join();
    const System::Stats afterCompact2Stats = ToStats(*index);
    PhaseMemory afterCompact2 = MemoryNow();
    afterCompact2.estimatedIndexMb = BytesToMb(afterCompact2Stats.estimatedBytes);
    const double compact2PeakMb = BytesToMb(compactPeak.load());

    std::ofstream out(outputPath);
    if (!out) {
        std::cerr << "Cannot open output: " << outputPath << "\n";
        return 6;
    }
    out << std::fixed << std::setprecision(3);
    out << "{";
    out << "\"generated_at\":\"2026-10-30\",";
    out << "\"config\":{\"min_search\":" << minSearchLength << ",\"max_string\":" << kMaxStringLength
        << ",\"case_insensitive\":true,\"bmp_only\":true,\"max_results\":" << kMaxResults << "},";
    out << "\"environment\":{\"compiler_msvc\":" << kCompilerVersion << ",\"pointer_bits\":" << (sizeof(void*) * 8)
        << ",\"total_ram_mb\":" << BytesToMb(TotalRamBytes()) << "},";
    out << "\"dataset\":{\"count\":" << kNameCount << ",\"name_bytes\":" << dataset.totalNameBytes
        << ",\"avg_codepoints\":" << dataset.avgCodepoints << ",\"cjk_ratio\":" << dataset.cjkRatio
        << ",\"ascii_ratio\":" << dataset.asciiRatio << ",\"seed\":" << kSeed << "},";
    out << "\"names_file\":\"" << JsonEscape(namesOutput) << "\",";
    out << "\"length_distribution\":[";
    for (int length = 2; length <= 8; ++length) {
        if (length != 2) out << ",";
        const auto count = dataset.lengthCounts[length];
        const double ratio = static_cast<double>(count) / static_cast<double>(kNameCount);
        out << "{\"length\":" << length << ",\"count\":" << count << ",\"ratio\":" << ratio << "}";
    }
    out << "],";
    auto writeGrams = [&](const std::vector<CountedGram>& grams) {
        out << "[";
        for (std::size_t i = 0; i < grams.size(); ++i) {
            if (i != 0) out << ",";
            out << "{\"text\":\"" << JsonEscape(grams[i].text) << "\",\"count\":" << grams[i].count << "}";
        }
        out << "]";
    };
    out << "\"top_unigrams\":";
    writeGrams(dataset.topUnigrams);
    out << ",";
    out << "\"top_bigrams\":";
    writeGrams(dataset.topBigrams);
    out << ",";
    out << "\"sample_names\":[";
    for (std::size_t i = 0; i < dataset.sampleNames.size(); ++i) {
        if (i != 0) out << ",";
        out << "\"" << JsonEscape(dataset.sampleNames[i]) << "\"";
    }
    out << "],";
    out << "\"baseline\":";
    WriteMemory(out, baseline);
    out << ",";
    out << "\"baseline_private_mb\":" << BytesToMb(baselinePrivate) << ",";
    out << "\"add\":{\"count\":" << kNameCount << ",\"seconds\":" << addSeconds << ",\"qps\":"
        << (kNameCount / addSeconds) << ",\"batch_qps\":";
    WriteBatchQps(out, addBatchQps);
    out << "},";
    out << "\"after_add\":";
    WriteMemory(out, afterAdd);
    out << ",\"after_add_stats\":";
    WriteStats(out, afterAddStats);
    out << ",";
    out << "\"compact1\":{\"seconds\":" << compactSeconds << ",\"peak_working_set_mb\":" << compactPeakMb
        << ",\"after\":";
    WriteMemory(out, afterCompact);
    out << ",\"after_stats\":";
    WriteStats(out, afterCompactStats);
    out << "},";
    out << "\"queries\":[";
    for (std::size_t i = 0; i < queryResults.size(); ++i) {
        if (i != 0) out << ",";
        WriteQueryResult(out, queryResults[i]);
    }
    out << "],";
    out << "\"remove\":{\"count\":" << kRemainCount << ",\"seconds\":" << removeSeconds << ",\"qps\":"
        << (static_cast<double>(kRemainCount) / removeSeconds) << ",\"after\":";
    WriteMemory(out, afterRemove);
    out << ",\"after_stats\":";
    WriteStats(out, afterRemoveStats);
    out << "},";
    out << "\"compact2\":{\"seconds\":" << compact2Seconds << ",\"peak_working_set_mb\":" << compact2PeakMb
        << ",\"after\":";
    WriteMemory(out, afterCompact2);
    out << ",\"after_stats\":";
    WriteStats(out, afterCompact2Stats);
    out << "}";
    out << "}";
    out.close();

    std::cout << "JSON written: " << outputPath << "\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::uint32_t minSearch = 1;
    std::string output = "fuzzy_search_benchmark.json";
    std::string namesOutput = "names_sample_100k.txt";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--min-search=", 0) == 0) {
            minSearch = static_cast<std::uint32_t>(std::stoul(arg.substr(13)));
        } else if (arg.rfind("--output=", 0) == 0) {
            output = arg.substr(9);
        } else if (arg.rfind("--names-out=", 0) == 0) {
            namesOutput = arg.substr(12);
        }
    }
    if (minSearch != 1 && minSearch != 2) {
        std::cerr << "min-search must be 1 or 2\n";
        return 1;
    }
    std::cout << "fuzzy_search benchmark, minSearch=" << minSearch << "\n";
    return Run(minSearch, output, namesOutput);
}
