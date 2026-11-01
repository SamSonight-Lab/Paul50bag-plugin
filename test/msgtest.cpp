#include "../Source/PluginProcessor.h"
#include <cstdio>
int main (int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI init;
  Paul50BagProcessor p; p.prepareToPlay (48000, 512);
  auto msgs = juce::JSON::parse (juce::File (argv[1]).loadFileAsString());
  for (auto& m : *msgs.getArray()) p.handleMessage (m);
  juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi; double s2 = 0, pk = 0; int T = 0;
  for (int i = 0; i < 48000 * 12 / 512; ++i) { p.processBlock (buf, midi); for (int j = 0; j < 512; ++j) { float v = buf.getSample (0, j); s2 += v * v; pk = std::max (pk, (double) std::abs (v)); ++T; } }
  printf ("rms %.3f pk %.2f\n", std::sqrt (s2 / T), pk);
  auto snap = p.engineSnapshot(); printf ("%s\n", juce::JSON::toString (snap, true).toRawUTF8());
  // state round trip
  juce::MemoryBlock mb; p.uiState = "{\"K\":[[1,0,0,0,0,0,0,0,0,1]],\"M\":{\"tempo\":0.5}}"; p.getStateInformation (mb); Paul50BagProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize()); printf ("state ok %d\n", q.uiState == p.uiState);
  return 0; }
