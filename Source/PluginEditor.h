#pragma once
#include "PluginProcessor.h"

class Paul50BagEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
  explicit Paul50BagEditor (Paul50BagProcessor&);
  ~Paul50BagEditor() override { stopTimer(); }
  void resized() override { web.setBounds (getLocalBounds()); }
private:
  void timerCallback() override { web.emitEventIfBrowserIsVisible ("eng", proc.engineSnapshot()); }
  static std::optional<juce::WebBrowserComponent::Resource> resource (const juce::String& url);
  juce::WebBrowserComponent::Options makeOptions();
  Paul50BagProcessor& proc;
  juce::WebBrowserComponent web;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Paul50BagEditor)
};
