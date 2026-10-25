#pragma once

#include "Sample.h"
#include <array>
#include <atomic>
#include <cmath>

namespace rf
{

// The keys synth: up to three layers per note, each an oscillator (with
// unison) or a sample (repitched, time-stretched or stretched to the host
// tempo), through one amp envelope and a per-voice filter with its own
// envelope. Poly, mono or legato with glide. Its output goes into the start
// of the effect chain.
class Synth
{
public:
    static constexpr int numLayers = 3;
    static constexpr int maxVoices = 16;
    static constexpr int maxUnison = 8;

    enum class Wave { supersaw, saw, square, triangle, sine, noise, sample };
    enum class Warp { repitch, stretch, synced };
    enum class VoiceMode { poly, mono, legato };

    struct Layer
    {
        bool on = false;
        Wave wave = Wave::saw;
        float level = 0.8f, pan = 0.0f;
        int octave = 0, semi = 0;
        float fine = 0.0f;            // cents
        int unison = 1;
        float detune = 20.0f;         // cents either side
        float spread = 0.5f;          // unison stereo width

        Warp warp = Warp::repitch;
        float speed = 1.0f;           // stretch: playback speed independent of pitch
        double syncBeats = 4.0;       // synced: the sample region lasts this many beats
        float grainMs = 60.0f;
        float start = 0.0f, end = 1.0f;
        bool loop = false, reverse = false;
    };

    struct Settings
    {
        std::array<Layer, numLayers> layers;

        float ampA = 2.0f, ampD = 300.0f, ampS = 1.0f, ampR = 250.0f;   // ms / level

        int filterType = 0;           // 0 LP, 1 BP, 2 HP
        float cutoff = 20000.0f, reso = 0.1f;
        float envAmt = 0.0f;          // -1..1 = ±6 octaves
        float keyTrack = 0.0f;
        float fltA = 2.0f, fltD = 400.0f, fltS = 0.0f, fltR = 300.0f;

        VoiceMode mode = VoiceMode::poly;
        float glideMs = 0.0f;
        float velSens = 0.7f;
        int transpose = 0;
        float level = 0.7f;
        double bpm = 128.0;
    };

    Synth()
    {
        for (auto& v : voices)
            v.owner = this;
        for (auto& p : playhead)
            p.store (-1.0f);
    }

    void prepare (double sr, int maxBlockSize)
    {
        sampleRate = sr;
        buffer.setSize (2, maxBlockSize);
        allOff (true);
    }

    //==========================================================================
    // Message thread: swap a layer's sample. Voices still playing the old one
    // keep it alive; it's freed later by collectGarbage(), never on the audio
    // thread.
    void setSample (int layer, Sample::Ptr s)
    {
        {
            const juce::SpinLock::ScopedLockType lock (sampleLock);
            std::swap (current[(size_t) layer], s);
        }
        if (s != nullptr)
            retired.add (s);
        collectGarbage();
    }

    Sample::Ptr getSample (int layer) const
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        return current[(size_t) layer];
    }

    void collectGarbage()
    {
        for (int i = retired.size(); --i >= 0;)
            if (retired.getUnchecked (i)->getReferenceCount() == 1)
                retired.remove (i);
    }

    // Normalised position of the newest voice in each layer's sample region,
    // or -1 when nothing is playing. Read by the editor.
    std::array<std::atomic<float>, numLayers> playhead;
    std::atomic<int> activeVoices { 0 };

    //==========================================================================
    // Renders the block's MIDI and adds the result into `out`.
    void renderAdding (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const Settings& s)
    {
        settings = s;
        {
            const juce::SpinLock::ScopedTryLockType lock (sampleLock);
            if (lock.isLocked())
                active = current;
        }

        auto n = out.getNumSamples();
        if (buffer.getNumSamples() < n)
            buffer.setSize (2, n, false, false, true);
        buffer.clear (0, n);

        int pos = 0;
        for (const auto m : midi)
        {
            auto when = juce::jlimit (0, n, m.samplePosition);
            renderVoices (pos, when - pos);
            pos = when;
            handle (m.getMessage());
        }
        renderVoices (pos, n - pos);

        for (int ch = 0; ch < out.getNumChannels(); ++ch)
            out.addFrom (ch, 0, buffer, juce::jmin (ch, 1), 0, n, s.level);

        publishPlayheads();
    }

