/*
 * Drift Engine - Granular Sample Explorer for distingNT
 *
 * "Build ambient worlds from frozen moments"
 *
 * 4 autonomous drifters wander through a sample, each in its own
 * frequency band and stereo position. Poisson-triggered grains
 * create organic, evolving ambient worlds.
 *
 * Developer: Ns (nealsanche)
 * Plugin ID: Dr (Drift)
 * GUID: NsDr
 */

#include <distingnt/api.h>
#include <distingnt/wav.h>
#include <math.h>
#include <new>
#include <cstring>

// M_PI may not be defined in all environments
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ============================================================================
// CONSTANTS
// ============================================================================

static constexpr int kNumDrifters = 4;
static constexpr int kMaxGrainsPerDrifter = 8;
static constexpr int kMaxTotalGrains = kNumDrifters * kMaxGrainsPerDrifter;
static constexpr int kMaxSampleFrames = 48000 * 32;  // 32 seconds at 48kHz

// Reverb constants
static constexpr int kReverbDelayLines = 8;
static constexpr int kReverbMaxDelay = 4800;  // 100ms at 48kHz

// Filter bank center frequencies (Hz) - lowest at 250Hz to avoid granular artifacts
static constexpr float kBandCenterFreqs[kNumDrifters] = { 250.0f, 750.0f, 1550.0f, 4000.0f };
// Stereo positions for each drifter (-1 to +1)
static constexpr float kDrifterPan[kNumDrifters] = { -1.0f, -0.5f, 0.5f, 1.0f };
// Clock phase offsets for each drifter (used for quantized clock mode)
// static constexpr float kClockPhaseOffset[kNumDrifters] = { 0.0f, 0.25f, 0.5f, 0.75f };

// ============================================================================
// GRAIN ENVELOPE SHAPES
// ============================================================================

enum GrainShape {
    kShapeMist = 0,    // Soft gaussian
    kShapeCloud,       // Tukey window
    kShapeRain,        // Triangle
    kShapeHail,        // Sharp attack, soft decay
    kShapeIce,         // Square-ish
    kNumShapes
};

static const char* const shapeNames[] = {
    "Mist",
    "Cloud",
    "Rain",
    "Hail",
    "Ice",
    NULL
};

// ============================================================================
// DATA STRUCTURES
// ============================================================================

// Simple 2-pole state variable filter for each drifter
struct BandFilter {
    float lowpass;
    float bandpass;
    float highpass;

    void reset() { lowpass = bandpass = highpass = 0; }

    // Flush denormals to zero
    static inline float flushDenormal(float x) {
        return (fabsf(x) < 1e-20f) ? 0.0f : x;
    }

    float process(float input, float freq, float q, float sr) {
        // Clamp frequency coefficient for stability
        float f = 2.0f * sinf(M_PI * fminf(freq, sr * 0.4f) / sr);
        f = fminf(f, 0.7f);  // Conservative stability limit

        // Clamp Q to prevent instability
        q = fminf(q, 0.95f);

        lowpass += f * bandpass;
        highpass = input - lowpass - q * bandpass;
        bandpass += f * highpass;

        // Prevent denormals and NaN
        lowpass = flushDenormal(lowpass);
        bandpass = flushDenormal(bandpass);
        highpass = flushDenormal(highpass);

        // NaN protection
        if (lowpass != lowpass) lowpass = 0;
        if (bandpass != bandpass) bandpass = 0;
        if (highpass != highpass) highpass = 0;

        return bandpass;  // Use bandpass for spectral separation
    }
};

// Single grain instance
struct Grain {
    bool active;
    float position;        // Playback position in samples
    float positionDelta;   // Playback rate (pitch)
    float phase;           // Envelope phase 0-1
    float phaseDelta;      // Envelope rate
    int drifterIndex;      // Which drifter spawned this grain
    GrainShape shape;      // Envelope shape
    float amplitude;       // Grain amplitude
    BandFilter filterL;    // Per-grain stereo filter
    BandFilter filterR;
};

// Drifter state
struct Drifter {
    float position;        // Current position in sample (0-1)
    float velocity;        // Current drift velocity
    float pitchOffset;     // Pitch offset in semitones
    float timeSinceGrain;  // Time since last grain trigger
    float nextGrainTime;   // Time until next grain (Poisson)
    float variation;       // Per-drifter speed variation (0.5-1.0), set once
    float driftDirection;  // -1 or +1, set once at init
};

// Reverb delay line
struct ReverbLine {
    float buffer[kReverbMaxDelay];
    int writePos;
    int delaySamples;
    float feedback;

    void init(int delay, float fb) {
        memset(buffer, 0, sizeof(buffer));
        writePos = 0;
        delaySamples = delay;
        feedback = fb;
    }

    float process(float input) {
        int readPos = (writePos - delaySamples + kReverbMaxDelay) % kReverbMaxDelay;
        float output = buffer[readPos];
        buffer[writePos] = input + output * feedback;
        writePos = (writePos + 1) % kReverbMaxDelay;
        return output;
    }
};

// Simple reverb
struct Reverb {
    ReverbLine lines[kReverbDelayLines];
    float damping;
    float dampState;

    void init(float sr) {
        // Prime number delay times for diffusion
        int delays[] = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };
        float fb = 0.84f;
        for (int i = 0; i < kReverbDelayLines; i++) {
            int d = (int)(delays[i] * sr / 48000.0f);
            lines[i].init(d, fb);
        }
        damping = 0.3f;
        dampState = 0;
    }

    float process(float input, float decay) {
        float sum = 0;
        for (int i = 0; i < kReverbDelayLines; i++) {
            lines[i].feedback = 0.7f + decay * 0.25f;
            sum += lines[i].process(input);
        }
        dampState += damping * (sum - dampState);
        return dampState / kReverbDelayLines;
    }
};

// DTC - Performance critical data
struct _driftEngine_DTC {
    Drifter drifters[kNumDrifters];
    Grain grains[kMaxTotalGrains];

    // Smoothed parameter values
    float anchorSmooth;
    float driftSmooth;
    float densitySmooth;
    float entropySmooth;
    float stormLevel;      // Current storm intensity (decays)

    // Clock sync state
    float clockPhase;
    float clockPeriod;
    bool clockReceived;
    float prevClock;

    // Output for CV
    float averagePosition;
    bool pulseOut;

    // Smoothed normalization factor (anti-click)
    float smoothNorm;

    // Random state
    uint32_t randState;
};

// DRAM - Large sample buffer
struct _driftEngine_DRAM {
    float sampleBufferL[kMaxSampleFrames];
    float sampleBufferR[kMaxSampleFrames];
    int32_t sampleLength;      // Current sample length in frames
    bool sampleLoaded;
    bool sampleIsStereo;

