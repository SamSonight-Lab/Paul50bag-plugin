#include "PluginProcessor.h"
#ifndef P50_NO_EDITOR
#include "PluginEditor.h"
#endif

Paul50BagProcessor::Paul50BagProcessor()
  : AudioProcessor (BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true)
      .withOutput ("Bass", juce::AudioChannelSet::stereo(), false).withOutput ("Lead", juce::AudioChannelSet::stereo(), false)
      .withOutput ("Drums", juce::AudioChannelSet::stereo(), false).withOutput ("Pad", juce::AudioChannelSet::stereo(), false)
      .withOutput ("FX", juce::AudioChannelSet::stereo(), false)) {
  band.prepare (48000); }

bool Paul50BagProcessor::isBusesLayoutSupported (const BusesLayout& l) const {
  if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
  for (int i = 1; i < l.outputBuses.size(); ++i) { auto c = l.outputBuses[i]; if (! c.isDisabled() && c != juce::AudioChannelSet::stereo()) return false; }
  return l.inputBuses.isEmpty(); }

void Paul50BagProcessor::prepareToPlay (double sampleRate, int block) {
  for (auto& a : scratch) for (auto& v : a) v.assign ((size_t) std::max (block, 4096), 0.f);
  const juce::SpinLock::ScopedLockType sl (lock);
  if (! prepared || std::abs (sampleRate - sr) > 1) {
    // keep the knobs/state, rebuild the buffers for the new rate
    p50::Band fresh; fresh.prepare (sampleRate);
    std::memcpy (fresh.K, band.K, sizeof band.K); std::memcpy (fresh.KS, band.K, sizeof band.K);
    std::memcpy (fresh.E, band.E, sizeof band.E); std::memcpy (fresh.S, band.S, sizeof band.S); std::memcpy (fresh.C, band.C, sizeof band.C);
    fresh.CG = band.CG; fresh.DR = band.DR; fresh.M = band.M; fresh.X = band.X; fresh.freeze = band.freeze; fresh.AL = band.AL;
    for (int r = 0; r < 5; ++r) for (int j = 0; j < 10; ++j) fresh.A[r][j] = band.A[r][j];
    for (int i = 0; i < 7; ++i) fresh.setSample (i, band.SM[i]);
    band = std::move (fresh); }
  sr = sampleRate; prepared = true; }

static double TOF (double b) { return b <= 0 ? 0 : b <= 300 ? .03 + .77 * std::max (0.0, std::log (b / 10) / std::log (30.0)) : std::min (1.0, .8 + .2 * std::log (b / 300) / std::log (200.0)); }

void Paul50BagProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals nd;
  const int n = buf.getNumSamples(); buf.clear();
  bool play = true; double hostBpm = 0;
  if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) {
    if (auto b = pos->getBpm()) hostBpm = *b;
    if (! juce::JUCEApplicationBase::isStandaloneApp()) play = pos->getIsPlaying() || ! pos->getTimeInSeconds().hasValue(); }
  const juce::SpinLock::ScopedTryLockType sl (lock);
  if (! sl.isLocked()) return;
  // MIDI: a note sets the key of the whole band
  for (const auto m : midi) { auto msg = m.getMessage(); if (msg.isNoteOn()) band.M.key = (msg.getNoteNumber() % 12) / 11.0; }
  if (hostBpm > 0 && followHost && ! juce::JUCEApplicationBase::isStandaloneApp()) band.M.tempo = TOF (hostBpm);
  if (! play) return;
  auto main = getBusBuffer (buf, false, 0);
  float* L = main.getWritePointer (0); float* R = main.getNumChannels() > 1 ? main.getWritePointer (1) : L;
  // stems: write straight into enabled aux buses (or into scratch so the engine always has somewhere to write)
  bool anyStem = false;
  for (int k = 0; k < 5; ++k) { band.stem[k][0] = band.stem[k][1] = nullptr;
    if (k + 1 < getBusCount (false) && getBus (false, k + 1)->isEnabled()) { auto b = getBusBuffer (buf, false, k + 1);
      if (b.getNumChannels() >= 2) { band.stem[k][0] = b.getWritePointer (0); band.stem[k][1] = b.getWritePointer (1); anyStem = true; } } }
  if (anyStem) for (int k = 0; k < 5; ++k) if (! band.stem[k][0]) { if ((int) scratch[k][0].size() < n) { band.stem[k][0] = nullptr; continue; } band.stem[k][0] = scratch[k][0].data(); band.stem[k][1] = scratch[k][1].data(); }
  if (! band.stem[0][0]) for (auto& row : band.stem) row[0] = row[1] = nullptr;
  band.process (L, R, n);
  for (int i = 0; i < 5; ++i) if (band.flash[i]) { flashAcc[i] = 1; band.flash[i] = 0; } }

