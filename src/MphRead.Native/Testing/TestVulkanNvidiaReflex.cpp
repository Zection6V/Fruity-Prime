#include "../NativeRuntime/Rhi/Vulkan/VulkanNvidiaReflex.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
using namespace MphRead::NativeRuntime::Rhi;
using namespace MphRead::NativeRuntime::Rhi::Vulkan;
namespace {
void Expect(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
struct Fake {
 static Fake* f;
 VkResult create = VK_SUCCESS, mode = VK_SUCCESS, sleep = VK_SUCCESS, wait = VK_SUCCESS;
 unsigned destroyed = 0, sleeps = 0, waits = 0, timingQueries = 0;
 std::vector<VkLatencySleepModeInfoNV> modes;
 std::vector<VkSetLatencyMarkerInfoNV> markers;
 std::vector<std::uint64_t> ids;
 Fake() { f = this; }
 static VKAPI_ATTR VkResult VKAPI_CALL Create(VkDevice, const VkSemaphoreCreateInfo* info, const VkAllocationCallbacks*, VkSemaphore* out) {
  const auto* type = static_cast<const VkSemaphoreTypeCreateInfo*>(info->pNext);
  Expect(type->semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE && !type->initialValue, "Dedicated timeline must start at zero.");
  if (f->create == VK_SUCCESS) *out = reinterpret_cast<VkSemaphore>(101); return f->create;
 }
 static VKAPI_ATTR void VKAPI_CALL Destroy(VkDevice, VkSemaphore sem, const VkAllocationCallbacks*) {
  Expect(sem == reinterpret_cast<VkSemaphore>(101), "Wrong timeline destroyed."); ++f->destroyed;
 }
 static VKAPI_ATTR VkResult VKAPI_CALL Mode(VkDevice, VkSwapchainKHR, const VkLatencySleepModeInfoNV* info) {
  f->modes.push_back(*info); return f->mode;
 }
 static VKAPI_ATTR VkResult VKAPI_CALL Sleep(VkDevice, VkSwapchainKHR, const VkLatencySleepInfoNV* info) {
  Expect(info->signalSemaphore == reinterpret_cast<VkSemaphore>(101), "Submission timeline reused for sleep.");
  ++f->sleeps; f->ids.push_back(info->value); return f->sleep;
 }
 static VKAPI_ATTR VkResult VKAPI_CALL Wait(VkDevice, const VkSemaphoreWaitInfo* info, std::uint64_t timeout) {
  Expect(timeout == 2'000'000 && info->semaphoreCount == 1 && *info->pValues == f->ids.back(), "Sleep poll must be bounded and use current frame ID.");
  ++f->waits; return f->wait;
 }
 static VKAPI_ATTR void VKAPI_CALL Marker(VkDevice, VkSwapchainKHR, const VkSetLatencyMarkerInfoNV* info) { f->markers.push_back(*info); }
 static VKAPI_ATTR void VKAPI_CALL Timings(VkDevice, VkSwapchainKHR, VkGetLatencyMarkerInfoNV* info) { ++f->timingQueries; info->timingCount = 0; }
 VulkanNvidiaReflex::Dispatch Dispatch(unsigned revision = 3) { return {reinterpret_cast<VkDevice>(1), true, revision, "", Create, Destroy, Wait, Mode, Sleep, Marker, Timings}; }
};
Fake* Fake::f = nullptr;
void Run() {
 std::uint64_t sequence = 40;
 const auto chain = reinterpret_cast<VkSwapchainKHR>(2);
 {
  Fake f; VulkanNvidiaReflex r(f.Dispatch(), sequence); r.SetSwapchain(chain);
  Expect(r.Caps().supportsBoost && r.Caps().provider == LowLatencyProvider::Nvidia, "Usable native capability lost.");
  Expect(!f.modes.back().lowLatencyMode && !f.modes.back().lowLatencyBoost && !f.modes.back().minimumIntervalUs, "Off mapping differs.");
  r.SetMode(LowLatencyMode::On, 6945);
  Expect(f.modes.back().lowLatencyMode && !f.modes.back().lowLatencyBoost && f.modes.back().minimumIntervalUs == 6945, "On mapping differs.");
  auto modeCalls = f.modes.size(); r.SetMode(LowLatencyMode::On, 6945); Expect(f.modes.size() == modeCalls, "Steady state repeats SetMode.");
  r.SetMode(LowLatencyMode::OnBoost, 4167); Expect(f.modes.back().lowLatencyBoost, "Boost did not reach driver.");
  f.wait = VK_TIMEOUT; Expect(!r.BeginFrame() && !r.BeginFrame() && f.sleeps == 1 && f.waits == 2, "Bounded polls repeated native sleep.");
  f.wait = VK_SUCCESS; Expect(r.BeginFrame() && r.FrameId() == 41 && r.SubmissionId() == 41, "Frame identity lost.");
  Expect(r.BeginFrame() && f.sleeps == 1, "Ready frame repeated sleep.");
  for (unsigned i = 0; i < 7; ++i) { r.Mark(static_cast<LowLatencyMarker>(i)); r.Mark(static_cast<LowLatencyMarker>(i)); }
  Expect(f.markers.size() == 7, "Markers duplicated or absent.");
  for (const auto& m : f.markers) Expect(m.presentID == 41, "Markers disagree on frame ID.");
  r.FinishFrame(); Expect(r.BeginFrame() && r.FrameId() == 42 && f.sleeps == 2, "Next present did not start a new frame."); r.FinishFrame();
  r.SetSwapchain(VK_NULL_HANDLE); Expect(!f.modes.back().lowLatencyMode, "Retiring chain was not disabled.");
  r.SetSwapchain(reinterpret_cast<VkSwapchainKHR>(3)); Expect(f.modes.back().lowLatencyBoost && r.Stats().swapchainGeneration == 2, "Requested Boost lost after recreate.");
  r.SetMode(LowLatencyMode::Off, 0); Expect(!f.modes.back().lowLatencyBoost && r.SubmissionId() == 0 && r.BeginFrame() && f.sleeps == 2, "Off retains native pacing.");
  const auto offId = r.FrameId();
  Expect(offId > 42 && r.SubmissionId() == 0 && r.MeasurementAvailable() && !r.PacingActive(), "Off lost measurement identity or retained explicit queue attribution.");
  const auto markersBeforeOff = f.markers.size();
  for (unsigned i = 0; i < 7; ++i) { r.Mark(static_cast<LowLatencyMarker>(i)); r.Mark(static_cast<LowLatencyMarker>(i)); }
  Expect(f.markers.size() == markersBeforeOff + 7, "Off marker contract absent or duplicated.");
  for (auto i = markersBeforeOff; i < f.markers.size(); ++i) Expect(f.markers[i].presentID == offId, "Off marker attribution differs.");
  r.FinishFrame(); Expect(!r.FrameId() && r.SubmissionId() == 0, "Closed measurement keeps attribution.");
  r.Shutdown(); r.Shutdown(); Expect(f.destroyed == 1, "Timeline destruction not idempotent.");
 }
 {
  Fake f; VulkanNvidiaReflex r(f.Dispatch(), sequence); r.SetSwapchain(chain);
  for (auto mode : {LowLatencyMode::Off, LowLatencyMode::On, LowLatencyMode::OnBoost}) {
   r.SetMode(mode, 0); Expect(r.BeginFrame(), "Frame admission failed.");
   const auto abandonedId = r.FrameId();
   r.Mark(LowLatencyMarker::InputSample); r.Mark(LowLatencyMarker::SimulationStart); r.Mark(LowLatencyMarker::SimulationEnd);
   const auto markers = f.markers.size();
   const auto completed = r.Stats().completedMeasurementFrames;
   const auto abandoned = r.Stats().abandonedMeasurementFrames;
   r.AbandonFrame(); r.AbandonFrame(); r.FinishFrame(); r.Mark(LowLatencyMarker::PresentStart); r.Mark(LowLatencyMarker::PresentEnd);
   Expect(!r.FrameId() && r.SubmissionId() == 0 && f.markers.size() == markers
    && r.Stats().completedMeasurementFrames == completed && r.Stats().abandonedMeasurementFrames == abandoned + 1,
    "Abandon manufactured completion/markers or retained attribution.");
   Expect(r.BeginFrame() && r.FrameId() > abandonedId, "Abandon reused a stale frame identity.");
   r.Mark(LowLatencyMarker::InputSample); r.Mark(LowLatencyMarker::SimulationStart); r.Mark(LowLatencyMarker::SimulationEnd);
   Expect(f.markers.size() == markers + 3, "Abandoned marker mask suppressed the next frame."); r.FinishFrame();
  }
 }
 {
  Fake f; VulkanNvidiaReflex r(f.Dispatch(), sequence); r.SetSwapchain(chain);
  // No timing query at zero or on duplicate closure, including after 120 sleeps.
  r.FinishFrame(); Expect(!f.timingQueries, "Empty closure polled timings.");
  for (unsigned i = 0; i < 360; ++i) {
   if (i == 120) r.SetMode(LowLatencyMode::OnBoost, 0);
   if (i == 240) r.SetMode(LowLatencyMode::Off, 0);
   Expect(r.BeginFrame(), "Cadence frame admission failed.");
   Expect(r.SubmissionId() == (i >= 120 && i < 240 ? r.FrameId() : 0), "Off/Boost/Off did not switch implicit/explicit attribution.");
   r.FinishFrame(); r.FinishFrame();
   Expect(f.timingQueries == (i + 1) / 120, "Timing polling is not once per 120 completed measurement frames.");
  }
  Expect(f.sleeps == 120 && r.Stats().completedMeasurementFrames == 360 && r.Stats().timingQueries == 3,
   "Timing cadence depends on native sleep count.");
  for (unsigned i = 0; i < 120; ++i) { Expect(r.BeginFrame(), "Abandoned cadence admission failed."); r.AbandonFrame(); r.FinishFrame(); }
  Expect(f.timingQueries == 3 && r.Stats().completedMeasurementFrames == 360, "Abandoned frames advanced timing cadence.");
 }
 {
  Fake f; VulkanNvidiaReflex r(f.Dispatch(), sequence); r.SetSwapchain(chain); r.SetMode(LowLatencyMode::On, 0);
  f.wait = VK_TIMEOUT; Expect(!r.BeginFrame(), "Pending sleep unexpectedly ready.");
  r.SetMode(LowLatencyMode::Off, 0); f.wait = VK_SUCCESS;
  Expect(r.BeginFrame() && f.sleeps == 1 && r.FrameId(), "Mode switch lost pending sleep identity.");
  const auto id = r.FrameId(); r.SetSwapchain(VK_NULL_HANDLE); r.SetSwapchain(reinterpret_cast<VkSwapchainKHR>(3));
  Expect(r.FrameId() == id && r.BeginFrame() && f.sleeps == 1, "Resize lost admitted Off measurement.");
  r.FinishFrame(); Expect(r.BeginFrame() && r.FrameId() > id && f.sleeps == 1, "Off after pending sleep repeated pacing.");
 }
 {
  Fake f; VulkanNvidiaReflex r(f.Dispatch(2), sequence); r.SetSwapchain(chain); r.SetMode(LowLatencyMode::OnBoost, 0);
  Expect(r.BeginFrame() && !r.SubmissionId(), "Revision 2 admitted explicit attribution.");
 }
 for (unsigned failure = 0; failure < 6; ++failure) {
  Fake f; auto d = f.Dispatch();
  if (failure == 0) { d.enabled = false; d.reason = "VK_NV_low_latency2 not exposed"; }
  if (failure == 1) d.marker = nullptr;
  if (failure == 2) f.create = VK_ERROR_OUT_OF_HOST_MEMORY;
  VulkanNvidiaReflex r(d, sequence); r.SetSwapchain(chain);
  if (failure == 3) f.mode = VK_ERROR_UNKNOWN;
  r.SetMode(LowLatencyMode::OnBoost, 0);
  if (failure == 4) f.sleep = VK_ERROR_UNKNOWN;
  if (failure == 5) f.wait = VK_ERROR_DEVICE_LOST;
  Expect(r.BeginFrame(), "Native error blocked generic recovery.");
  const auto state = ResolveLowLatency(LowLatencyMode::OnBoost, r.Caps());
  Expect(state.requested == LowLatencyMode::OnBoost && state.effective == LowLatencyMode::On && state.provider == LowLatencyProvider::Generic
   && state.authority == PacingAuthority::Generic && !state.boostSupported && !state.fallbackReason.empty(), "Optional failure retained native authority or erased requested Boost.");
  if (failure >= 2) Expect(state.fallbackReason.find("VkResult=") != std::string::npos, "Native error code lost.");
 }
}
}
int main() {
#if defined(_WIN32)
 _putenv_s("FRUITY_RENDER_METRICS", "1");
#else
 setenv("FRUITY_RENDER_METRICS", "1", 1);
#endif
 try { Run(); std::cout << "NVIDIA Reflex PASS: modes, dedicated timeline, bounded admission, Off markers, timing cadence, abandon, recreation, revision gate, failures\n"; return 0; }
 catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