    Reverb reverb;
};

// ============================================================================
// PARAMETERS
// ============================================================================

enum {
    // Audio I/O
    kParamOutputL,
    kParamOutputLMode,
    kParamOutputR,

    // CV Inputs
    kParamCvAnchor,
    kParamCvPitch,
    kParamCvDrift,
    kParamCvEntropy,
    kParamCvStorm,
    kParamCvClock,

    // CV Outputs (simulated via audio bus for position/pulse)
    kParamCvOutPosition,
    kParamCvOutPulse,

    // Sample selection (folder + sample within folder)
    kParamFolder,
    kParamSample,

    // Position controls
    kParamAnchor,
    kParamWander,
    kParamGravity,
    kParamDrift,

    // Density & timing
    kParamDensity,
    kParamDeviation,

    // Pitch controls
    kParamPitch,
    kParamScatter,

    // Spectral controls
    kParamSpectrum,
    kParamTilt,

    // Character
    kParamShape,
    kParamFog,
    kParamEntropy,

    kNumParameters
};

static const _NT_parameter parameters[] = {
    // Audio outputs
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Out L", 1, 13)
    NT_PARAMETER_AUDIO_OUTPUT("Out R", 1, 14)

    // CV inputs
    NT_PARAMETER_CV_INPUT("Anchor CV", 0, 0)
    NT_PARAMETER_CV_INPUT("Pitch CV", 0, 0)
    NT_PARAMETER_CV_INPUT("Drift CV", 0, 0)
    NT_PARAMETER_CV_INPUT("Entropy CV", 0, 0)
    NT_PARAMETER_CV_INPUT("Storm Gate", 0, 0)
    NT_PARAMETER_CV_INPUT("Clock", 0, 0)

    // CV outputs
    NT_PARAMETER_CV_OUTPUT("Position", 1, 1)
    NT_PARAMETER_CV_OUTPUT("Pulse", 1, 2)

    // Sample selection - max values updated dynamically when SD card mounts
    { .name = "Folder", .min = 0, .max = 32767, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL },
    { .name = "Sample", .min = 0, .max = 32767, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL },

    // Position controls
    { .name = "Anchor", .min = 0, .max = 100, .def = 50, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL },
    { .name = "Wander", .min = 0, .max = 100, .def = 30, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL },
    { .name = "Gravity", .min = -100, .max = 100, .def = 0, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL },
    { .name = "Drift", .min = 0, .max = 100, .def = 30, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL },

    // Density
    { .name = "Density", .min = 0, .max = 100, .def = 50, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL },
    { .name = "Deviation", .min = 0, .max = 100, .def = 100, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL },

    // Pitch
    { .name = "Pitch", .min = -24, .max = 24, .def = 0, .unit = kNT_unitSemitones, .scaling = 0, .enumStrings = NULL },
    { .name = "Scatter", .min = 0, .max = 12, .def = 0, .unit = kNT_unitSemitones, .scaling = 0, .enumStrings = NULL },

    // Spectral
    { .name = "Spectrum", .min = 0, .max = 100, .def = 0, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL },
    { .name = "Tilt", .min = -100, .max = 100, .def = 0, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL },

    // Character
    { .name = "Shape", .min = 0, .max = kNumShapes - 1, .def = 1, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = shapeNames },
    { .name = "Fog", .min = 0, .max = 100, .def = 30, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL },
    { .name = "Entropy", .min = 0, .max = 100, .def = 25, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL },
};

// ============================================================================
// PARAMETER PAGES
// ============================================================================

static const uint8_t pageSample[] = { kParamFolder, kParamSample };
static const uint8_t pagePosition[] = { kParamAnchor, kParamWander, kParamGravity, kParamDrift };
static const uint8_t pageDensity[] = { kParamDensity, kParamDeviation };
static const uint8_t pagePitch[] = { kParamPitch, kParamScatter };
static const uint8_t pageSpectral[] = { kParamSpectrum, kParamTilt };
static const uint8_t pageCharacter[] = { kParamShape, kParamFog, kParamEntropy };
static const uint8_t pageRouting[] = { kParamOutputL, kParamOutputLMode, kParamOutputR };
static const uint8_t pageCvIn[] = { kParamCvAnchor, kParamCvPitch, kParamCvDrift, kParamCvEntropy, kParamCvStorm, kParamCvClock };
static const uint8_t pageCvOut[] = { kParamCvOutPosition, kParamCvOutPulse };

static const _NT_parameterPage pages[] = {
    { .name = "Sample", .numParams = ARRAY_SIZE(pageSample), .params = pageSample },
    { .name = "Position", .numParams = ARRAY_SIZE(pagePosition), .params = pagePosition },
    { .name = "Density", .numParams = ARRAY_SIZE(pageDensity), .params = pageDensity },
    { .name = "Pitch", .numParams = ARRAY_SIZE(pagePitch), .params = pagePitch },
    { .name = "Spectral", .numParams = ARRAY_SIZE(pageSpectral), .params = pageSpectral },
    { .name = "Character", .numParams = ARRAY_SIZE(pageCharacter), .params = pageCharacter },
    { .name = "Outputs", .numParams = ARRAY_SIZE(pageRouting), .params = pageRouting },
    { .name = "CV In", .numParams = ARRAY_SIZE(pageCvIn), .params = pageCvIn },
    { .name = "CV Out", .numParams = ARRAY_SIZE(pageCvOut), .params = pageCvOut },
};

static const _NT_parameterPages parameterPages = {
    .numPages = ARRAY_SIZE(pages),
    .pages = pages,
};

// ============================================================================
// ALGORITHM STRUCTURE
// ============================================================================

// Forward declaration for callback
struct _driftEngineAlgorithm;
static void wavLoadCallback(void* callbackData, bool success);

// Main algorithm structure (like sample player example)
struct _driftEngineAlgorithm : public _NT_algorithm {
    _driftEngineAlgorithm(_driftEngine_DTC* dtc_, _driftEngine_DRAM* dram_)
        : dtc(dtc_), dram(dram_) {}
    ~_driftEngineAlgorithm() {}

    _driftEngine_DTC* dtc;
    _driftEngine_DRAM* dram;

    // Mutable copy of parameters (for dynamic max values like sample player example)
    _NT_parameter params[kNumParameters];

    // WAV loading state
    _NT_wavRequest wavRequest;
    bool cardMounted;
    bool awaitingCallback;
    bool initialized;          // Set after construct completes
    bool pendingSampleLoad;    // Deferred sample load request
    int32_t pendingSampleLength; // Length of sample being loaded
    float pendingSampleRateRatio; // Sample rate ratio of sample being loaded
    float sampleRateRatio;     // Source sample rate / device sample rate