// ---------- messages ----------
static int slotIndex (const juce::String& s) { if (s == "b") return 0; if (s == "l") return 1; if (s == "p") return 2; if (s == "dk") return 3; if (s == "ds") return 4; if (s == "dh") return 5; if (s == "dt") return 6; return -1; }
static double num (const juce::var& v, double def = 0) { return v.isVoid() || v.isUndefined() ? def : (double) v; }

void Paul50BagProcessor::handleMessage (const juce::var& d) {
  if (! d.isObject()) return;
  // build heavy things (samples) before taking the lock
  p50::SampleP smp; int smpSlot = -1;
  if (d.hasProperty ("smp")) { auto m = d["smp"]; smpSlot = slotIndex (m["slot"].toString()); auto s = std::make_shared<p50::Sample>();
    if (auto* arr = m["d"].getArray()) { s->d.reserve ((size_t) arr->size()); for (auto& x : *arr) s->d.push_back ((float) (double) x); }
    s->rf = num (m["rf"], 440); s->loop = (int) num (m["loop"]) != 0; smp = s; }
  const juce::SpinLock::ScopedLockType sl (lock);
  auto& b = band;
  if (auto* K = d["K"].getArray()) for (int r = 0; r < 5 && r < K->size(); ++r) if (auto* row = (*K)[r].getArray()) for (int j = 0; j < 10 && j < row->size(); ++j) b.K[r][j] = (double) (*row)[j];
  if (d.hasProperty ("r")) { int r = (int) d["r"], j = (int) d["j"]; if (r >= 0 && r < 5 && j >= 0 && j < 10) b.K[r][j] = (double) d["v"]; }
  if (auto* E = d["E"].getArray()) for (int i = 0; i < 5 && i < E->size(); ++i) b.E[i] = (int) (*E)[i];
  if (auto* S = d["S"].getArray()) for (int i = 0; i < 5 && i < S->size(); ++i) b.S[i] = (int) (*S)[i];
  if (d["M"].isObject()) { auto M = d["M"]; auto set = [&] (const char* k, double& dst) { if (M.hasProperty (k)) dst = (double) M[k]; };
    set ("tempo", b.M.tempo); set ("key", b.M.key); set ("scale", b.M.scale); set ("glue", b.M.glue); set ("vol", b.M.vol); set ("dirt", b.M.dirt); set ("prog", b.M.prog); set ("arr", b.M.arr);
    if (M.hasProperty ("ring")) b.M.ring = (int) num (M["ring"]); }
  if (d.hasProperty ("freeze")) b.freeze = (int) num (d["freeze"]);
  if (d["X"].isObject()) { auto X = d["X"]; auto arr = [&] (const char* k, double* dst, int n) { if (auto* a = X[k].getArray()) for (int i = 0; i < n && i < a->size(); ++i) dst[i] = (double) (*a)[i]; };
    auto arri = [&] (const char* k, int* dst, int n) { if (auto* a = X[k].getArray()) for (int i = 0; i < n && i < a->size(); ++i) dst[i] = (int) num ((*a)[i]); };
    arr ("g", b.X.g, 4); arri ("mu", b.X.mu, 4); arr ("pa", b.X.pa, 3); arr ("pb", b.X.pb, 3); arr ("dm", b.X.dm, 4); arri ("dmu", b.X.dmu, 4);
    if (X.hasProperty ("rv")) b.X.rv = (double) X["rv"]; if (X.hasProperty ("dl")) b.X.dl = (double) X["dl"]; if (X.hasProperty ("rm")) b.X.rm = (double) X["rm"]; if (X.hasProperty ("fb")) b.X.fb = (double) X["fb"]; }
  if (auto* C = d["C"].getArray()) for (int i = 0; i < 8 && i < C->size(); ++i) b.C[i] = (double) (*C)[i];
  if (d.hasProperty ("CG")) b.CG = (double) d["CG"];
  if (d.hasProperty ("DR")) b.DR = (double) d["DR"];
  if (d.hasProperty ("song")) b.startSong();
  if (auto* R = d["setreg"].getArray()) for (int i = 0; i < 5 && i < R->size(); ++i) b.reg[i] = (int) (*R)[i];
  if (smpSlot >= 0) b.setSample (smpSlot, smp);
  auto rj = [] (const juce::String& k, int& r, int& j) { r = k.upToFirstOccurrenceOf ("-", false, false).getIntValue(); j = k.fromFirstOccurrenceOf ("-", false, false).getIntValue(); return r >= 0 && r < 5 && j >= 0 && j < 10; };
  int r, j;
  if (d.hasProperty ("hold") && rj (d["hold"].toString(), r, j)) b.held[r][j] = true;
  if (d.hasProperty ("unhold") && rj (d["unhold"].toString(), r, j)) b.held[r][j] = false;
  if (d.hasProperty ("reseed")) { int q = (int) d["reseed"]; if (q >= 0 && q < 5) b.reseed (q); }
  if (d.hasProperty ("aclear")) for (auto& row : b.A) for (auto& L : row) { L.on = false; L.a.clear(); }
  if (d.hasProperty ("alen")) b.AL = std::max (1, (int) d["alen"]);
  if (auto* o = d["auto"].getDynamicObject()) for (auto& p : o->getProperties()) if (rj (p.name.toString(), r, j)) {
    auto& L = b.A[r][j]; if (auto* a = p.value.getArray()) { L.a.clear(); for (auto& x : *a) L.a.push_back ((float) (double) x); L.on = ! L.a.empty(); } else { L.on = false; L.a.clear(); } } }

