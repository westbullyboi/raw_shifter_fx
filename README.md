# RAW SHIFTER FX

An audio-domain step-sequenced glitch/mangle plugin, in the spirit of
Sugar Bytes' Effectrix: a 16-step pattern where each step can arm any
combination of seven effect lanes - Gate, Pan, Filter, Bitcrush, Repeat,
Reverse, Echo - that mangle whatever audio lands in that step, tempo/host
transport synced, while leaving unarmed steps untouched.

This project shares its technical architecture (JUCE + CMake, a
framework-independent `Source/DSP` core, custom-painted `Source/UI`,
dependency-free `Tests/`, GitHub Actions CI) with its sibling project,
[`raw_swing_vst`](https://github.com/westbullyboi/raw_swing_vst) ("RAW
SWING FX").

## Features

* Stereo (or mono) audio effect, DAW tempo/transport sync (`juce::AudioPlayHead`)
* A 16-step pattern, tempo-synced via a selectable **Rate** (1/32, 1/16, 1/8)
* Seven click-to-arm effect **lanes**, layerable per step:
  * **Gate** - ducks the step towards silence (Amount = how hard), with a
    short click-free ramp at both edges of the step
  * **Pan** - collapses the step to its mono sum and hard-pans it,
    alternating left/right by step parity (Amount = how hard)
  * **Filter** - a resonant low-pass that sweeps closed across the step
    (Amount = how far/resonant)
  * **Bitcrush** - sample-and-hold downsampling + bit-depth reduction
    (Amount = how crushed)
  * **Repeat** - loops a `1/Amount`-sized fragment from the step's start,
    the classic stutter/beat-repeat glitch
  * **Reverse** - plays the step's own audio back in reverse (binary; no
    Amount knob)
  * **Echo** - throws the step into a short tempo-locked feedback delay
    that keeps ringing out after the step ends (Amount = feedback/decay)
* **Mix** and **Output** controls
* A custom-painted step-grid GUI with a live playhead highlight
* Correctly-reported, tempo-adaptive plugin latency; zero heap allocation
  inside the audio callback

## Architecture

```
Source/
  PluginProcessor.*   JUCE AudioProcessor glue: parameters, playhead, latency reporting
  PluginEditor.*       Custom-painted step-grid GUI
  DSP/                 Framework-independent DSP core (no JUCE includes)
    RingBuffer.h           Power-of-two circular buffer primitive
    TransportInfo.h         Plain POD snapshot of host transport state
    StepClock.h              PPQ-based step-sequencer beat clock (Rate, pattern position)
    StepRemap.h               Pure delay-math for the Reverse/Repeat time-remap lanes
    GateEnvelope.h             Pure per-sample gain function for the Gate lane
    PanLaw.h                    Pure hard-pan function for the Pan lane
    StepFilter.h                 Resonant sweeping low-pass for the Filter lane
    Bitcrusher.h                  Sample-and-hold + bit-quantizer for the Bitcrush lane
    EchoEffect.h                  Feedback delay "throw" for the Echo lane
    ShifterEngine.*                Orchestrates everything above
  Utility/              Math/Constants helpers (no JUCE, no state)
  UI/                   Custom-painted GUI components
    Theme.h                Colour palette + font helpers
    AmountKnob.h            Small rotary control bound directly to a parameter
    StepGridView.h           The lane x step toggle grid + playhead
Tests/                 Dependency-free unit tests for the DSP core (no JUCE required)
```

## The step-sequencing latency model

Every channel's input is continuously recorded into a `RingBuffer`
(`ShifterEngine`'s `recordBuffer`), and output is emitted a constant
`currentStepLenSamples` (one whole step) behind input. That delay is
chosen so that, by the moment output *starts* a given step, that step's
own audio has just finished being recorded in full - which is what lets
the Reverse and Repeat lanes rearrange a step's own samples (samples that
would not exist yet under a naive zero-latency design) while the plugin
still reports one single, constant, tempo-adaptive latency to the host.
This mirrors the "shared baseline delay" trick `raw_swing_vst`'s
`DelayEngine` uses for its own bipolar swing-shift range.