    // Cached values from parameters
    float anchorTarget;
    float wanderAmount;
    float gravityForce;
    float driftSpeed;
    float densityRate;     // Grains per second
    float grainSize;       // Grain duration in samples
    float pitchOffset;     // Master pitch in semitones
    float scatterAmount;   // Pitch scatter
    float spectrumSep;     // Filter bank separation
    float tiltAmount;      // Spectral tilt
    GrainShape grainShape;
    float fogAmount;       // Reverb send
    float entropyTarget;
    float deviationAmount; // Clock deviation
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Fast pseudo-random number generator (Xorshift32)
static inline uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

// Random float 0-1
static inline float randFloat(_driftEngine_DTC* dtc) {
    return (float)xorshift32(&dtc->randState) / (float)0xFFFFFFFF;
}

// Find nearest zero crossing in sample buffer
static int findNearestZeroCrossing(const float* buffer, int startPos, int sampleLen, int searchRadius = 64) {
    // Guard against division by zero
    if (sampleLen <= 0) return 0;

    int bestPos = startPos % sampleLen;
    float bestVal = fabsf(buffer[bestPos]);

    for (int offset = 1; offset <= searchRadius; offset++) {
        // Search forward
        int posF = (startPos + offset) % sampleLen;
        float valF = fabsf(buffer[posF]);
        if (valF < bestVal) {
            bestVal = valF;
            bestPos = posF;
        }
        // Also check for actual zero crossing (sign change)
        if (offset > 0) {
            int prevF = (startPos + offset - 1) % sampleLen;
            if (buffer[prevF] * buffer[posF] < 0) {
                // Zero crossing found
                return (fabsf(buffer[prevF]) < fabsf(buffer[posF])) ? prevF : posF;
            }
        }

        // Search backward
        int posB = (startPos - offset + sampleLen) % sampleLen;
        float valB = fabsf(buffer[posB]);
        if (valB < bestVal) {
            bestVal = valB;
            bestPos = posB;
        }
        if (offset > 0) {
            int prevB = (startPos - offset + 1 + sampleLen) % sampleLen;
            if (buffer[prevB] * buffer[posB] < 0) {
                return (fabsf(buffer[prevB]) < fabsf(buffer[posB])) ? prevB : posB;
            }
        }
    }

    return bestPos;
}

// Random float -1 to +1
static inline float randFloatBipolar(_driftEngine_DTC* dtc) {
    return randFloat(dtc) * 2.0f - 1.0f;
}

// Exponential distribution for Poisson process
static inline float randExponential(_driftEngine_DTC* dtc, float lambda) {
    float u = randFloat(dtc);
    if (u < 0.0001f) u = 0.0001f;  // Avoid log(0)
    return -logf(u) / lambda;
}

// Compute grain envelope value
static float grainEnvelope(float phase, GrainShape shape) {
    if (phase < 0 || phase > 1) return 0;

    // Universal safety fade at boundaries (3% fade-in/out for click reduction)
    float fade = 1.0f;
    const float fadeLen = 0.03f;
    if (phase < fadeLen) fade = phase / fadeLen;
    else if (phase > 1.0f - fadeLen) fade = (1.0f - phase) / fadeLen;

    float env = 1.0f;
    switch (shape) {
        case kShapeMist: {
            // Gaussian-ish: sin^2 approximation
            float x = phase * M_PI;
            float s = sinf(x);
            env = s * s;
            break;
        }
        case kShapeCloud: {
            // Tukey window (tapered cosine)
            float alpha = 0.5f;
            if (phase < alpha/2) {
                env = 0.5f * (1 - cosf(2*M_PI*phase/alpha));
            } else if (phase > 1 - alpha/2) {
                env = 0.5f * (1 - cosf(2*M_PI*(1-phase)/alpha));
            } else {
                env = 1.0f;
            }
            break;
        }
        case kShapeRain:
            // Triangle
            env = phase < 0.5f ? phase * 2 : (1 - phase) * 2;
            break;

        case kShapeHail: {
            // Sharp attack, exponential decay
            if (phase < 0.1f) env = phase * 10;
            else env = expf(-4.0f * (phase - 0.1f));
            break;
        }

        case kShapeIce:
            // Near-square with tiny fade
            if (phase < 0.02f) env = phase * 50;
            else if (phase > 0.98f) env = (1 - phase) * 50;
            else env = 1.0f;
            break;

        default:
            env = 1.0f;
            break;
    }

    return env * fade;
}

// Map density (0-100) to grains per second
static float densityToRate(float density) {
    // 0% -> 0.25 grains/sec (sparse!), 100% -> 50 grains/sec
    // NOT a perfect inverse of size - allows gaps at low density
    return 0.25f * powf(200.0f, density / 100.0f);
}

// Map density to grain size in seconds
static float densityToSize(float density) {
    // 0% -> 0.5s, 100% -> 20ms
    // Shorter grains at low density = audible gaps between grains
    return 0.5f * powf(0.04f, density / 100.0f);
}

// Calculate per-drifter volume based on tilt
static float tiltVolume(int drifterIndex, float tilt) {
    // tilt: -1 (dark) to +1 (bright)
    // D1 (bass) gets louder when tilt negative, D4 (air) gets louder when positive
    float normalizedIndex = (float)drifterIndex / (kNumDrifters - 1);  // 0 to 1
    float tiltEffect = (normalizedIndex - 0.5f) * 2.0f * tilt;  // -1 to +1 range
    return powf(10.0f, tiltEffect * 0.3f);  // ±6dB range
}

// ============================================================================
// FACTORY FUNCTIONS
// ============================================================================

void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications) {
    req.numParameters = ARRAY_SIZE(parameters);
    req.sram = sizeof(_driftEngineAlgorithm);
    req.dram = sizeof(_driftEngine_DRAM);
    req.dtc = sizeof(_driftEngine_DTC);
    req.itc = 0;
}

// Callback when WAV loading completes (like sample player example)
static void wavLoadCallback(void* callbackData, bool success) {
    _driftEngineAlgorithm* pThis = (_driftEngineAlgorithm*)callbackData;
    pThis->awaitingCallback = false;

    if (success) {
        // Apply the pending sample info now that load is complete
        pThis->dram->sampleLength = pThis->pendingSampleLength;
        pThis->sampleRateRatio = pThis->pendingSampleRateRatio;
        pThis->dram->sampleLoaded = true;
    }
}

