#include "PluginEditor.h"
#include "BinaryData.h"

static std::vector<std::byte> bytesOf (const char* data, int size) { std::vector<std::byte> v ((size_t) size); std::memcpy (v.data(), data, (size_t) size); return v; }

std::optional<juce::WebBrowserComponent::Resource> Paul50BagEditor::resource (const juce::String& url) {
  auto path = url.fromFirstOccurrenceOf ("/", false, false).upToFirstOccurrenceOf ("?", false, false);
  if (path.isEmpty() || path == "index.html") return juce::WebBrowserComponent::Resource { bytesOf (BinaryData::index_html, BinaryData::index_htmlSize), "text/html" };
  auto file = path.fromLastOccurrenceOf ("/", false, false);
  for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    if (juce::String (BinaryData::originalFilenames[i]) == file) { int sz = 0; auto* d = BinaryData::getNamedResource (BinaryData::namedResourceList[i], sz);
      return juce::WebBrowserComponent::Resource { bytesOf (d, sz), file.endsWith (".woff2") ? "font/woff2" : "application/octet-stream" }; }
  return std::nullopt; }

juce::WebBrowserComponent::Options Paul50BagEditor::makeOptions() {
  juce::String init = "window.__p50sr=" + juce::String (proc.currentSampleRate()) + ";";
  if (proc.uiState.isNotEmpty()) init += "window.__p50state=" + juce::JSON::toString (juce::var (proc.uiState)) + ";";
  return juce::WebBrowserComponent::Options {}
    .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
    .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}.withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Paul50Bag")))
    .withNativeIntegrationEnabled()
    .withKeepPageLoadedWhenBrowserIsHidden()
    .withUserScript (init)
    .withResourceProvider ([] (const auto& url) { return resource (url); })
    .withEventListener ("msg", [this] (juce::var d) { proc.handleMessage (d); })
    .withEventListener ("state", [this] (juce::var s) { proc.uiState = s.toString(); }); }

Paul50BagEditor::Paul50BagEditor (Paul50BagProcessor& p) : AudioProcessorEditor (p), proc (p), web (makeOptions()) {
  addAndMakeVisible (web);
  web.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
  setResizable (true, true); setResizeLimits (360, 640, 900, 1600); setSize (430, 900);
  startTimerHz (30); }