    void allOff (bool immediately)
    {
        for (auto& v : voices)
            if (immediately) v.kill();
            else v.release();
        heldCount = 0;
        sustainDown = false;
    }

private:
    //==========================================================================
    struct Svf
    {
        float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        float s1[2] {}, s2[2] {};

        void set (float fc, float q, double sr)
        {
            g = (float) std::tan (juce::MathConstants<double>::pi * fc / sr);
            k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        float process (int ch, float v0, int type)
        {
            auto v3 = v0 - s2[ch];
            auto v1 = a1 * s1[ch] + a2 * v3;
            auto v2 = s2[ch] + a2 * s1[ch] + a3 * v3;
            s1[ch] = 2.0f * v1 - s1[ch];
            s2[ch] = 2.0f * v2 - s2[ch];
            return type == 1 ? v1 : type == 2 ? v0 - k * v1 - v2 : v2;
        }

        void reset() { s1[0] = s1[1] = s2[0] = s2[1] = 0.0f; }
    };

    struct Grain { double src = 0.0, offset = 0.0, phase = 0.0; };

    struct Sub
    {
        double phase = 0.0, inc = 0.0;    // oscillator
        double pos = 0.0, rate = 0.0;     // sample playhead and repitch rate
        Grain grains[2];
        float gl = 0.7f, gr = 0.7f;
        bool done = false;
    };

    struct LayerVoice
    {
        Sample::Ptr sample;
        std::array<Sub, maxUnison> subs;
        int count = 0;
        double regionStart = 0.0, regionEnd = 0.0, pitchRate = 0.0, headRate = 0.0;
        double grainLen = 2000.0;
        bool on = false;
    };

    class Voice
    {
    public:
        Synth* owner = nullptr;
        int note = -1, channel = 1;
        bool released = true, sustained = false;
        juce::uint32 age = 0;

        bool isActive() const { return activeFlag; }

        void start (int newNote, int newChannel, float velocity, float fromPitch, bool glide)
        {
            const auto& s = owner->settings;
            note = newNote;
            channel = newChannel;
            released = sustained = false;
            age = ++owner->ageCounter;
            target = (float) note;
            pitch = glide ? fromPitch : target;

            gain = (1.0f - s.velSens) + s.velSens * velocity;

            ampEnv.setSampleRate (owner->sampleRate);
            fltEnv.setSampleRate (owner->sampleRate);
            ampEnv.setParameters ({ s.ampA * 0.001f, s.ampD * 0.001f, s.ampS, s.ampR * 0.001f });
            fltEnv.setParameters ({ s.fltA * 0.001f, s.fltD * 0.001f, s.fltS, s.fltR * 0.001f });
            ampEnv.reset();
            fltEnv.reset();
            ampEnv.noteOn();
            fltEnv.noteOn();
            filter.reset();

            for (int l = 0; l < numLayers; ++l)
                startLayer (l);

            controlCountdown = 0;
            activeFlag = true;
        }

        // Legato: move to a new note without retriggering.
        void slideTo (int newNote)
        {
            note = newNote;
            target = (float) newNote;
            age = ++owner->ageCounter;
        }

        void release()
        {
            if (! activeFlag || released)
                return;
            released = true;
            ampEnv.noteOff();
            fltEnv.noteOff();
        }

        void kill()
        {
            activeFlag = false;
            released = true;
            note = -1;
            for (auto& lv : layers)
                lv.sample = nullptr; // just drops a reference; the owner still holds it
        }

        float currentPitch() const { return pitch; }

        float layerPosition (int l) const
        {
            const auto& lv = layers[(size_t) l];
            if (! lv.on || lv.sample == nullptr || lv.regionEnd <= lv.regionStart)
                return -1.0f;
            return (float) ((lv.subs[0].pos - lv.regionStart) / (lv.regionEnd - lv.regionStart));
        }