// Helper to initiate sample loading (like sample player example)
// Returns true if load was initiated, false if conditions not met
static bool loadSample(_driftEngineAlgorithm* pThis) {
    // Don't try to load during construction or if card not mounted
    if (!pThis->initialized || !NT_isSdCardMounted()) {
        return false;
    }

    int folder = pThis->v[kParamFolder];
    int sample = pThis->v[kParamSample];

    // Get sample info (like sample player example)
    _NT_wavInfo info;
    NT_getSampleFileInfo(folder, sample, info);

    if (info.numFrames == 0) {
        // No valid sample - keep playing whatever was loaded before
        return false;
    }

    // Limit to our buffer size
    uint32_t framesToRead = info.numFrames;
    if (framesToRead > kMaxSampleFrames) {
        framesToRead = kMaxSampleFrames;
    }

    // Store pending values - will be applied in callback when load completes
    // This keeps the old sample playing until the new one is ready
    pThis->pendingSampleLength = framesToRead;
    pThis->pendingSampleRateRatio = (float)info.sampleRate / NT_globals.sampleRate;
    pThis->dram->sampleIsStereo = (info.channels == kNT_WavStereo);

    // Prepare the request (like sample player example)
    // Always request mono - the granular engine adds stereo spread via panning
    pThis->wavRequest.folder = folder;
    pThis->wavRequest.sample = sample;
    pThis->wavRequest.dst = pThis->dram->sampleBufferL;
    pThis->wavRequest.numFrames = framesToRead;
    pThis->wavRequest.startOffset = 0;
    pThis->wavRequest.channels = kNT_WavMono;    // Always mono (API will sum stereo)
    pThis->wavRequest.bits = kNT_WavBits32;      // Convert to float
    pThis->wavRequest.progress = kNT_WavProgress;
    pThis->wavRequest.callback = wavLoadCallback;
    pThis->wavRequest.callbackData = pThis;

    if (NT_readSampleFrames(pThis->wavRequest)) {
        pThis->awaitingCallback = true;
        return true;
    }
    return false;
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs,
                         const _NT_algorithmRequirements& req,
                         const int32_t* specifications) {
    _driftEngine_DTC* dtc = (_driftEngine_DTC*)ptrs.dtc;
    _driftEngine_DRAM* dram = (_driftEngine_DRAM*)ptrs.dram;

    // Initialize DTC
    memset(dtc, 0, sizeof(_driftEngine_DTC));
    dtc->randState = 0x12345678;  // Seed
    dtc->smoothNorm = 1.0f;       // Start at unity gain

    // Initialize drifters with spread positions
    for (int i = 0; i < kNumDrifters; i++) {
        dtc->drifters[i].position = 0.25f + i * 0.15f;  // Spread across sample
        dtc->drifters[i].velocity = 0;
        dtc->drifters[i].pitchOffset = 0;
        dtc->drifters[i].timeSinceGrain = 0;
        dtc->drifters[i].nextGrainTime = randFloat(dtc) * 0.5f;  // Stagger initial grains
        dtc->drifters[i].variation = 0.5f + randFloat(dtc) * 0.5f;  // 0.5-1.0, set once
        dtc->drifters[i].driftDirection = (i % 2 == 0) ? 1.0f : -1.0f;  // Alternate directions
    }

    // Initialize DRAM
    memset(dram->sampleBufferL, 0, sizeof(dram->sampleBufferL));
    memset(dram->sampleBufferR, 0, sizeof(dram->sampleBufferR));
    dram->sampleLength = 0;
    dram->sampleLoaded = false;
    dram->sampleIsStereo = false;

    // Initialize reverb
    float sr = NT_globals.sampleRate;
    dram->reverb.init(sr);

    // Generate a test tone for testing without SD card samples
    // Creates a 4-second rich harmonic pad sound
    int testLen = (int)(sr * 4);
    dram->sampleLength = testLen;
    dram->sampleLoaded = true;
    dram->sampleIsStereo = false;
    for (int i = 0; i < testLen; i++) {
        float t = (float)i / sr;
        float env = 1.0f;  // Sustained
        // Multiple harmonics for a rich sound
        float sample = 0;
        sample += sinf(2.0f * M_PI * 220.0f * t) * 0.3f;       // Fundamental
        sample += sinf(2.0f * M_PI * 440.0f * t) * 0.2f;       // Octave
        sample += sinf(2.0f * M_PI * 330.0f * t) * 0.15f;      // Fifth
        sample += sinf(2.0f * M_PI * 550.0f * t) * 0.1f;       // Major third
        sample += sinf(2.0f * M_PI * 660.0f * t) * 0.08f;      // Fifth + octave
        // Slow amplitude modulation for movement
        sample *= (0.8f + 0.2f * sinf(2.0f * M_PI * 0.5f * t));
        sample *= env * 0.5f;
        dram->sampleBufferL[i] = sample;
        dram->sampleBufferR[i] = sample;
    }

    // Create algorithm
    _driftEngineAlgorithm* alg = new (ptrs.sram) _driftEngineAlgorithm(dtc, dram);

    // Copy parameters to mutable array (like sample player example)
    memcpy(alg->params, parameters, sizeof(parameters));
    alg->parameters = alg->params;
    alg->parameterPages = &parameterPages;

    // Initialize WAV loading state
    alg->cardMounted = false;
    alg->awaitingCallback = false;
    alg->initialized = false;
    alg->pendingSampleLoad = false;
    alg->pendingSampleLength = 0;
    alg->pendingSampleRateRatio = 1.0f;
    alg->sampleRateRatio = 1.0f;

    // Initialize cached values
    alg->anchorTarget = 0.5f;
    alg->wanderAmount = 0.3f;
    alg->gravityForce = 0;
    alg->driftSpeed = 0.3f;
    alg->densityRate = 8.0f;
    alg->grainSize = 0.125f * sr;
    alg->pitchOffset = 0;
    alg->scatterAmount = 0;
    alg->spectrumSep = 0;
    alg->tiltAmount = 0;
    alg->grainShape = kShapeCloud;
    alg->fogAmount = 0.3f;
    alg->entropyTarget = 0.25f;
    alg->deviationAmount = 1.0f;

    // Mark as fully initialized
    alg->initialized = true;

    return alg;
}

