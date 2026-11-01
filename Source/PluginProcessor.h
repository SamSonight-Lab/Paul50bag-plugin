#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"

class Paul50BagProcessor : public juce::AudioProcessor {
public:
  Paul50BagProcessor();
  void prepareToPlay (double sampleRate, int) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported (const BusesLayout& l) const override;
  void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }
  const juce::String getName() const override { return "The Paul 50 Bag"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  double getTailLengthSeconds() const override { return 2.0; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram (int) override {}
  const juce::String getProgramName (int) override { return {}; }
  void changeProgramName (int, const juce::String&) override {}
  void getStateInformation (juce::MemoryBlock&) override;
  void setStateInformation (const void*, int) override;

  // messages from the editor page (same protocol as the web AudioWorklet)
  void handleMessage (const juce::var& d);
  void applyFullState (const juce::String& json);
  juce::var engineSnapshot();
  juce::String uiState;            // the page's full state as JSON, saved with the DAW project
  double currentSampleRate() const { return sr; }
  std::atomic<bool> followHost { true };
private:
  p50::Band band;
  juce::SpinLock lock;
  double sr = 48000;
  bool prepared = false;
  int flashAcc[5] = {};
  std::vector<float> scratch[5][2];
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Paul50BagProcessor)
};
