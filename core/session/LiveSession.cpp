#include "core/session/LiveSession.h"

#include <algorithm>
#include <cmath>

#include "core/assets/ClipPreparation.h"

namespace atmos::core {

LiveSession::LiveSession(double sampleRate, int maxBlockSize) { rebuild(sampleRate, maxBlockSize); }

void LiveSession::rebuild(double sampleRate, int maxBlockSize) {
    engine_ = std::make_unique<AudioEngine>();
    applySettings(); // before prepare(): smoothers start at their targets
    engine_->prepare({sampleRate, maxBlockSize});
    if (original_) engine_->setLayer(kTrackSlot, prepareClipForEngine(*original_, sampleRate), Bus::Music, 0);
}

bool LiveSession::rebuildIfNeeded(double sampleRate, int maxBlockSize) {
    if (std::fabs(sampleRate - this->sampleRate()) < 1e-6 && maxBlockSize <= this->maxBlockSize()) return false;
    rebuild(sampleRate, std::max(maxBlockSize, this->maxBlockSize()));
    return true;
}

bool LiveSession::setTrack(std::shared_ptr<const AudioClip> original, ClipPtr prepared, double preparedRate) {
    if (!original || original->numFrames() == 0) return false;
    const double rate = sampleRate();
    if (!prepared || std::fabs(preparedRate - rate) > 1e-6) prepared = prepareClipForEngine(*original, rate);
    engine_->pause();
    engine_->seek(0);
    if (!engine_->setLayer(kTrackSlot, std::move(prepared), Bus::Music, 0)) return false;
    original_ = std::move(original);
    return true;
}

void LiveSession::clearTrack() {
    engine_->pause();
    engine_->clearLayer(kTrackSlot);
    original_.reset();
}

double LiveSession::trackDurationSeconds() const noexcept { return original_ ? original_->durationSeconds() : 0.0; }

void LiveSession::applySettings() {
    AudioEngine& e = *engine_;
    for (int b = 0; b < kNumBuses; ++b) {
        e.setBusGain(static_cast<Bus>(b), settings_.busGain[b]);
        e.setBusMuted(static_cast<Bus>(b), settings_.busMuted[b]);
    }
    e.setMasterGain(settings_.masterGain);
    e.setLimiterCeilingDb(settings_.limiterCeilingDb);
    e.setAtmosphereBedEnabled(settings_.bedEnabled);
    e.atmosphereBed().level.set(settings_.bedLevel);
    e.atmosphereBed().colourHz.set(settings_.bedColourHz);
    e.micTransform().cutoffHz.set(settings_.micToneHz);
    e.micTransform().echoSeconds.set(settings_.micEchoSeconds);
    e.micTransform().feedback.set(settings_.micFeedback);
    e.micTransform().wet.set(settings_.micWet);
    e.micTransform().shiftHz.set(settings_.micShiftHz);
    e.micTransform().shiftMix.set(settings_.micShiftMix);
    e.roomMemory().setEnabled(settings_.roomMemoryEnabled);
    e.roomMemory().level.set(settings_.roomMemoryLevel);
    e.roomMemory().pitchSemitones.store(settings_.roomMemoryPitch);
    e.roomMemory().listenerYawDeg.store(listenerYaw_);
    e.setMicInputEnabled(settings_.micEnabled);
    e.setLooping(settings_.looping);
}

void LiveSession::setBusGain(Bus b, float g) {
    if (b == Bus::Count) return;
    settings_.busGain[static_cast<int>(b)] = g;
    engine_->setBusGain(b, g);
}
void LiveSession::setBusMuted(Bus b, bool m) {
    if (b == Bus::Count) return;
    settings_.busMuted[static_cast<int>(b)] = m;
    engine_->setBusMuted(b, m);
}
void LiveSession::setMasterGain(float g) { settings_.masterGain = g; engine_->setMasterGain(g); }
void LiveSession::setLimiterCeilingDb(float db) { settings_.limiterCeilingDb = db; engine_->setLimiterCeilingDb(db); }
void LiveSession::setBedEnabled(bool e) { settings_.bedEnabled = e; engine_->setAtmosphereBedEnabled(e); }
void LiveSession::setBedLevel(float v) { settings_.bedLevel = v; engine_->atmosphereBed().level.set(v); }
void LiveSession::setBedColourHz(float v) { settings_.bedColourHz = v; engine_->atmosphereBed().colourHz.set(v); }
void LiveSession::setMicToneHz(float v) { settings_.micToneHz = v; engine_->micTransform().cutoffHz.set(v); }
void LiveSession::setMicEchoSeconds(float v) { settings_.micEchoSeconds = v; engine_->micTransform().echoSeconds.set(v); }
void LiveSession::setMicFeedback(float v) { settings_.micFeedback = v; engine_->micTransform().feedback.set(v); }
void LiveSession::setMicWet(float v) { settings_.micWet = v; engine_->micTransform().wet.set(v); }
void LiveSession::setMicEnabled(bool e) { settings_.micEnabled = e; engine_->setMicInputEnabled(e); }
void LiveSession::setLooping(bool l) { settings_.looping = l; engine_->setLooping(l); }
void LiveSession::setMicShiftHz(float v) { settings_.micShiftHz = v; engine_->micTransform().shiftHz.set(v); }
void LiveSession::setMicShiftMix(float v) { settings_.micShiftMix = v; engine_->micTransform().shiftMix.set(v); }
void LiveSession::setRoomMemoryEnabled(bool e) { settings_.roomMemoryEnabled = e; engine_->roomMemory().setEnabled(e); }
void LiveSession::setRoomMemoryLevel(float v) { settings_.roomMemoryLevel = v; engine_->roomMemory().level.set(v); }
void LiveSession::setRoomMemoryPitch(float v) { settings_.roomMemoryPitch = v; engine_->roomMemory().pitchSemitones.store(v); }

void LiveSession::updateEnvironment(double dt) {
    states_.update(engine_->environmentFeatures(), engine_->meters().micActive, dt);
}
void LiveSession::addMotionSample(double ax, double ay, double az, double dt) {
    motion_.addSample(ax, ay, az, dt);
    states_.updateMotion(motion_.energy(), dt);
}
void LiveSession::setMotionAvailable(bool available) { states_.setMotionAvailable(available); }
void LiveSession::setListenerYaw(float degrees) {
    listenerYaw_ = degrees;
    engine_->roomMemory().listenerYawDeg.store(degrees, std::memory_order_relaxed);
}

} // namespace atmos::core