void parameterChanged(_NT_algorithm* self, int p) {
    _driftEngineAlgorithm* pThis = (_driftEngineAlgorithm*)self;
    float sr = NT_globals.sampleRate;

    switch (p) {
        case kParamFolder: {
            // Set the maximum value of the sample parameter (like sample player example)
            _NT_wavFolderInfo folderInfo;
            NT_getSampleFolderInfo(pThis->v[kParamFolder], folderInfo);
            pThis->params[kParamSample].max = folderInfo.numSampleFiles - 1;
#ifdef DISTING_HARDWARE
            NT_updateParameterDefinition(NT_algorithmIndex(self), kParamSample);
#endif
            break;
        }
        case kParamSample:
            // Request sample load (deferred to step() for safety)
            pThis->pendingSampleLoad = true;
            break;
        case kParamAnchor:
            pThis->anchorTarget = pThis->v[kParamAnchor] / 100.0f;
            break;
        case kParamWander:
            pThis->wanderAmount = pThis->v[kParamWander] / 100.0f;
            break;
        case kParamGravity:
            pThis->gravityForce = pThis->v[kParamGravity] / 100.0f;
            break;
        case kParamDrift:
            pThis->driftSpeed = pThis->v[kParamDrift] / 100.0f;
            break;
        case kParamDensity: {
            float d = pThis->v[kParamDensity];
            pThis->densityRate = densityToRate(d);
            pThis->grainSize = densityToSize(d) * sr;
            break;
        }
        case kParamDeviation:
            pThis->deviationAmount = pThis->v[kParamDeviation] / 100.0f;
            break;
        case kParamPitch:
            pThis->pitchOffset = pThis->v[kParamPitch];
            break;
        case kParamScatter:
            pThis->scatterAmount = pThis->v[kParamScatter];
            break;
        case kParamSpectrum:
            pThis->spectrumSep = pThis->v[kParamSpectrum] / 100.0f;
            break;
        case kParamTilt:
            pThis->tiltAmount = pThis->v[kParamTilt] / 100.0f;
            break;
        case kParamShape:
            pThis->grainShape = (GrainShape)pThis->v[kParamShape];
            break;
        case kParamFog:
            pThis->fogAmount = pThis->v[kParamFog] / 100.0f;
            break;
        case kParamEntropy:
            pThis->entropyTarget = pThis->v[kParamEntropy] / 100.0f;
            break;
    }
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    _driftEngineAlgorithm* pThis = (_driftEngineAlgorithm*)self;
    _driftEngine_DTC* dtc = pThis->dtc;
    _driftEngine_DRAM* dram = pThis->dram;

    int numFrames = numFramesBy4 * 4;
    float sr = NT_globals.sampleRate;
    float dt = 1.0f / sr;

    // Check for SD card mount/unmount
    bool cardMounted = NT_isSdCardMounted();
    if (pThis->cardMounted != cardMounted) {
        pThis->cardMounted = cardMounted;
        if (cardMounted) {
            // Set the maximum value of the folder parameter (like sample player example)
            pThis->params[kParamFolder].max = NT_getNumSampleFolders() - 1;
#ifdef DISTING_HARDWARE
            NT_updateParameterDefinition(NT_algorithmIndex(self), kParamFolder);
#endif
            // Also update sample max for current folder
            _NT_wavFolderInfo folderInfo;
            NT_getSampleFolderInfo(pThis->v[kParamFolder], folderInfo);
            pThis->params[kParamSample].max = folderInfo.numSampleFiles - 1;
#ifdef DISTING_HARDWARE
            NT_updateParameterDefinition(NT_algorithmIndex(self), kParamSample);
#endif
            // Request sample load (handled via pendingSampleLoad below)
            pThis->pendingSampleLoad = true;
        } else {
            // Card unmounted - DON'T clear sampleLoaded!
            // Keep playing whatever was loaded (test tone or previously loaded sample)
            // Only clear pending load state
            pThis->pendingSampleLoad = false;
            pThis->awaitingCallback = false;
        }
    }

    // Handle deferred sample load requests
    if (pThis->pendingSampleLoad && !pThis->awaitingCallback) {
        if (loadSample(pThis)) {
            pThis->pendingSampleLoad = false;  // Only clear if load actually started
        }
    }

    // Get output busses
    float* outL = busFrames + (pThis->v[kParamOutputL] - 1) * numFrames;
    float* outR = busFrames + (pThis->v[kParamOutputR] - 1) * numFrames;
    bool replace = pThis->v[kParamOutputLMode];

    // Get CV inputs (only if assigned - value 0 means "None")
    const float* cvAnchor = (pThis->v[kParamCvAnchor] > 0) ? busFrames + (pThis->v[kParamCvAnchor] - 1) * numFrames : NULL;
    const float* cvPitch = (pThis->v[kParamCvPitch] > 0) ? busFrames + (pThis->v[kParamCvPitch] - 1) * numFrames : NULL;
    const float* cvDrift = (pThis->v[kParamCvDrift] > 0) ? busFrames + (pThis->v[kParamCvDrift] - 1) * numFrames : NULL;
    const float* cvEntropy = (pThis->v[kParamCvEntropy] > 0) ? busFrames + (pThis->v[kParamCvEntropy] - 1) * numFrames : NULL;
    const float* cvStorm = (pThis->v[kParamCvStorm] > 0) ? busFrames + (pThis->v[kParamCvStorm] - 1) * numFrames : NULL;
    const float* cvClock = (pThis->v[kParamCvClock] > 0) ? busFrames + (pThis->v[kParamCvClock] - 1) * numFrames : NULL;

    // Get CV outputs
    float* cvOutPos = busFrames + (pThis->v[kParamCvOutPosition] - 1) * numFrames;
    float* cvOutPulse = busFrames + (pThis->v[kParamCvOutPulse] - 1) * numFrames;

    // Check if sample loaded
    if (!dram->sampleLoaded || dram->sampleLength < 100) {
        // Output silence
        for (int i = 0; i < numFrames; i++) {
            if (replace) {
                outL[i] = 0;
                outR[i] = 0;
            }
            cvOutPos[i] = 0;
            cvOutPulse[i] = 0;
        }
        return;
    }

    float sampleLen = (float)dram->sampleLength;

    // Process each sample
    for (int frame = 0; frame < numFrames; frame++) {
        // Read CV modulation (sample at audio rate, check for NULL)
        float anchorMod = cvAnchor ? cvAnchor[frame] * 0.1f : 0;  // ±5V = ±0.5 (50%)
        float pitchMod = cvPitch ? cvPitch[frame] * 12.0f : 0;  // 1V/oct
        float driftMod = cvDrift ? 1.0f + cvDrift[frame] * 0.2f : 1.0f;  // ±5V = ±100%
        float entropyMod = cvEntropy ? fmaxf(0, cvEntropy[frame] * 0.2f) : 0;  // 0-5V = 0-100%
        float stormGate = cvStorm ? cvStorm[frame] > 1.0f : false;  // Gate threshold
        float clockIn = cvClock ? cvClock[frame] : 0;

        // Smooth parameters
        float smoothRate = 0.001f;
        dtc->anchorSmooth += (pThis->anchorTarget + anchorMod - dtc->anchorSmooth) * smoothRate;
        dtc->driftSmooth += (pThis->driftSpeed * driftMod - dtc->driftSmooth) * smoothRate;
        dtc->densitySmooth += (pThis->densityRate - dtc->densitySmooth) * smoothRate;

        // Entropy with CV and storm
        float targetEntropy = pThis->entropyTarget + entropyMod;
        if (stormGate) {
            dtc->stormLevel = 1.0f;  // Instant max
        } else {
            dtc->stormLevel *= 0.9999f;  // Slow decay (~5-10 seconds)
        }
        float effectiveEntropy = fminf(1.0f, targetEntropy + dtc->stormLevel);
        dtc->entropySmooth += (effectiveEntropy - dtc->entropySmooth) * smoothRate;

        // Clock detection
        if (clockIn > 1.0f && dtc->prevClock <= 1.0f) {
            // Rising edge
            if (dtc->clockReceived) {
                // Calculate period from last clock
                dtc->clockPeriod = 1.0f / dtc->clockPhase;  // Period in samples
            }
            dtc->clockPhase = 0;
            dtc->clockReceived = true;
        }
        dtc->prevClock = clockIn;
        if (dtc->clockReceived) {
            dtc->clockPhase += dt;
        }

        // ====== UPDATE DRIFTERS ======
        float anchor = fmaxf(0, fminf(1, dtc->anchorSmooth));
        float wander = pThis->wanderAmount;
        float gravity = pThis->gravityForce;
        float drift = dtc->driftSmooth;
        float entropy = dtc->entropySmooth;

        float avgPos = 0;

        for (int d = 0; d < kNumDrifters; d++) {
            Drifter& drifter = dtc->drifters[d];

            // Calculate gravity force toward/away from anchor
            float dist = drifter.position - anchor;
            float gravityAccel = -gravity * dist * 100.0f;  // Toward anchor when positive

            // Random walk based on entropy
            float randomWalk = randFloatBipolar(dtc) * entropy * 0.01f;

            // Update velocity
            drifter.velocity += gravityAccel * dt;
            drifter.velocity += randomWalk;
            drifter.velocity *= 0.995f;  // Slightly less aggressive damping

            // Base drift speed - uses stored variation and BIDIRECTIONAL direction
            // Scale by dt so drift is time-based, not sample-rate dependent
            // 0.05 gives ~1% per second at 30% drift - slow ambient movement
            float baseDrift = drift * drifter.variation * drifter.driftDirection * dt * 0.05f;

            // Update position
            drifter.position += drifter.velocity * dt + baseDrift;

            // Constrain to wander range around anchor
            float minPos = anchor - wander;
            float maxPos = anchor + wander;

            // Soft boundaries with bounce - reverse drift direction on hit
            if (drifter.position < minPos) {
                drifter.position = minPos + (minPos - drifter.position) * 0.5f;
                drifter.velocity = fabsf(drifter.velocity) * 0.5f;
                drifter.driftDirection = 1.0f;  // Now drift right
            }
            if (drifter.position > maxPos) {
                drifter.position = maxPos - (drifter.position - maxPos) * 0.5f;
                drifter.velocity = -fabsf(drifter.velocity) * 0.5f;
                drifter.driftDirection = -1.0f;  // Now drift left
            }


            // Hard clamp
            drifter.position = fmaxf(0.001f, fminf(0.999f, drifter.position));

            avgPos += drifter.position;

            // ====== GRAIN TRIGGERING (Poisson) ======
            drifter.timeSinceGrain += dt;

            if (drifter.timeSinceGrain >= drifter.nextGrainTime) {
                // Trigger new grain
                drifter.timeSinceGrain = 0;

                // Calculate lambda for NEXT grain interval (not every sample)
                float lambda = dtc->densitySmooth;

                // If clock is present and deviation is low, quantize to clock
                if (dtc->clockReceived && pThis->deviationAmount < 1.0f) {
                    float clockRate = 1.0f / dtc->clockPeriod;
                    float blend = 1.0f - pThis->deviationAmount;
                    lambda = lambda * pThis->deviationAmount + clockRate * blend;
                }

                // Add entropy-based jitter to rate (only at trigger time, not every sample)
                lambda *= 1.0f + randFloatBipolar(dtc) * entropy * 0.5f;

                drifter.nextGrainTime = randExponential(dtc, lambda);

                // Find free grain slot
                for (int g = 0; g < kMaxTotalGrains; g++) {
                    if (!dtc->grains[g].active) {
                        Grain& grain = dtc->grains[g];
                        grain.active = true;
                        // Snap to nearest zero crossing to reduce clicks (256 samples for low freq content)
                        int rawPos = (int)(drifter.position * sampleLen);
                        grain.position = (float)findNearestZeroCrossing(dram->sampleBufferL, rawPos, dram->sampleLength, 256);
                        grain.phase = 0;
                        grain.phaseDelta = 1.0f / pThis->grainSize;
                        grain.drifterIndex = d;
                        grain.shape = pThis->grainShape;
                        grain.amplitude = 1.0f;  // Base amplitude (soft clipping handles overload)

                        // Calculate pitch
                        float pitchSemis = pThis->pitchOffset + pitchMod;
                        // Scatter: D1&D4 get positive, D2&D3 get negative
                        float scatterDir = (d == 0 || d == 3) ? 1.0f : -1.0f;
                        float scatterIdx = (d == 0 || d == 3) ? fabsf(d - 1.5f) : fabsf(d - 1.5f);
                        pitchSemis += pThis->scatterAmount * scatterDir * (scatterIdx / 1.5f);

                        // Add per-grain random pitch based on entropy
                        pitchSemis += randFloatBipolar(dtc) * entropy * 2.0f;  // ±2 semitones max

                        // Include sample rate ratio for proper playback speed
                        grain.positionDelta = powf(2.0f, pitchSemis / 12.0f) * pThis->sampleRateRatio;

                        // Don't reset filters - let state carry over to avoid transients
                        // grain.filterL.reset();
                        // grain.filterR.reset();

                        // Pulse output trigger
                        dtc->pulseOut = true;

                        break;
                    }
                }
            }
        }

        dtc->averagePosition = avgPos / kNumDrifters;

        // ====== RENDER GRAINS ======
        float mixL = 0;
        float mixR = 0;
        int activeGrains = 0;

        for (int g = 0; g < kMaxTotalGrains; g++) {
            Grain& grain = dtc->grains[g];
            if (!grain.active) continue;
            activeGrains++;

            // Read sample with linear interpolation
            int pos0 = (int)grain.position;
            int pos1 = pos0 + 1;
            float frac = grain.position - pos0;

            // Wrap positions
            pos0 = pos0 % dram->sampleLength;
            pos1 = pos1 % dram->sampleLength;
            if (pos0 < 0) pos0 += dram->sampleLength;
            if (pos1 < 0) pos1 += dram->sampleLength;

            // Read from mono buffer (we always load mono, stereo spread comes from panning)
            float sampleMono = dram->sampleBufferL[pos0] * (1 - frac) + dram->sampleBufferL[pos1] * frac;
            float sampleL = sampleMono;
            float sampleR = sampleMono;

            // Apply grain envelope
            float env = grainEnvelope(grain.phase, grain.shape);
            sampleL *= env * grain.amplitude;
            sampleR *= env * grain.amplitude;

            // Apply filter bank separation (spectrum parameter)
            int d = grain.drifterIndex;
            float filterFreq = kBandCenterFreqs[d];
            float filterQ = 1.0f + pThis->spectrumSep * 2.0f;  // Q from 1 to 3

            if (pThis->spectrumSep > 0.01f) {
                sampleL = grain.filterL.process(sampleL, filterFreq, filterQ, sr) * (1.0f + pThis->spectrumSep);
                sampleR = grain.filterR.process(sampleR, filterFreq, filterQ, sr) * (1.0f + pThis->spectrumSep);
            }

            // Apply tilt (per-drifter volume)
            float tiltVol = tiltVolume(d, pThis->tiltAmount);
            sampleL *= tiltVol;
            sampleR *= tiltVol;

            // Apply stereo panning
            float pan = kDrifterPan[d];
            float panL = cosf((pan + 1) * 0.25f * M_PI);  // Equal power panning
            float panR = sinf((pan + 1) * 0.25f * M_PI);

            mixL += sampleL * panL;
            mixR += sampleR * panR;

            // Advance grain
            grain.position += grain.positionDelta;
            grain.phase += grain.phaseDelta;

            // Wrap position
            while (grain.position >= sampleLen) grain.position -= sampleLen;
            while (grain.position < 0) grain.position += sampleLen;

            // Check if grain finished
            if (grain.phase >= 1.0f) {
                grain.active = false;
            }
        }

        // Normalize by grain count to prevent saturation (sqrt for density perception)
        // Smooth the normalization factor to prevent clicks from sudden grain count changes
        float targetNorm = (activeGrains > 1) ? 1.0f / sqrtf((float)activeGrains) : 1.0f;
        dtc->smoothNorm += 0.001f * (targetNorm - dtc->smoothNorm);  // Very slow smoothing
        if (dtc->smoothNorm < 0.1f) dtc->smoothNorm = 0.1f;  // Prevent divide issues
        mixL *= dtc->smoothNorm;
        mixR *= dtc->smoothNorm;

        // Apply reverb (fog) - fixed: wet applied only once
        float dry = 1.0f - pThis->fogAmount * 0.5f;
        float wet = pThis->fogAmount;
        float reverbIn = (mixL + mixR) * 0.5f;  // No wet here
        float reverbOut = dram->reverb.process(reverbIn, pThis->fogAmount);

        mixL = mixL * dry + reverbOut * wet;
        mixR = mixR * dry + reverbOut * wet;

        // Soft clipping with Eurorack-level output (±5V)
        mixL = tanhf(mixL * 2.0f) * 5.0f;
        mixR = tanhf(mixR * 2.0f) * 5.0f;

        // NaN/Inf protection
        if (mixL != mixL || mixL > 1e10f || mixL < -1e10f) mixL = 0;
        if (mixR != mixR || mixR > 1e10f || mixR < -1e10f) mixR = 0;

        // Output
        if (replace) {
            outL[frame] = mixL;
            outR[frame] = mixR;
        } else {
            outL[frame] += mixL;
            outR[frame] += mixR;
        }

        // CV outputs
        cvOutPos[frame] = dtc->averagePosition * 5.0f;  // 0-5V
        cvOutPulse[frame] = dtc->pulseOut ? 5.0f : 0;
        dtc->pulseOut = false;
    }
}