Dry and wet are both read/computed at that same constant delay, so the
**Mix** knob never introduces comb filtering: an unarmed step's wet path
already equals its dry path exactly.

`RecordBuffer` is sized (in `ShifterEngine::prepare`) against a worst-case
step length - the slowest supported tempo (`kMinSupportedBPM`) at the
longest selectable Rate (1/8) - with headroom for Reverse/Repeat's
up-to-`2x` internal read range, so nothing is ever reallocated in the
audio callback; only the *active* step length within that fixed capacity
changes with tempo/Rate.

### Which step's pattern flags a given output sample uses

Because output lags input by exactly one whole step, the live transport
clock (`StepClock`) is always one step *ahead* of what is currently being
emitted: at the same audio-callback iteration that records live input for
step `M`, the plugin is emitting delayed output that belongs to step
`M - 1`. `ShifterEngine::process` computes this explicitly
(`outputAbsoluteStep = stepClock.getAbsoluteStepIndex() - 1`) so the
pattern lookup and the Pan lane's left/right parity both use the step that
actually owns the audio being emitted, not whatever step the live
transport happens to be recording into right now.

## Building

### Plugin (VST3 + Standalone, AU on macOS)

Requires network access on first configure (JUCE is fetched via CMake
`FetchContent`). On Linux you'll also need X11/ALSA/GL development
headers (`libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev
libxext-dev libasound2-dev libfreetype-dev libfontconfig1-dev
libcurl4-openssl-dev libgl1-mesa-dev` on Debian/Ubuntu).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# on macOS, build a universal binary unless you only need your own machine's architecture:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build build --target ShifterFX_VST3 --parallel
# or: --target ShifterFX_Standalone
# on macOS only: --target ShifterFX_AU   (required for Logic Pro / GarageBand)
```

Built artifacts land under `build/ShifterFX_artefacts/Release/<FORMAT>/`
as `RAW SHIFTER FX.vst3` / `RAW SHIFTER FX.component` / the `RAW SHIFTER
FX` standalone app - the CMake *target* is `ShifterFX`, the shipped
product name is `RAW SHIFTER FX`.

### DSP unit tests (no JUCE required)

```sh
cmake -S . -B build-tests -DSHIFTERFX_BUILD_PLUGIN=OFF -DSHIFTERFX_BUILD_TESTS=ON
cmake --build build-tests --parallel
./build-tests/Tests/ShifterFX_Tests
```

### Prebuilt binaries via GitHub Actions

`.github/workflows/build.yml` builds VST3 for Linux/macOS/Windows and AU
for macOS on every push/PR, and uploads each as a downloadable workflow
artifact. See `raw_swing_vst`'s README for the detailed unzip/quarantine
instructions - the packaging story (staged VST3 folder on Linux/Windows,
`ditto`-zipped signed bundles on macOS, `xattr -dr com.apple.quarantine`
after download) is identical here.

## Known limitations / future work

* A mid-step tempo change is not crossfaded - `currentStepLenSamples` is
  only re-latched once per block (gated by `kTempoChangeThresholdBPM`,
  matching `raw_swing_vst`'s own approach), so an abrupt tempo change
  landing inside an already-armed step's playback can click. Tempo
  changes between blocks/steps are unaffected.
* The Filter lane bypasses entirely (no processing at all) on unarmed
  steps to guarantee true passthrough there; reactivating it on the next
  armed step always starts from a freshly-converging filter state rather
  than a continuously-tracked one, which can produce a brief "filter
  turn-on" transient - a minor, deliberate trade-off in favour of exact
  bypass transparency.
* No preset/pattern browser yet (patterns are plain host-automatable
  state, saved/restored via the usual `AudioProcessorValueTreeState` XML,
  but there is no curated bank of starting patterns like `raw_swing_vst`'s
  per-target preset list).
* The seven lanes are original, documented approximations of the classic
  step-sequenced glitch effects they take inspiration from (Effectrix and
  similar), not reverse-engineered or bit-accurate reproductions of any
  specific product's algorithms.