void Paul50BagProcessor::applyFullState (const juce::String& json) {
  auto st = juce::JSON::parse (json); if (! st.isObject()) return;
  juce::DynamicObject::Ptr m = new juce::DynamicObject();
  for (auto k : { "K", "E", "S", "M", "X", "C", "CG", "DR", "freeze" }) if (st.hasProperty (k)) m->setProperty (k, st[k]);
  m->setProperty ("aclear", 1); if (st.hasProperty ("A")) m->setProperty ("auto", st["A"]); if (st.hasProperty ("AL")) m->setProperty ("alen", (int) num (st["AL"], 2) * 16);
  handleMessage (juce::var (m.get())); }

juce::var Paul50BagProcessor::engineSnapshot() {
  juce::DynamicObject::Ptr o = new juce::DynamicObject(); juce::Array<juce::var> regs, fl, dro;
  { const juce::SpinLock::ScopedLockType sl (lock);
    for (int i = 0; i < 5; ++i) { regs.add (band.reg[i]); fl.add (flashAcc[i]); flashAcc[i] = 0; }
    if (band.DR > .001 || band.dro[1] != 0) for (int i = 0; i < 50; ++i) dro.add (band.dro[i]);
    o->setProperty ("step", (double) band.step); o->setProperty ("apos", band.apos); o->setProperty ("lvl", band.cmp); }
  o->setProperty ("regs", regs); o->setProperty ("fl", fl); if (! dro.isEmpty()) o->setProperty ("dro", dro);
  return juce::var (o.get()); }

void Paul50BagProcessor::getStateInformation (juce::MemoryBlock& dest) {
  juce::ValueTree t ("P50"); t.setProperty ("ui", uiState, nullptr); t.setProperty ("v", 1, nullptr);
  juce::MemoryOutputStream os (dest, false); t.writeToStream (os); }
void Paul50BagProcessor::setStateInformation (const void* data, int size) {
  auto t = juce::ValueTree::readFromData (data, (size_t) size); if (! t.isValid()) return;
  uiState = t["ui"].toString(); if (uiState.isNotEmpty()) applyFullState (uiState); }

#ifndef P50_NO_EDITOR
juce::AudioProcessorEditor* Paul50BagProcessor::createEditor() { return new Paul50BagEditor (*this); }
#else
juce::AudioProcessorEditor* Paul50BagProcessor::createEditor() { return nullptr; }
#endif
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Paul50BagProcessor(); }