bool draw(_NT_algorithm* self) {
    _driftEngineAlgorithm* pThis = (_driftEngineAlgorithm*)self;
    _driftEngine_DTC* dtc = pThis->dtc;
    _driftEngine_DRAM* dram = pThis->dram;

    // Title
    NT_drawText(10, 10, "DRIFT ENGINE", 15, kNT_textLeft, kNT_textNormal);

    // Get folder/sample info directly from API (like sample player example)
    int folder = pThis->v[kParamFolder];
    int sample = pThis->v[kParamSample];

    _NT_wavFolderInfo folderInfo;
    NT_getSampleFolderInfo(folder, folderInfo);
    if (folderInfo.name) {
        NT_drawText(100, 10, folderInfo.name, 10, kNT_textLeft, kNT_textTiny);
    }

    _NT_wavInfo wavInfo;
    NT_getSampleFileInfo(folder, sample, wavInfo);
    if (wavInfo.name) {
        NT_drawText(10, 20, wavInfo.name, 10, kNT_textLeft, kNT_textTiny);
    }

    // Sample length indicator
    char slotText[32];
    if (dram->sampleLoaded) {
        // Show sample duration adjusted for sample rate
        float secs = (float)dram->sampleLength / (NT_globals.sampleRate * pThis->sampleRateRatio);
        int secInt = (int)secs;
        int secFrac = (int)((secs - secInt) * 10);
        int len = NT_intToString(slotText, secInt);
        slotText[len++] = '.';
        len += NT_intToString(slotText + len, secFrac);
        slotText[len++] = 's';
        slotText[len] = 0;
    } else if (pThis->awaitingCallback) {
        slotText[0] = '.'; slotText[1] = '.'; slotText[2] = '.'; slotText[3] = 0;
    } else {
        slotText[0] = '-';
        slotText[1] = 0;
    }
    NT_drawText(246, 10, slotText, 12, kNT_textRight, kNT_textNormal);

    // Sample waveform bar (simplified)
    int barY = 28;
    int barH = 10;
    NT_drawShapeI(kNT_box, 10, barY, 246, barY + barH, 8);  // Outline

    // Draw anchor position
    float anchor = dtc->anchorSmooth;
    int anchorX = 10 + (int)(anchor * 236);
    NT_drawShapeI(kNT_line, anchorX, barY - 2, anchorX, barY + barH + 2, 10);  // Anchor line

    // Draw wander range
    float wander = pThis->wanderAmount;
    int wanderMinX = 10 + (int)((anchor - wander) * 236);
    int wanderMaxX = 10 + (int)((anchor + wander) * 236);
    wanderMinX = fmaxf(10, wanderMinX);
    wanderMaxX = fminf(246, wanderMaxX);
    NT_drawShapeI(kNT_rectangle, wanderMinX, barY + 1, wanderMaxX, barY + barH - 1, 4);  // Wander zone

    // Draw drifter positions as bright markers extending above and below bar
    for (int d = 0; d < kNumDrifters; d++) {
        int x = 10 + (int)(dtc->drifters[d].position * 236);
        x = fmaxf(12, fminf(244, x));

        // Draw triangular marker above the bar (pointing down)
        NT_drawShapeI(kNT_rectangle, x - 1, barY - 4, x + 2, barY, 15);
        // Draw triangular marker below the bar (pointing up)
        NT_drawShapeI(kNT_rectangle, x - 1, barY + barH, x + 2, barY + barH + 4, 15);
    }

    // Status line
    char statusLine[32];
    int activeGrains = 0;
    for (int g = 0; g < kMaxTotalGrains; g++) {
        if (dtc->grains[g].active) activeGrains++;
    }

    // Show active grain count and entropy
    NT_drawText(10, 48, "Grains:", 10, kNT_textLeft, kNT_textTiny);
    NT_intToString(statusLine, activeGrains);
    NT_drawText(45, 48, statusLine, 12, kNT_textLeft, kNT_textTiny);

    // Show storm indicator if active
    if (dtc->stormLevel > 0.01f) {
        int stormWidth = (int)(dtc->stormLevel * 40);
        NT_drawShapeI(kNT_rectangle, 200, 48, 200 + stormWidth, 52, 15);
        NT_drawText(200, 48, "STORM", 15, kNT_textLeft, kNT_textTiny);
    }

    // Gravity indicator (bipolar bar: center = 0, left = negative, right = positive)
    NT_drawText(60, 48, "Grav:", 10, kNT_textLeft, kNT_textTiny);
    int gravCenterX = 105;
    int gravHalfWidth = 20;
    float gravNorm = pThis->gravityForce;  // -1 to +1
    NT_drawShapeI(kNT_line, gravCenterX, 49, gravCenterX, 53, 8);  // Center mark
    if (gravNorm > 0.01f) {
        int gravWidth = (int)(gravNorm * gravHalfWidth);
        NT_drawShapeI(kNT_rectangle, gravCenterX, 49, gravCenterX + gravWidth, 53, 12);
    } else if (gravNorm < -0.01f) {
        int gravWidth = (int)(-gravNorm * gravHalfWidth);
        NT_drawShapeI(kNT_rectangle, gravCenterX - gravWidth, 49, gravCenterX, 53, 12);
    }

    // Entropy indicator
    NT_drawText(135, 48, "Ent:", 10, kNT_textLeft, kNT_textTiny);
    int entropyWidth = (int)(dtc->entropySmooth * 30);
    NT_drawShapeI(kNT_rectangle, 160, 49, 160 + entropyWidth, 53, 12);

    return true;  // Hide standard parameter line, we draw everything
}

