#pragma once

namespace AdaptiveIRConfig
{
    // =========================
    // UI
    // =========================
    inline constexpr int UiRefreshMs = 100;

    // =========================
    // Behaviour when no IR loaded
    // =========================
    // true = pass-through (no processing)
    // false = mute output
    inline constexpr bool BypassIfNoIR = true;

    // =========================
    // Frequency range for curve generation & display
    // =========================
    inline constexpr float MinFreqHz = 40.0f;
    inline constexpr float MaxFreqCapHz = 18000.0f; // clamped to Nyquist

    // =========================
    // DynamicEQ (FFT)
    // =========================
    inline constexpr int FftOrder = 12; // 2^12 = 4096
    inline constexpr float HopFraction = 0.5f; // hop = fftSize * HopFraction
    inline constexpr bool NormalizeIfft = true; // (nota: ora non viene più usato nel DynamicEQ patchato)
    // Depth of inverse EQ mapping: gain = 1/(1 + energy*EqDepthFactor)
    inline constexpr float EqDepthFactor = 2.0f;

    // =========================
    // SpectralAnalyzer
    // =========================
    inline constexpr int PreSamples = 512;

    // =========================
    // TransientDetector
    // =========================
    inline constexpr float TransientAttackMs = 1.0f;
    inline constexpr float TransientReleaseMs = 50.0f;
    inline constexpr float RefractoryMs = 200.0f;

    // NEW: soglia in dB mappata dalla sensitivity (0..1)
    // Sens=0  -> trigger difficile (soglia alta, tipo -12 dB)
    // Sens=1  -> trigger facile   (soglia bassa, tipo -60 dB)
    inline constexpr float TransientThresholdDbAtSens0 = -12.0f;
    inline constexpr float TransientThresholdDbAtSens1 = -60.0f;

    // =========================
    // Parameters: ranges & defaults (centralizzati)
    // =========================
    // Release (seconds)
    inline constexpr float ReleaseMin = 0.1f;
    inline constexpr float ReleaseMax = 5.0f;
    inline constexpr float ReleaseStep = 0.01f;
    inline constexpr float ReleaseSkew = 0.5f;
    inline constexpr float ReleaseDefault = 1.0f;

    // Sensitivity (0..1)
    inline constexpr float SensMin = 0.0f;
    inline constexpr float SensMax = 1.0f;
    inline constexpr float SensStep = 0.01f;
    inline constexpr float SensDefault = 0.5f;

    // Resolution (bands per octave)
    inline constexpr int ResolutionMin = 1;
    inline constexpr int ResolutionMax = 4;
    inline constexpr int ResolutionDefault = 1;

    // Process mode default
    inline constexpr bool ProcessModeDefault = false;
}