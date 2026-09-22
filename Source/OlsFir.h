#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>

// Overlap-save FIR convolution with automatic direct-form fallback.
//
// OLS is active when  blockSize × 8 >= N_OLS  (the same efficiency threshold
// as the reference plugin). At smaller block sizes it falls back to a
// circular-buffer direct-form convolution so the audio is never wrong —
// just slightly more expensive.
//
// With the default L1 FIR of ~1025 taps at 44.1 kHz:
//   block 32 / 64 / 128 → direct-form  (OLS overhead too large)
//   block 256 / 1024 / 2048 → OLS      (~5× cheaper than direct-form)
//
// Call prepare() whenever taps or block size change. It is safe to call
// prepare() every block — it caches the tap-data pointer and skips the FFT
// if nothing has changed.
struct OlsFir
{
    void prepare(const std::vector<float>& taps, int blockSize)
    {
        if (taps.empty() || blockSize <= 0) { reset(); return; }

        // Cache hit: same taps and same block size → nothing to recompute
        if (blockSize == bSz && (int)taps.size() == M && taps.data() == lastTapsPtr)
            return;

        M           = static_cast<int>(taps.size());
        bSz         = blockSize;
        lastTapsPtr = taps.data();
        directTaps  = taps;

        // N_OLS = next power of 2 >= blockSize + M - 1
        int minN = blockSize + M - 1;
        N = 1; int order = 0;
        while (N < minN) { N <<= 1; ++order; }

        // Efficiency threshold: OLS cheaper than direct-form when B×8 >= N
        olsActive = (blockSize * 8 >= N);

        // Direct-form state (always kept as fallback)
        history.assign(static_cast<size_t>(M), 0.0f);
        histWP = 0;

        if (olsActive)
        {
            fft = std::make_unique<juce::dsp::FFT>(order);

            // Zero-pad FIR to length N and compute its spectrum once
            firSpec.assign(static_cast<size_t>(2 * N), 0.0f);
            for (int i = 0; i < M; ++i)
                firSpec[static_cast<size_t>(i)] = taps[static_cast<size_t>(i)];
            fft->performRealOnlyForwardTransform(firSpec.data(), false);

            overlap.assign(static_cast<size_t>(M - 1), 0.0f);
            segBuf.assign(static_cast<size_t>(2 * N), 0.0f);
        }
    }

    void reset()
    {
        std::fill(history.begin(), history.end(), 0.0f); histWP = 0;
        std::fill(overlap.begin(), overlap.end(), 0.0f);
        M = 0; N = 0; bSz = 0; olsActive = false; lastTapsPtr = nullptr;
    }

    bool empty() const { return M == 0; }

    // Process one block in-place.
    // Uses OLS if numSamples == bSz and OLS is active; otherwise direct-form.
    void process(float* data, int numSamples)
    {
        if (M == 0 || numSamples == 0) return;
        if (olsActive && numSamples == bSz && fft)
            processOLS(data, numSamples);
        else
            processDirectForm(data, numSamples);
    }

private:
    void processOLS(float* data, int B)
    {
        // ── Build segment: [overlap(M-1) | input(B) | zeros(N-M-B+1)] ──────────
        std::fill(segBuf.begin(), segBuf.end(), 0.0f);
        for (int i = 0; i < M - 1; ++i)
            segBuf[static_cast<size_t>(i)] = overlap[static_cast<size_t>(i)];
        for (int i = 0; i < B; ++i)
            segBuf[static_cast<size_t>(M - 1 + i)] = data[i];

        // ── Update overlap = last M-1 samples of [overlap | input] ───────────
        if (B >= M - 1)
        {
            for (int i = 0; i < M - 1; ++i)
                overlap[static_cast<size_t>(i)] = data[B - (M - 1) + i];
        }
        else
        {
            // Shift old overlap left by B, append new B samples on the right
            const int keep = M - 1 - B;
            for (int i = 0; i < keep; ++i)
                overlap[static_cast<size_t>(i)] = overlap[static_cast<size_t>(i + B)];
            for (int i = 0; i < B; ++i)
                overlap[static_cast<size_t>(keep + i)] = data[i];
        }

        // ── FFT of segment ────────────────────────────────────────────────────
        fft->performRealOnlyForwardTransform(segBuf.data(), false);

        // ── Pointwise complex multiply with precomputed FIR spectrum ──────────
        for (int k = 0; k < N; ++k)
        {
            const int   k2  = k * 2;
            const float sRe = segBuf[k2],    sIm = segBuf[k2 + 1];
            const float hRe = firSpec[k2],   hIm = firSpec[k2 + 1];
            segBuf[k2]     = sRe * hRe - sIm * hIm;
            segBuf[k2 + 1] = sRe * hIm + sIm * hRe;
        }

        // ── IFFT (JUCE normalises by 1/N) ─────────────────────────────────────
        fft->performRealOnlyInverseTransform(segBuf.data());

        // ── Extract valid samples: positions M-1 … M-1+B-1 ───────────────────
        for (int i = 0; i < B; ++i)
            data[i] = segBuf[static_cast<size_t>(M - 1 + i)];
    }

    void processDirectForm(float* data, int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            history[static_cast<size_t>(histWP)] = data[n];
            float y = 0.0f;
            int   rp = histWP;
            for (int k = 0; k < M; ++k)
            {
                y += directTaps[static_cast<size_t>(k)] * history[static_cast<size_t>(rp)];
                if (--rp < 0) rp = M - 1;
            }
            if (++histWP >= M) histWP = 0;
            data[n] = y;
        }
    }

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> firSpec;        // 2*N interleaved complex, precomputed
    std::vector<float> overlap;        // M-1 overlap samples carried between blocks
    std::vector<float> segBuf;         // 2*N working buffer
    std::vector<float> history;        // M-sample circular buffer (direct-form)
    std::vector<float> directTaps;     // tap copy for direct-form
    const float*       lastTapsPtr = nullptr;
    int M = 0, N = 0, bSz = 0, histWP = 0;
    bool olsActive = false;
};