// ============================================================================
// CUSTOM UI - Hardware pot/encoder mapping
// ============================================================================

// Custom UI layout:
//   Pot L: Density    Pot C: Anchor (Position)    Pot R: Spectrum
//   Enc L: Fog        Enc R: Entropy

uint32_t hasCustomUi(_NT_algorithm* self) {
    // Return bitmask of controls we override
    return kNT_potL | kNT_potC | kNT_potR | kNT_encoderL | kNT_encoderR;
}

void customUi(_NT_algorithm* self, const _NT_uiData& data) {
    _driftEngineAlgorithm* pThis = (_driftEngineAlgorithm*)self;
    int algIndex = NT_algorithmIndex(self);
    int offset = NT_parameterOffset();

    // Pot L: Density (0-100%)
    if (data.controls & kNT_potL) {
        int value = (int)(data.pots[0] * 100.0f);
        NT_setParameterFromUi(algIndex, kParamDensity + offset, value);
    }

    // Pot C: Anchor/Position (0-100%)
    if (data.controls & kNT_potC) {
        int value = (int)(data.pots[1] * 100.0f);
        NT_setParameterFromUi(algIndex, kParamAnchor + offset, value);
    }

    // Pot R: Spectrum (0-100%)
    if (data.controls & kNT_potR) {
        int value = (int)(data.pots[2] * 100.0f);
        NT_setParameterFromUi(algIndex, kParamSpectrum + offset, value);
    }

    // Encoder L (left): Gravity - increment/decrement
    if (data.encoders[0] != 0) {
        int current = pThis->v[kParamGravity];
        int newVal = current + data.encoders[0] * 5;  // 5% steps
        if (newVal < -100) newVal = -100;
        if (newVal > 100) newVal = 100;
        NT_setParameterFromUi(algIndex, kParamGravity + offset, newVal);
    }

    // Encoder R (right): Entropy - increment/decrement
    if (data.encoders[1] != 0) {
        int current = pThis->v[kParamEntropy];
        int newVal = current + data.encoders[1] * 5;  // 5% steps
        if (newVal < 0) newVal = 0;
        if (newVal > 100) newVal = 100;
        NT_setParameterFromUi(algIndex, kParamEntropy + offset, newVal);
    }
}

