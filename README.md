# The Paul 50 Bag — plugin

VST3 / AU / Standalone version of The Paul 50 Bag (birthday instrument for Paul Tas).

- `Source/Engine.h` — the sound engine, a C++ port of the web AudioWorklet (framework-free).
- `Source/PluginProcessor.*` — JUCE processor; speaks the same message protocol as the web page.
- `Source/PluginEditor.*` — the editor is the web page itself (WebView), served from `ui/`.
- `ui/index.html` — built from the web app (plugin mode is detected via `window.__JUCE__`).

Build (JUCE 8): `git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git JUCE`, then
`cmake -B build && cmake --build build --config Release`. CI builds Mac (AU/VST3, universal) and Windows (VST3).