        void render (float* outL, float* outR, int start, int num)
        {
            const auto& s = owner->settings;

            for (int i = start; i < start + num; ++i)
            {
                if (--controlCountdown <= 0)
                {
                    updateControl();
                    controlCountdown = controlInterval;
                }

                auto fenv = fltEnv.getNextSample();
                float l = 0.0f, r = 0.0f;
                bool anySound = false;

                for (int li = 0; li < numLayers; ++li)
                {
                    auto& lv = layers[(size_t) li];
                    if (! lv.on)
                        continue;
                    const auto& ls = s.layers[(size_t) li];
                    float ll = 0.0f, lr = 0.0f;
                    if (renderLayer (lv, ls, ll, lr))
                        anySound = true;
                    l += ll;
                    r += lr;
                }

                if (filterOn)
                {
                    if ((i & 7) == 0)
                    {
                        auto oct = s.envAmt * 6.0f * fenv + s.keyTrack * (pitch - 60.0f) / 12.0f;
                        auto fc = juce::jlimit (20.0f, (float) owner->sampleRate * 0.45f, s.cutoff * std::exp2 (oct));
                        filter.set (fc, 0.5f * std::pow (20.0f, s.reso), owner->sampleRate);
                    }
                    l = filter.process (0, l, s.filterType);
                    r = filter.process (1, r, s.filterType);
                }

                auto a = ampEnv.getNextSample() * gain;
                outL[i] += l * a;
                outR[i] += r * a;

                if (! ampEnv.isActive() || ! anySound)
                {
                    kill();
                    return;
                }
            }
        }