void setupUi(_NT_algorithm* self, _NT_float3& pots) {
    _driftEngineAlgorithm* pThis = (_driftEngineAlgorithm*)self;

    // Initialize pot positions for soft takeover
    pots[0] = pThis->v[kParamDensity] / 100.0f;    // Pot L: Density
    pots[1] = pThis->v[kParamAnchor] / 100.0f;     // Pot C: Anchor
    pots[2] = pThis->v[kParamSpectrum] / 100.0f;   // Pot R: Spectrum
}

// ============================================================================
// FACTORY DEFINITION
// ============================================================================

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('N', 's', 'D', 'r'),  // Nealsanche + Drift
    .name = "Drift Engine",
    .description = "Granular sample explorer - 4 autonomous drifters",
    .numSpecifications = 0,
    .specifications = NULL,
    .calculateStaticRequirements = NULL,
    .initialise = NULL,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = parameterChanged,
    .step = step,
    .draw = draw,
    .midiRealtime = NULL,
    .midiMessage = NULL,
    .tags = kNT_tagEffect | kNT_tagInstrument,
    .hasCustomUi = hasCustomUi,
    .customUi = customUi,
    .setupUi = setupUi,
};

// ============================================================================
// PLUGIN ENTRY POINT
// ============================================================================

uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    switch (selector) {
        case kNT_selector_version:
            return kNT_apiVersion9;
        case kNT_selector_numFactories:
            return 1;
        case kNT_selector_factoryInfo:
            return (uintptr_t)((data == 0) ? &factory : NULL);
    }
    return 0;
}
