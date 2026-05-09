#include "common/RnNoiseCommonPlugin.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <vector>

namespace {

constexpr int kExpectedSampleRate = 48000;
constexpr size_t kDenoiseBlockSize = 480;
constexpr size_t kMinimumAggregateClipCount = 10;

struct Audio {
    int sampleRate = 0;
    std::vector<float> samples;
};

struct Options {
    std::string dataDir;
    std::string outputDir;
    size_t limit = 25;
    std::vector<size_t> sampleFrames = {200, 480, 512};
    uint32_t retroactiveVadBlocks = 5;
    uint32_t ignoreStartMs = 100;
    uint32_t maxLagMs = 100;
    size_t lagSearchStep = 32;
    size_t jobs = std::max(1u, std::thread::hardware_concurrency());
    bool writeAll = false;
};

struct ClipMetrics {
    std::string name;
    size_t sampleFrames = 0;
    double inputSnr = 0.0;
    double processedSnr = 0.0;
    double snrLift = 0.0;
    double quietAttenuation = 0.0;
    double outputRms = 0.0;
    double outputPeak = 0.0;
    size_t bestLag = 0;
    size_t maxLag = 0;
    bool bestLagNearLimit = false;
};

struct Summary {
    size_t sampleFrames = 0;
    size_t clips = 0;
    double averageSnrLift = 0.0;
    double minimumSnrLift = std::numeric_limits<double>::infinity();
    double averageQuietAttenuation = 0.0;
};

struct TaskResult {
    ClipMetrics metrics;
    std::string error;
};

uint16_t readLe16(std::istream &stream) {
    unsigned char bytes[2] = {};
    stream.read(reinterpret_cast<char *>(bytes), sizeof(bytes));
    if (!stream) {
        throw std::runtime_error("Unexpected end of file");
    }
    return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
}

uint32_t readLe32(std::istream &stream) {
    unsigned char bytes[4] = {};
    stream.read(reinterpret_cast<char *>(bytes), sizeof(bytes));
    if (!stream) {
        throw std::runtime_error("Unexpected end of file");
    }
    return static_cast<uint32_t>(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
}

void writeLe16(std::ostream &stream, uint16_t value) {
    const unsigned char bytes[2] = {
            static_cast<unsigned char>(value & 0xff),
            static_cast<unsigned char>((value >> 8) & 0xff),
    };
    stream.write(reinterpret_cast<const char *>(bytes), sizeof(bytes));
}

void writeLe32(std::ostream &stream, uint32_t value) {
    const unsigned char bytes[4] = {
            static_cast<unsigned char>(value & 0xff),
            static_cast<unsigned char>((value >> 8) & 0xff),
            static_cast<unsigned char>((value >> 16) & 0xff),
            static_cast<unsigned char>((value >> 24) & 0xff),
    };
    stream.write(reinterpret_cast<const char *>(bytes), sizeof(bytes));
}

bool fileExists(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool dirExists(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string joinPath(const std::string &left, const std::string &right) {
    if (left.empty()) {
        return right;
    }
    if (left[left.size() - 1] == '/') {
        return left + right;
    }
    return left + "/" + right;
}

std::string shellQuote(const std::string &value) {
    std::string result = "'";
    for (char ch: value) {
        if (ch == '\'') {
            result += "'\\''";
        } else {
            result += ch;
        }
    }
    result += "'";
    return result;
}

void makeDirs(const std::string &path) {
    if (path.empty() || dirExists(path)) {
        return;
    }

    size_t pos = 0;
    while (true) {
        pos = path.find('/', pos + 1);
        const std::string part = pos == std::string::npos ? path : path.substr(0, pos);
        if (!part.empty() && !dirExists(part)) {
            if (mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) {
                throw std::runtime_error("Failed to create directory " + part + ": " + std::strerror(errno));
            }
        }
        if (pos == std::string::npos) {
            break;
        }
    }
}

std::vector<std::string> listWavNames(const std::string &dir) {
    DIR *handle = opendir(dir.c_str());
    if (handle == nullptr) {
        throw std::runtime_error("Failed to open directory " + dir);
    }

    std::vector<std::string> names;
    while (dirent *entry = readdir(handle)) {
        const std::string name = entry->d_name;
        if (name.size() > 4 && name.substr(name.size() - 4) == ".wav") {
            names.push_back(name);
        }
    }
    closedir(handle);
    std::sort(names.begin(), names.end());
    return names;
}

std::string withoutExtension(const std::string &name) {
    const size_t pos = name.rfind('.');
    return pos == std::string::npos ? name : name.substr(0, pos);
}

Audio readWavMono16(const std::string &path) {
    std::ifstream stream(path.c_str(), std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open " + path);
    }

    char riff[4] = {};
    char wave[4] = {};
    stream.read(riff, sizeof(riff));
    readLe32(stream);
    stream.read(wave, sizeof(wave));
    if (std::strncmp(riff, "RIFF", 4) != 0 || std::strncmp(wave, "WAVE", 4) != 0) {
        throw std::runtime_error(path + " is not a RIFF/WAVE file");
    }

    uint16_t audioFormat = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    std::vector<unsigned char> data;

    while (stream) {
        char chunkId[4] = {};
        stream.read(chunkId, sizeof(chunkId));
        if (!stream) {
            break;
        }
        const uint32_t chunkSize = readLe32(stream);
        const std::streampos chunkStart = stream.tellg();

        if (std::strncmp(chunkId, "fmt ", 4) == 0) {
            audioFormat = readLe16(stream);
            channels = readLe16(stream);
            sampleRate = readLe32(stream);
            readLe32(stream);
            readLe16(stream);
            bitsPerSample = readLe16(stream);
        } else if (std::strncmp(chunkId, "data", 4) == 0) {
            data.resize(chunkSize);
            stream.read(reinterpret_cast<char *>(data.data()), chunkSize);
        }

        stream.clear();
        stream.seekg(chunkStart + static_cast<std::streamoff>(chunkSize + (chunkSize & 1u)));
    }

    if (audioFormat != 1 || channels != 1 || bitsPerSample != 16 || sampleRate != kExpectedSampleRate) {
        std::ostringstream message;
        message << path << " must be mono 48 kHz PCM16, got format=" << audioFormat
                << " channels=" << channels << " sampleRate=" << sampleRate
                << " bits=" << bitsPerSample;
        throw std::runtime_error(message.str());
    }
    if (data.empty() || (data.size() % 2) != 0) {
        throw std::runtime_error(path + " has no valid PCM data");
    }

    Audio audio;
    audio.sampleRate = static_cast<int>(sampleRate);
    audio.samples.resize(data.size() / 2);
    for (size_t i = 0; i < audio.samples.size(); i++) {
        const uint16_t raw = static_cast<uint16_t>(data[i * 2] | (data[i * 2 + 1] << 8));
        const int16_t signedValue = static_cast<int16_t>(raw);
        audio.samples[i] = static_cast<float>(signedValue / 32768.0f);
    }
    return audio;
}

void writeWavMono16(const std::string &path, const std::vector<float> &samples) {
    std::ofstream stream(path.c_str(), std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to write " + path);
    }

    const uint32_t dataSize = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    stream.write("RIFF", 4);
    writeLe32(stream, 36u + dataSize);
    stream.write("WAVE", 4);
    stream.write("fmt ", 4);
    writeLe32(stream, 16);
    writeLe16(stream, 1);
    writeLe16(stream, 1);
    writeLe32(stream, kExpectedSampleRate);
    writeLe32(stream, kExpectedSampleRate * 2);
    writeLe16(stream, 2);
    writeLe16(stream, 16);
    stream.write("data", 4);
    writeLe32(stream, dataSize);

    for (float sample: samples) {
        const float clipped = std::max(-1.0f, std::min(1.0f, sample));
        const int32_t scaled = static_cast<int32_t>(std::lrint(clipped * 32767.0f));
        writeLe16(stream, static_cast<uint16_t>(static_cast<int16_t>(scaled)));
    }
}

void printUsage(const char *program) {
    std::cout
            << "Usage: " << program << " [options]\n"
            << "\n"
            << "Options:\n"
            << "  --data-dir <path>        Required dataset directory containing Edinburgh ZIPs or extracted folders\n"
            << "  --output-dir <path>      Required directory for extracted cache and processed WAV output\n"
            << "  --limit <N>              Number of matching clips to evaluate, default 25\n"
            << "  --sample-frames <N[,N]>  Process call size. Repeatable. Default 200,480,512\n"
            << "  --retroactive-vad <N>    Retroactive VAD blocks for stats check, default 5\n"
            << "  --ignore-start-ms <N>    Ignore this much startup audio in metrics, default 100\n"
            << "  --max-lag-ms <N>         Maximum alignment lag to search, default 100\n"
            << "  --lag-search-step <N>    Coarse lag-search step before exact refinement, default 32\n"
            << "  --jobs <N>               Parallel clip workers, default hardware concurrency\n"
            << "  --write-all              Save all processed clips instead of representatives\n"
            << "  --help                   Show this help\n";
}

size_t parseSize(const std::string &value, const std::string &name) {
    char *end = nullptr;
    errno = 0;
    const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
    if (errno != 0 || end == value.c_str() || *end != '\0' || parsed == 0) {
        throw std::runtime_error("Invalid " + name + ": " + value);
    }
    return static_cast<size_t>(parsed);
}

size_t parseNonNegativeSize(const std::string &value, const std::string &name) {
    char *end = nullptr;
    errno = 0;
    const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
    if (errno != 0 || end == value.c_str() || *end != '\0') {
        throw std::runtime_error("Invalid " + name + ": " + value);
    }
    return static_cast<size_t>(parsed);
}

void parseSampleFrames(const std::string &value, std::vector<size_t> &sampleFrames) {
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        if (!item.empty()) {
            sampleFrames.push_back(parseSize(item, "--sample-frames"));
        }
    }
}

Options parseArgs(int argc, char **argv) {
    Options options;
    bool sawSampleFrames = false;

    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        auto requireValue = [&](const std::string &option) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(option + " requires a value");
            }
            return argv[++i];
        };

        if (arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--data-dir") {
            options.dataDir = requireValue(arg);
        } else if (arg == "--output-dir") {
            options.outputDir = requireValue(arg);
        } else if (arg == "--limit") {
            options.limit = parseSize(requireValue(arg), arg);
        } else if (arg == "--sample-frames") {
            if (!sawSampleFrames) {
                options.sampleFrames.clear();
                sawSampleFrames = true;
            }
            parseSampleFrames(requireValue(arg), options.sampleFrames);
        } else if (arg == "--retroactive-vad") {
            options.retroactiveVadBlocks = static_cast<uint32_t>(parseSize(requireValue(arg), arg));
        } else if (arg == "--ignore-start-ms") {
            options.ignoreStartMs = static_cast<uint32_t>(parseNonNegativeSize(requireValue(arg), arg));
        } else if (arg == "--max-lag-ms") {
            options.maxLagMs = static_cast<uint32_t>(parseSize(requireValue(arg), arg));
        } else if (arg == "--lag-search-step") {
            options.lagSearchStep = parseSize(requireValue(arg), arg);
        } else if (arg == "--jobs") {
            options.jobs = parseSize(requireValue(arg), arg);
        } else if (arg == "--write-all") {
            options.writeAll = true;
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    if (options.sampleFrames.empty()) {
        throw std::runtime_error("At least one --sample-frames value is required");
    }
    if (options.dataDir.empty()) {
        throw std::runtime_error("--data-dir is required");
    }
    if (options.outputDir.empty()) {
        throw std::runtime_error("--output-dir is required");
    }
    std::sort(options.sampleFrames.begin(), options.sampleFrames.end());
    options.sampleFrames.erase(std::unique(options.sampleFrames.begin(), options.sampleFrames.end()),
                               options.sampleFrames.end());
    return options;
}

void extractZip(const std::string &zipPath, const std::string &destination) {
    if (!fileExists(zipPath)) {
        throw std::runtime_error("Missing dataset archive: " + zipPath);
    }

    makeDirs(destination);
    const std::string command = "unzip -q -n " + shellQuote(zipPath) + " -d " + shellQuote(destination);
    const int result = std::system(command.c_str());
    if (result != 0) {
        throw std::runtime_error("Failed to extract " + zipPath + " using unzip");
    }
}

std::pair<std::string, std::string> prepareDataset(const Options &options) {
    const std::string cleanDir = joinPath(options.dataDir, "clean_testset_wav");
    const std::string noisyDir = joinPath(options.dataDir, "noisyreverb_testset_wav");
    if (dirExists(cleanDir) && dirExists(noisyDir)) {
        return std::make_pair(cleanDir, noisyDir);
    }

    const std::string cacheDir = joinPath(options.outputDir, "_extracted");
    const std::string cachedCleanDir = joinPath(cacheDir, "clean_testset_wav");
    const std::string cachedNoisyDir = joinPath(cacheDir, "noisyreverb_testset_wav");
    if (!dirExists(cachedCleanDir)) {
        extractZip(joinPath(options.dataDir, "clean_testset_wav.zip"), cacheDir);
    }
    if (!dirExists(cachedNoisyDir)) {
        extractZip(joinPath(options.dataDir, "noisyreverb_testset_wav.zip"), cacheDir);
    }
    return std::make_pair(cachedCleanDir, cachedNoisyDir);
}

std::vector<std::string> matchingClipNames(const std::string &cleanDir, const std::string &noisyDir, size_t limit) {
    std::vector<std::string> result;
    for (const std::string &name: listWavNames(cleanDir)) {
        if (fileExists(joinPath(noisyDir, name))) {
            result.push_back(name);
            if (result.size() >= limit) {
                break;
            }
        }
    }
    if (result.empty()) {
        throw std::runtime_error("No matching WAV files found in " + cleanDir + " and " + noisyDir);
    }
    return result;
}

std::vector<float> processAudio(const std::vector<float> &input, size_t sampleFrames,
                                float vadThreshold, uint32_t vadGracePeriodBlocks,
                                uint32_t retroactiveVADGraceBlocks, RnNoiseStats *stats) {
    RnNoiseCommonPlugin plugin(1);
    plugin.init();

    std::vector<float> padded = input;
    padded.resize(((padded.size() + sampleFrames - 1) / sampleFrames) * sampleFrames, 0.0f);

    const size_t flushCalls = retroactiveVADGraceBlocks + 4u;
    std::vector<float> output(padded.size() + flushCalls * sampleFrames, 0.0f);
    std::vector<float> blockOut(sampleFrames, 0.0f);
    size_t outputOffset = 0;

    for (size_t pos = 0; pos < padded.size(); pos += sampleFrames) {
        const float *inputs[] = {padded.data() + pos};
        float *outputs[] = {blockOut.data()};
        plugin.process(inputs, outputs, sampleFrames, vadThreshold, vadGracePeriodBlocks,
                       retroactiveVADGraceBlocks, 0.0f);
        std::copy(blockOut.begin(), blockOut.end(), output.begin() + outputOffset);
        outputOffset += sampleFrames;
    }

    const std::vector<float> zeros(sampleFrames, 0.0f);
    for (size_t flush = 0; flush < flushCalls; flush++) {
        const float *inputs[] = {zeros.data()};
        float *outputs[] = {blockOut.data()};
        plugin.process(inputs, outputs, sampleFrames, vadThreshold, vadGracePeriodBlocks,
                       retroactiveVADGraceBlocks, 0.0f);
        std::copy(blockOut.begin(), blockOut.end(), output.begin() + outputOffset);
        outputOffset += sampleFrames;
    }

    if (stats != nullptr) {
        *stats = plugin.getStats();
    }
    return output;
}

size_t ignoreStartFrames(uint32_t ignoreStartMs) {
    return static_cast<size_t>((static_cast<uint64_t>(ignoreStartMs) * kExpectedSampleRate) / 1000u);
}

size_t lagFrames(uint32_t lagMs) {
    return static_cast<size_t>((static_cast<uint64_t>(lagMs) * kExpectedSampleRate) / 1000u);
}

double snrAtLag(const std::vector<float> &reference, const std::vector<float> &candidate, size_t lag,
                size_t referenceStart, size_t sampleStride) {
    if (referenceStart >= reference.size() || referenceStart + lag >= candidate.size()) {
        return -std::numeric_limits<double>::infinity();
    }

    const size_t count = std::min(reference.size() - referenceStart, candidate.size() - referenceStart - lag);
    double signal = 0.0;
    double error = 0.0;
    for (size_t i = 0; i < count; i += sampleStride) {
        const float referenceSample = reference[referenceStart + i];
        signal += static_cast<double>(referenceSample) * referenceSample;
        const double diff = static_cast<double>(referenceSample) - candidate[referenceStart + lag + i];
        error += diff * diff;
    }
    return 10.0 * std::log10((signal + 1e-20) / (error + 1e-20));
}

std::pair<double, size_t> bestAlignedSnr(const std::vector<float> &reference, const std::vector<float> &candidate,
                                         size_t maxLag, size_t referenceStart, size_t coarseStep) {
    coarseStep = std::max<size_t>(1, coarseStep);

    std::vector<std::pair<double, size_t>> candidates;
    candidates.reserve((maxLag / coarseStep) + 2);
    for (size_t lag = 0; lag <= maxLag; lag += coarseStep) {
        candidates.emplace_back(snrAtLag(reference, candidate, lag, referenceStart, coarseStep), lag);
    }
    if (maxLag % coarseStep != 0) {
        candidates.emplace_back(snrAtLag(reference, candidate, maxLag, referenceStart, coarseStep), maxLag);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const std::pair<double, size_t> &lhs, const std::pair<double, size_t> &rhs) {
                  return lhs.first > rhs.first;
              });

    double bestSnr = -std::numeric_limits<double>::infinity();
    size_t bestLag = 0;
    std::vector<bool> searched(maxLag + 1, false);
    const size_t candidatesToRefine = std::min<size_t>(6, candidates.size());
    const size_t refinementStride = std::max<size_t>(1, coarseStep / 4);
    for (size_t i = 0; i < candidatesToRefine; i++) {
        const size_t center = candidates[i].second;
        const size_t begin = center > coarseStep ? center - coarseStep : 0;
        const size_t end = std::min(maxLag, center + coarseStep);
        for (size_t lag = begin; lag <= end; lag++) {
            if (searched[lag]) {
                continue;
            }
            searched[lag] = true;
            const double snr = snrAtLag(reference, candidate, lag, referenceStart, refinementStride);
            if (snr > bestSnr) {
                bestSnr = snr;
                bestLag = lag;
            }
        }
    }

    return std::make_pair(snrAtLag(reference, candidate, bestLag, referenceStart, 1), bestLag);
}

double rms(const std::vector<float> &samples) {
    double energy = 0.0;
    for (float sample: samples) {
        energy += static_cast<double>(sample) * sample;
    }
    return std::sqrt(energy / std::max<size_t>(1, samples.size()));
}

double peak(const std::vector<float> &samples) {
    double result = 0.0;
    for (float sample: samples) {
        result = std::max(result, std::abs(static_cast<double>(sample)));
    }
    return result;
}

void requireFinite(const std::vector<float> &samples, const std::string &name) {
    for (size_t i = 0; i < samples.size(); i++) {
        if (!std::isfinite(samples[i])) {
            std::ostringstream message;
            message << "Non-finite output sample in " << name << " at " << i;
            throw std::runtime_error(message.str());
        }
    }
}

double quietRegionAttenuation(const std::vector<float> &clean, const std::vector<float> &noisy,
                              const std::vector<float> &processed, size_t lag, size_t referenceStart) {
    const size_t count = std::min(clean.size(), std::min(noisy.size(), processed.size() > lag ? processed.size() - lag : 0));
    double noisyEnergy = 0.0;
    double processedEnergy = 0.0;
    size_t quietFrames = 0;

    const size_t firstBlock = ((referenceStart + kDenoiseBlockSize - 1) / kDenoiseBlockSize) * kDenoiseBlockSize;
    for (size_t start = firstBlock; start + kDenoiseBlockSize <= count; start += kDenoiseBlockSize) {
        double cleanEnergy = 0.0;
        for (size_t i = 0; i < kDenoiseBlockSize; i++) {
            cleanEnergy += static_cast<double>(clean[start + i]) * clean[start + i];
        }

        const double cleanRms = std::sqrt(cleanEnergy / kDenoiseBlockSize);
        if (cleanRms < 0.003) {
            for (size_t i = 0; i < kDenoiseBlockSize; i++) {
                noisyEnergy += static_cast<double>(noisy[start + i]) * noisy[start + i];
                processedEnergy += static_cast<double>(processed[start + lag + i]) * processed[start + lag + i];
            }
            quietFrames += kDenoiseBlockSize;
        }
    }

    if (quietFrames == 0) {
        return 0.0;
    }
    return 10.0 * std::log10((noisyEnergy + 1e-20) / (processedEnergy + 1e-20));
}

ClipMetrics evaluateClip(const std::string &name, size_t sampleFrames,
                         const std::vector<float> &clean, const std::vector<float> &noisy,
                         size_t ignoredStartFrames, size_t maxLag, size_t lagSearchStep,
                         std::vector<float> *processedOut) {
    std::vector<float> processed = processAudio(noisy, sampleFrames, 0.0f, 20, 0, nullptr);
    requireFinite(processed, name);

    const double inputSnr = snrAtLag(clean, noisy, 0, ignoredStartFrames, 1);
    const std::pair<double, size_t> aligned = bestAlignedSnr(clean, processed, maxLag, ignoredStartFrames,
                                                            lagSearchStep);

    ClipMetrics metrics;
    metrics.name = name;
    metrics.sampleFrames = sampleFrames;
    metrics.inputSnr = inputSnr;
    metrics.processedSnr = aligned.first;
    metrics.snrLift = aligned.first - inputSnr;
    metrics.quietAttenuation = quietRegionAttenuation(clean, noisy, processed, aligned.second, ignoredStartFrames);
    metrics.outputRms = rms(processed);
    metrics.outputPeak = peak(processed);
    metrics.bestLag = aligned.second;
    metrics.maxLag = maxLag;
    metrics.bestLagNearLimit = aligned.second + lagSearchStep >= maxLag;

    if (processedOut != nullptr) {
        *processedOut = std::move(processed);
    }
    return metrics;
}

bool shouldWriteClip(size_t clipIndex, size_t sampleFrames, const Options &options) {
    if (options.writeAll) {
        return true;
    }
    return clipIndex < 3 && (sampleFrames == 480 || sampleFrames == options.sampleFrames.front());
}

void printClipMetrics(const ClipMetrics &metrics) {
    std::cout << std::fixed << std::setprecision(3)
              << metrics.name
              << " frames=" << metrics.sampleFrames
              << " inputSNR=" << metrics.inputSnr
              << " processedSNR=" << metrics.processedSnr
              << " lift=" << metrics.snrLift
              << " quietAtt=" << metrics.quietAttenuation
              << " rms=" << metrics.outputRms
              << " peak=" << metrics.outputPeak
              << " lag=" << metrics.bestLag
              << "/" << metrics.maxLag
              << (metrics.bestLagNearLimit ? " lag-limit-warning" : "")
              << "\n";
}

void assertSummary(const Summary &summary) {
    if (summary.clips < kMinimumAggregateClipCount) {
        std::cout << "NOTE frames=" << summary.sampleFrames
                  << " skipping aggregate thresholds for " << summary.clips
                  << " clips; use at least " << kMinimumAggregateClipCount
                  << " clips for pass/fail checks\n";
        return;
    }

    if (summary.averageSnrLift < 0.75) {
        throw std::runtime_error("Average SNR lift below threshold for sampleFrames="
                                 + std::to_string(summary.sampleFrames));
    }
    if (summary.minimumSnrLift < -0.25) {
        throw std::runtime_error("A clip regressed below SNR threshold for sampleFrames="
                                 + std::to_string(summary.sampleFrames));
    }
    if (summary.averageQuietAttenuation < 1.5) {
        throw std::runtime_error("Average quiet-region attenuation below threshold for sampleFrames="
                                 + std::to_string(summary.sampleFrames));
    }
}

std::map<size_t, Summary> runQualityEvaluation(const Options &options,
                                               const std::string &cleanDir,
                                               const std::string &noisyDir,
                                               const std::vector<std::string> &names) {
    std::map<size_t, Summary> summaries;
    const size_t ignoredStartFrames = ignoreStartFrames(options.ignoreStartMs);
    const size_t maxLag = lagFrames(options.maxLagMs);

    for (size_t sampleFrames: options.sampleFrames) {
        Summary summary;
        summary.sampleFrames = sampleFrames;
        std::vector<TaskResult> results(names.size());
        size_t nextClip = 0;
        std::mutex nextClipMutex;

        const size_t workerCount = std::min(options.jobs, names.size());
        std::vector<std::thread> workers;
        workers.reserve(workerCount);
        for (size_t workerIdx = 0; workerIdx < workerCount; workerIdx++) {
            workers.emplace_back([&, sampleFrames]() {
                while (true) {
                    size_t clipIndex = 0;
                    {
                        std::lock_guard<std::mutex> lock(nextClipMutex);
                        if (nextClip >= names.size()) {
                            return;
                        }
                        clipIndex = nextClip++;
                    }

                    try {
                        const std::string &name = names[clipIndex];
                        const Audio clean = readWavMono16(joinPath(cleanDir, name));
                        const Audio noisy = readWavMono16(joinPath(noisyDir, name));
                        if (clean.samples.size() != noisy.samples.size()) {
                            throw std::runtime_error("Mismatched clean/noisy length for " + name);
                        }

                        std::vector<float> processed;
                        const bool writeClip = shouldWriteClip(clipIndex, sampleFrames, options);
                        results[clipIndex].metrics = evaluateClip(name, sampleFrames, clean.samples, noisy.samples,
                                                                  ignoredStartFrames, maxLag,
                                                                  options.lagSearchStep,
                                                                  writeClip ? &processed : nullptr);

                        if (writeClip) {
                            writeWavMono16(joinPath(options.outputDir,
                                                    withoutExtension(name) + "_frames"
                                                    + std::to_string(sampleFrames) + "_processed.wav"),
                                           processed);
                            writeWavMono16(joinPath(options.outputDir, withoutExtension(name) + "_clean.wav"),
                                           clean.samples);
                            writeWavMono16(joinPath(options.outputDir, withoutExtension(name) + "_noisy.wav"),
                                           noisy.samples);
                        }
                    } catch (const std::exception &ex) {
                        results[clipIndex].error = ex.what();
                    }
                }
            });
        }

        for (std::thread &worker: workers) {
            worker.join();
        }

        for (size_t clipIndex = 0; clipIndex < names.size(); clipIndex++) {
            if (!results[clipIndex].error.empty()) {
                throw std::runtime_error(results[clipIndex].error);
            }

            const ClipMetrics &metrics = results[clipIndex].metrics;
            printClipMetrics(metrics);

            summary.clips++;
            summary.averageSnrLift += metrics.snrLift;
            summary.minimumSnrLift = std::min(summary.minimumSnrLift, metrics.snrLift);
            summary.averageQuietAttenuation += metrics.quietAttenuation;
        }

        summary.averageSnrLift /= static_cast<double>(summary.clips);
        summary.averageQuietAttenuation /= static_cast<double>(summary.clips);
        summaries[sampleFrames] = summary;

        std::cout << std::fixed << std::setprecision(3)
                  << "SUMMARY frames=" << sampleFrames
                  << " clips=" << summary.clips
                  << " avgLift=" << summary.averageSnrLift
                  << " minLift=" << summary.minimumSnrLift
                  << " avgQuietAtt=" << summary.averageQuietAttenuation
                  << "\n";
        assertSummary(summary);
    }

    const auto baseline = summaries.find(480);
    if (baseline != summaries.end() && baseline->second.clips >= kMinimumAggregateClipCount) {
        for (const auto &entry: summaries) {
            if (entry.first == 480) {
                continue;
            }
            if (entry.second.clips < kMinimumAggregateClipCount) {
                continue;
            }
            const double diff = std::abs(entry.second.averageSnrLift - baseline->second.averageSnrLift);
            if (diff > 0.75) {
                throw std::runtime_error("Average SNR lift for sampleFrames=" + std::to_string(entry.first)
                                         + " differs from 480-frame baseline by more than 0.75 dB");
            }
        }
    }

    return summaries;
}

void runRetroactiveVadCheck(const Options &options,
                            const std::string &noisyDir,
                            const std::vector<std::string> &names) {
    const std::vector<std::string> preferred = {"p232_001.wav", "p232_002.wav", "p232_003.wav"};
    std::vector<std::string> retroNames;
    for (const std::string &name: preferred) {
        if (fileExists(joinPath(noisyDir, name))) {
            retroNames.push_back(name);
        }
    }
    for (const std::string &name: names) {
        if (retroNames.size() >= 3) {
            break;
        }
        if (std::find(retroNames.begin(), retroNames.end(), name) == retroNames.end()) {
            retroNames.push_back(name);
        }
    }

    for (const std::string &name: retroNames) {
        const Audio noisy = readWavMono16(joinPath(noisyDir, name));
        for (size_t sampleFrames: options.sampleFrames) {
            RnNoiseStats stats {};
            processAudio(noisy.samples, sampleFrames, 0.85f, 0, options.retroactiveVadBlocks, &stats);
            std::cout << "RETRO "
                      << name
                      << " frames=" << sampleFrames
                      << " retroBlocks=" << stats.retroactiveVADGraceBlocks
                      << " vadGraceBlocks=" << stats.vadGraceBlocks
                      << " waiting=" << stats.blocksWaitingForOutput
                      << " zeroedFrames=" << stats.outputFramesForcedToBeZeroed
                      << "\n";

            if (stats.retroactiveVADGraceBlocks == 0) {
                throw std::runtime_error("Retroactive VAD did not unmute any blocks for " + name
                                         + " sampleFrames=" + std::to_string(sampleFrames));
            }
            if (stats.blocksWaitingForOutput > options.retroactiveVadBlocks + 1u) {
                throw std::runtime_error("Retroactive VAD left too much buffered output for " + name
                                         + " sampleFrames=" + std::to_string(sampleFrames));
            }
        }
    }
}

int run(int argc, char **argv) {
    const Options options = parseArgs(argc, argv);
    makeDirs(options.outputDir);

    const std::pair<std::string, std::string> dataset = prepareDataset(options);
    const std::vector<std::string> names = matchingClipNames(dataset.first, dataset.second, options.limit);

    std::cout << "Data: clean=" << dataset.first << " noisy=" << dataset.second << "\n";
    std::cout << "Output: " << options.outputDir << "\n";
    std::cout << "Clips: " << names.size() << "\n";
    std::cout << "Jobs: " << options.jobs << "\n";
    std::cout << "Ignoring startup: " << options.ignoreStartMs << " ms\n";
    std::cout << "Alignment max lag: " << options.maxLagMs << " ms, coarse step: "
              << options.lagSearchStep << " frames\n";

    runQualityEvaluation(options, dataset.first, dataset.second, names);
    runRetroactiveVadCheck(options, dataset.second, names);

    std::cout << "Audio evaluation passed\n";
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception &ex) {
        std::cerr << "rnnoise_audio_eval: " << ex.what() << "\n";
        return 1;
    }
}