    private:
        void startLayer (int l)
        {
            const auto& s = owner->settings;
            const auto& ls = s.layers[(size_t) l];
            auto& lv = layers[(size_t) l];

            lv.on = ls.on;
            lv.sample = nullptr;
            if (! lv.on)
                return;

            auto useSample = ls.wave == Wave::sample;
            if (useSample)
            {
                lv.sample = owner->active[(size_t) l];
                if (lv.sample == nullptr || lv.sample->audio.getNumSamples() < 4)
                {
                    lv.on = false;
                    lv.sample = nullptr;
                    return;
                }
            }

            lv.count = ls.wave == Wave::supersaw ? juce::jmax (7, ls.unison)
                     : ls.wave == Wave::noise ? 1
                     : juce::jlimit (1, maxUnison, ls.unison);
            lv.count = juce::jmin (lv.count, maxUnison);

            if (useSample)
            {
                auto len = (double) lv.sample->audio.getNumSamples();
                auto a = juce::jlimit (0.0, 1.0, (double) juce::jmin (ls.start, ls.end));
                auto b = juce::jlimit (0.0, 1.0, (double) juce::jmax (ls.start, ls.end));
                if (b - a < 0.002)
                    b = juce::jmin (1.0, a + 0.002);
                lv.regionStart = a * (len - 1.0);
                lv.regionEnd = b * (len - 1.0);
            }

            for (int k = 0; k < lv.count; ++k)
            {
                auto& sub = lv.subs[(size_t) k];
                sub.done = false;
                sub.phase = lv.count > 1 || ls.wave == Wave::supersaw ? owner->random.nextDouble() : 0.0;
                sub.pos = ls.reverse ? lv.regionEnd : lv.regionStart;
                // One grain at full level from the start (keeps the attack), one silent.
                sub.grains[0] = { sub.pos, 0.0, 0.5 };
                sub.grains[1] = { sub.pos, 0.0, 0.0 };

                auto t = unisonPosition (k, lv.count);
                auto p = juce::jlimit (-1.0f, 1.0f, ls.pan + t * ls.spread);
                auto angle = (p + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                auto norm = 1.0f / std::sqrt ((float) lv.count);
                sub.gl = std::cos (angle) * norm * 1.41f;
                sub.gr = std::sin (angle) * norm * 1.41f;
            }
        }

        static float unisonPosition (int k, int count)
        {
            return count <= 1 ? 0.0f : 2.0f * (float) k / (float) (count - 1) - 1.0f;
        }

        void updateControl()
        {
            const auto& s = owner->settings;
            auto sr = owner->sampleRate;

            if (s.glideMs > 0.5f)
            {
                auto coef = 1.0f - std::exp (-(float) controlInterval / (s.glideMs * 0.001f * (float) sr));
                pitch += (target - pitch) * coef;
            }
            else
            {
                pitch = target;
            }

            filterOn = s.cutoff < 19900.0f || s.filterType != 0 || s.envAmt != 0.0f;

            for (int l = 0; l < numLayers; ++l)
            {
                auto& lv = layers[(size_t) l];
                if (! lv.on)
                    continue;
                const auto& ls = s.layers[(size_t) l];
                auto base = pitch + (float) (s.transpose + ls.octave * 12 + ls.semi) + ls.fine * 0.01f;

                if (ls.wave == Wave::sample)
                {
                    auto& smp = *lv.sample;
                    auto srcRatio = smp.sampleRate / sr;
                    lv.pitchRate = std::exp2 ((base - (float) smp.rootNote) / 12.0) * srcRatio;
                    auto region = lv.regionEnd - lv.regionStart;
                    lv.headRate = ls.warp == Warp::stretch ? ls.speed * srcRatio
                                : ls.warp == Warp::synced  ? region / (ls.syncBeats * 60.0 / juce::jmax (20.0, s.bpm) * sr)
                                : 0.0;
                    lv.grainLen = juce::jmax (64.0, ls.grainMs * 0.001 * sr);
                }

                for (int k = 0; k < lv.count; ++k)
                {
                    auto cents = unisonPosition (k, lv.count) * ls.detune;
                    if (ls.wave == Wave::supersaw)
                        cents *= 1.0f + 0.5f * std::abs (unisonPosition (k, lv.count)); // wider at the edges
                    auto ratio = std::exp2 (cents / 1200.0f);
                    auto& sub = lv.subs[(size_t) k];
                    if (ls.wave == Wave::sample)
                        sub.rate = lv.pitchRate * ratio;
                    else
                        sub.inc = juce::jmin (0.45, 440.0 * std::exp2 ((base - 69.0) / 12.0) * ratio / sr);
                }
            }
        }

        bool renderLayer (LayerVoice& lv, const Layer& ls, float& l, float& r)
        {
            auto gainBase = ls.level * (ls.wave == Wave::sample ? 0.5f : 0.25f);
            bool sounding = false;

            if (ls.wave == Wave::noise)
            {
                auto v = owner->random.nextFloat() * 2.0f - 1.0f;
                l += v * gainBase * lv.subs[0].gl;
                r += v * gainBase * lv.subs[0].gr;
                return true;
            }

            for (int k = 0; k < lv.count; ++k)
            {
                auto& sub = lv.subs[(size_t) k];
                if (sub.done)
                    continue;

                float v = 0.0f;
                if (ls.wave == Wave::sample)
                {
                    v = ls.warp == Warp::repitch ? sampleRepitch (lv, ls, sub) : sampleStretch (lv, ls, sub);
                    if (sub.done)
                        continue;
                }
                else
                {
                    v = oscillator (ls.wave, sub);
                }

                sounding = true;
                l += v * sub.gl * gainBase;
                r += v * sub.gr * gainBase;
            }
            return sounding;
        }

        static double blep (double t, double dt)
        {
            if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
            if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
            return 0.0;
        }

        static float oscillator (Wave w, Sub& sub)
        {
            auto ph = sub.phase, dt = sub.inc;
            double v;
            switch (w)
            {
                case Wave::square:
                {
                    auto ph2 = ph + 0.5;
                    ph2 -= std::floor (ph2);
                    v = (ph < 0.5 ? 1.0 : -1.0) + blep (ph, dt) - blep (ph2, dt);
                    v *= 0.8;
                    break;
                }
                case Wave::triangle: v = 4.0 * std::abs (ph - 0.5) - 1.0; break;
                case Wave::sine:     v = std::sin (juce::MathConstants<double>::twoPi * ph); break;
                case Wave::saw:
                case Wave::supersaw:
                case Wave::noise:    // handled by the caller
                case Wave::sample:
                default:             v = 2.0 * ph - 1.0 - blep (ph, dt); break;
            }
            sub.phase += dt;
            if (sub.phase >= 1.0)
                sub.phase -= 1.0;
            return (float) v;
        }

        // 4-point Hermite read. Outside the region it wraps (looping) or is silent.
        float read (const LayerVoice& lv, bool loop, double pos, int ch) const
        {
            const auto& audio = lv.sample->audio;
            auto len = audio.getNumSamples();
            auto* d = audio.getReadPointer (juce::jmin (ch, audio.getNumChannels() - 1));
            auto regionLen = lv.regionEnd - lv.regionStart;

            if (loop && regionLen > 1.0)
            {
                pos = lv.regionStart + std::fmod (pos - lv.regionStart, regionLen);
                if (pos < lv.regionStart)
                    pos += regionLen;
            }

            auto i = (int) std::floor (pos);
            auto f = (float) (pos - i);
            auto at = [&] (int j) { return j >= 0 && j < len ? d[j] : 0.0f; };
            auto xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            auto c1 = 0.5f * (x1 - xm1);
            auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }

        float readMono (const LayerVoice& lv, bool loop, double pos) const
        {
            return lv.sample->audio.getNumChannels() > 1 ? 0.5f * (read (lv, loop, pos, 0) + read (lv, loop, pos, 1))
                                                         : read (lv, loop, pos, 0);
        }

        // Keeps a playhead inside the region: wraps when looping, else ends the sub.
        bool advance (const LayerVoice& lv, const Layer& ls, Sub& sub, double step)
        {
            sub.pos += ls.reverse ? -step : step;
            auto regionLen = lv.regionEnd - lv.regionStart;
            if (sub.pos > lv.regionEnd || sub.pos < lv.regionStart)
            {
                if (! ls.loop)
                {
                    sub.done = true;
                    return false;
                }
                sub.pos += ls.reverse ? regionLen : -regionLen;
            }
            return true;
        }

        float sampleRepitch (const LayerVoice& lv, const Layer& ls, Sub& sub)
        {
            auto v = readMono (lv, ls.loop, sub.pos);
            advance (lv, ls, sub, sub.rate);
            return v;
        }

        // Granular time-stretch: the playhead moves at the warp speed while two
        // overlapping Hann-windowed grains read the audio at the note's pitch.
        float sampleStretch (const LayerVoice& lv, const Layer& ls, Sub& sub)
        {
            auto dir = ls.reverse ? -1.0 : 1.0;
            auto pitchRatio = sub.rate;
            float v = 0.0f;

            for (auto& g : sub.grains)
            {
                auto w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) g.phase);
                if (w > 0.0001f)
                    v += w * readMono (lv, ls.loop, g.src + g.offset);

                g.offset += pitchRatio * dir;
                g.phase += 1.0 / lv.grainLen;
                if (g.phase >= 1.0)
                {
                    g.phase -= 1.0;
                    g.src = sub.pos;
                    g.offset = 0.0;
                }
            }

            advance (lv, ls, sub, lv.headRate);
            return v;
        }

        static constexpr int controlInterval = 16;

        std::array<LayerVoice, numLayers> layers;
        juce::ADSR ampEnv, fltEnv;
        Svf filter;
        float pitch = 60.0f, target = 60.0f, gain = 1.0f;
        int controlCountdown = 0;
        bool activeFlag = false, filterOn = false;
    };

    //==========================================================================
    void handle (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
            noteOn (m.getChannel(), m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())
            noteOff (m.getChannel(), m.getNoteNumber());
        else if (m.isSustainPedalOn())
            sustainDown = true;
        else if (m.isSustainPedalOff())
        {
            sustainDown = false;
            for (auto& v : voices)
                if (v.sustained)
                {
                    v.sustained = false;
                    v.release();
                }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            allOff (m.isAllSoundOff());
    }

    void noteOn (int channel, int note, float velocity)
    {
        auto glideOn = settings.glideMs > 0.5f;

        if (settings.mode != VoiceMode::poly)
        {
            pushHeld (note, channel);
            auto& v = voices[0];
            for (int i = 1; i < maxVoices; ++i)
                voices[(size_t) i].kill();

            if (v.isActive() && ! v.released && settings.mode == VoiceMode::legato)
                v.slideTo (note);
            else
                v.start (note, channel, velocity, v.isActive() ? v.currentPitch() : lastPitch, glideOn && (v.isActive() || hasPlayed));
            lastPitch = (float) note;
            hasPlayed = true;
            return;
        }

        Voice* chosen = nullptr;
        for (auto& v : voices)
            if (v.isActive() && v.note == note && v.channel == channel)
                chosen = &v; // retrigger the same key

        if (chosen == nullptr)
            for (auto& v : voices)
                if (! v.isActive()) { chosen = &v; break; }

        if (chosen == nullptr)
        {
            // Steal the oldest, preferring ones already releasing.
            for (auto pass = 0; pass < 2 && chosen == nullptr; ++pass)
                for (auto& v : voices)
                    if ((pass == 1 || v.released) && (chosen == nullptr || v.age < chosen->age))
                        chosen = &v;
        }

        chosen->start (note, channel, velocity, lastPitch, glideOn && hasPlayed);
        lastPitch = (float) note;
        hasPlayed = true;
    }

    void noteOff (int channel, int note)
    {
        if (settings.mode != VoiceMode::poly)
        {
            removeHeld (note, channel);
            auto& v = voices[0];
            if (! v.isActive() || v.note != note)
                return;
            if (heldCount > 0)
            {
                auto& back = held[(size_t) heldCount - 1];
                v.slideTo (back.first);
                v.channel = back.second;
                lastPitch = (float) back.first;
            }
            else
            {
                v.release();
            }
            return;
        }

        for (auto& v : voices)
            if (v.isActive() && ! v.released && v.note == note && v.channel == channel)
            {
                if (sustainDown)
                    v.sustained = true;
                else
                    v.release();
            }
    }

    void pushHeld (int note, int channel)
    {
        removeHeld (note, channel);
        if (heldCount < (int) held.size())
            held[(size_t) heldCount++] = { note, channel };
    }

    void removeHeld (int note, int channel)
    {
        for (int i = 0; i < heldCount; ++i)
            if (held[(size_t) i].first == note && held[(size_t) i].second == channel)
            {
                for (int j = i; j < heldCount - 1; ++j)
                    held[(size_t) j] = held[(size_t) j + 1];
                --heldCount;
                return;
            }
    }

    void renderVoices (int start, int num)
    {
        if (num <= 0)
            return;
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        for (auto& v : voices)
            if (v.isActive())
                v.render (l, r, start, num);
    }

    void publishPlayheads()
    {
        const Voice* newest = nullptr;
        int count = 0;
        for (auto& v : voices)
            if (v.isActive())
            {
                ++count;
                if (newest == nullptr || v.age > newest->age)
                    newest = &v;
            }

        activeVoices.store (count);
        for (int l = 0; l < numLayers; ++l)
            playhead[(size_t) l].store (newest != nullptr ? newest->layerPosition (l) : -1.0f);
    }

    double sampleRate = 44100.0;
    Settings settings;
    std::array<Voice, maxVoices> voices;
    juce::AudioBuffer<float> buffer;
    juce::Random random;
    juce::uint32 ageCounter = 0;
    float lastPitch = 60.0f;
    bool hasPlayed = false, sustainDown = false;
    std::array<std::pair<int, int>, 32> held {};
    int heldCount = 0;

    mutable juce::SpinLock sampleLock;
    std::array<Sample::Ptr, numLayers> current;   // set by the message thread
    std::array<Sample::Ptr, numLayers> active;    // the audio thread's copies
    juce::ReferenceCountedArray<Sample> retired;
};

} // namespace rf
