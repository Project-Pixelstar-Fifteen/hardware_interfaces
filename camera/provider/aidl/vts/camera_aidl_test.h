/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HARDWARE_INTERFACES_CAMERA_PROVIDER_AIDL_VTS_CAMERA_AIDL_TEST_H_
#define HARDWARE_INTERFACES_CAMERA_PROVIDER_AIDL_VTS_CAMERA_AIDL_TEST_H_

// TODO: LOG_TAG should not be in header
#ifndef LOG_TAG
#define LOG_TAG "camera_aidl_hal_test"
#endif

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <CameraMetadata.h>
#include <CameraParameters.h>
#include <HandleImporter.h>
#include <fmq/AidlMessageQueue.h>

#include <aidl/android/hardware/graphics/common/Dataspace.h>

#include <aidl/android/hardware/camera/common/Status.h>
#include <aidl/android/hardware/camera/common/TorchModeStatus.h>
#include <aidl/android/hardware/common/NativeHandle.h>

#include <aidl/android/hardware/camera/device/CaptureResult.h>
#include <aidl/android/hardware/camera/device/ErrorCode.h>
#include <aidl/android/hardware/camera/device/HalStream.h>
#include <aidl/android/hardware/camera/device/ICameraDevice.h>
#include <aidl/android/hardware/camera/device/NotifyMsg.h>
#include <aidl/android/hardware/camera/device/PhysicalCameraMetadata.h>
#include <aidl/android/hardware/camera/device/Stream.h>

#include <aidl/android/hardware/camera/provider/ICameraProvider.h>

#include <aidl/android/hardware/camera/metadata/RequestAvailableColorSpaceProfilesMap.h>

#include <aidl/android/hardware/graphics/common/PixelFormat.h>

#include <gtest/gtest.h>

#include <log/log.h>
#include <system/camera_metadata.h>
#include <utils/KeyedVector.h>
#include <utils/Timers.h>

using ::aidl::android::hardware::camera::common::Status;
using ::aidl::android::hardware::camera::common::TorchModeStatus;
using ::aidl::android::hardware::camera::device::BufferRequest;
using ::aidl::android::hardware::camera::device::BufferRequestStatus;
using ::aidl::android::hardware::camera::device::CameraMetadata;
using ::aidl::android::hardware::camera::device::CaptureResult;
using ::aidl::android::hardware::camera::device::ConfigureStreamsRet;
using ::aidl::android::hardware::camera::device::ErrorCode;
using ::aidl::android::hardware::camera::device::HalStream;
using ::aidl::android::hardware::camera::device::ICameraDevice;
using ::aidl::android::hardware::camera::device::ICameraDeviceSession;
using ::aidl::android::hardware::camera::device::ICameraInjectionSession;
using ::aidl::android::hardware::camera::device::NotifyMsg;
using ::aidl::android::hardware::camera::device::PhysicalCameraMetadata;
using ::aidl::android::hardware::camera::device::RequestTemplate;
using ::aidl::android::hardware::camera::device::Stream;
using ::aidl::android::hardware::camera::device::StreamBuffer;
using ::aidl::android::hardware::camera::device::StreamBufferRet;
using ::aidl::android::hardware::camera::device::StreamConfiguration;
using ::aidl::android::hardware::camera::device::StreamConfigurationMode;
using ::aidl::android::hardware::camera::metadata::RequestAvailableColorSpaceProfilesMap;
using ::aidl::android::hardware::camera::metadata::RequestAvailableDynamicRangeProfilesMap;
using ::aidl::android::hardware::camera::metadata::ScalerAvailableStreamUseCases;
using ::aidl::android::hardware::camera::provider::ConcurrentCameraIdCombination;
using ::aidl::android::hardware::camera::provider::ICameraProvider;

using ::aidl::android::hardware::common::NativeHandle;
using ::aidl::android::hardware::common::fmq::SynchronizedReadWrite;

using ::aidl::android::hardware::graphics::common::Dataspace;
using ::aidl::android::hardware::graphics::common::PixelFormat;

using ::android::hardware::camera::common::V1_0::helper::HandleImporter;
using ::android::hardware::camera::common::V1_0::helper::Size;

using ResultMetadataQueue = android::AidlMessageQueue<int8_t, SynchronizedReadWrite>;

using ::ndk::ScopedAStatus;

class DeviceCb;  // Forward declare to break circular header dependency

template <typename T>
bool contains(const std::set<T>& container, T value) {
    return container.find(value) != container.end();
}

class CameraAidlTest : public ::testing::TestWithParam<std::string> {
  public:
    enum SystemCameraKind {
        /**
         * These camera devices are visible to all apps and system components alike
         */
        PUBLIC = 0,

        /**
         * These camera devices are visible only to processes having the
         * android.permission.SYSTEM_CAMERA permission. They are not exposed to 3P
         * apps.
         */
        SYSTEM_ONLY_CAMERA,

        /**
         * These camera devices are visible only to HAL clients (that try to connect
         * on a hwbinder thread).
         */
        HIDDEN_SECURE_CAMERA
    };

    enum BufferManagerType { FRAMEWORK = 0, HAL, SESSION };

    struct AvailableStream {
        int32_t width;
        int32_t height;
        int32_t format;
    };

    enum ReprocessType {
        PRIV_REPROCESS,
        YUV_REPROCESS,
    };

    // Copied from ColorSpace.java (see Named)
    enum ColorSpaceNamed {
        SRGB,
        LINEAR_SRGB,
        EXTENDED_SRGB,
        LINEAR_EXTENDED_SRGB,
        BT709,
        BT2020,
        DCI_P3,
        DISPLAY_P3,
        NTSC_1953,
        SMPTE_C,
        ADOBE_RGB,
        PRO_PHOTO_RGB,
        ACES,
        ACESCG,
        CIE_XYZ,
        CIE_LAB,
        BT2020_HLG,
        BT2020_PQ
    };

    struct AvailableZSLInputOutput {
        int32_t inputFormat;
        int32_t outputFormat;
    };

    virtual void SetUp() override;
    virtual void TearDown() override;

    std::vector<std::string> getCameraDeviceNames(std::shared_ptr<ICameraProvider>& provider,
                                                  bool addSecureOnly = false);

    static bool isSecureOnly(const std::shared_ptr<ICameraProvider>& provider,
                             const std::string& name);

    std::map<std::string, std::string> getCameraDeviceIdToNameMap(
            std::shared_ptr<ICameraProvider> provider);

    static std::vector<ConcurrentCameraIdCombination> getConcurrentDeviceCombinations(
            std::shared_ptr<ICameraProvider>& provider);

    void notifyDeviceState(int64_t state);

    static void allocateGraphicBuffer(uint32_t width, uint32_t height, uint64_t usage,
                                      PixelFormat format, buffer_handle_t* buffer_handle /*out*/);

    static void openEmptyDeviceSession(const std::string& name,
                                       const std::shared_ptr<ICameraProvider>& provider,
                                       std::shared_ptr<ICameraDeviceSession>* session /*out*/,
                                       CameraMetadata* staticMeta /*out*/,
                                       std::shared_ptr<ICameraDevice>* device /*out*/);
    static void openEmptyInjectionSession(const std::string& name,
                                          const std::shared_ptr<ICameraProvider>& provider,
                                          std::shared_ptr<ICameraInjectionSession>* session /*out*/,
                                          CameraMetadata* staticMeta /*out*/,
                                          std::shared_ptr<ICameraDevice>* device /*out*/);

    static void createStreamConfiguration(std::vector<Stream>& streams,
                                          StreamConfigurationMode configMode,
                                          StreamConfiguration* config, int32_t jpegBufferSize = 0);

    void configureOfflineStillStream(
            const std::string& name, const std::shared_ptr<ICameraProvider>& provider,
            const AvailableStream* threshold,
            std::shared_ptr<ICameraDeviceSession>* session /*out*/, Stream* stream /*out*/,
            std::vector<HalStream>* halStreams, bool* supportsPartialResults /*out*/,
            int32_t* partialResultCount /*out*/, std::shared_ptr<DeviceCb>* outCb /*out*/,
            int32_t* jpegBufferSize /*out*/, std::set<int32_t>* halBufManagedStreamIds /*out*/);

    ndk::ScopedAStatus configureStreams(std::shared_ptr<ICameraDeviceSession>& session,
                                        const StreamConfiguration& config,
                                        BufferManagerType bufferManagerType,
                                        std::set<int32_t>* halBufManagedStreamIds,
                                        std::vector<HalStream>* halStreams);

    void configureStreams(
            const std::string& name, const std::shared_ptr<ICameraProvider>& provider,
            PixelFormat format, std::shared_ptr<ICameraDeviceSession>* session /*out*/,
            Stream* previewStream /*out*/, std::vector<HalStream>* halStreams /*out*/,
            bool* supportsPartialResults /*out*/, int32_t* partialResultCount /*out*/,
            std::set<int32_t>* halBufManagedStreamIds /*out*/,
            std::shared_ptr<DeviceCb>* outCb /*out*/, uint32_t streamConfigCounter,
            bool maxResolution,
            RequestAvailableDynamicRangeProfilesMap dynamicRangeProf =
                    RequestAvailableDynamicRangeProfilesMap::
                            ANDROID_REQUEST_AVAILABLE_DYNAMIC_RANGE_PROFILES_MAP_STANDARD,
            RequestAvailableColorSpaceProfilesMap colorSpaceProf =
                    RequestAvailableColorSpaceProfilesMap::
                            ANDROID_REQUEST_AVAILABLE_COLOR_SPACE_PROFILES_MAP_UNSPECIFIED);

    void configurePreviewStreams(
            const std::string& name, const std::shared_ptr<ICameraProvider>& provider,
            const AvailableStream* previewThreshold,
            const std::unordered_set<std::string>& physicalIds,
            std::shared_ptr<ICameraDeviceSession>* session /*out*/, Stream* previewStream /*out*/,
            std::vector<HalStream>* halStreams /*out*/, bool* supportsPartialResults /*out*/,
            int32_t* partialResultCount /*out*/, std::set<int32_t>* halBufManagedStreamIds /*out*/,
            std::shared_ptr<DeviceCb>* cb /*out*/, int32_t streamConfigCounter = 0,
            bool allowUnsupport = false);

    void configurePreviewStream(
            const std::string& name, const std::shared_ptr<ICameraProvider>& provider,
            const AvailableStream* previewThreshold,
            std::shared_ptr<ICameraDeviceSession>* session /*out*/, Stream* previewStream /*out*/,
            std::vector<HalStream>* halStreams /*out*/, bool* supportsPartialResults /*out*/,
            int32_t* partialResultCount /*out*/, bool* useHalBufManager /*out*/,
            std::shared_ptr<DeviceCb>* cb /*out*/, uint32_t streamConfigCounter = 0);

    void configureStreamUseCaseInternal(const AvailableStream &threshold);

    void configureSingleStream(
            const std::string& name, const std::shared_ptr<ICameraProvider>& provider,
            const AvailableStream* previewThreshold, uint64_t bufferUsage,
            RequestTemplate reqTemplate, std::shared_ptr<ICameraDeviceSession>* session /*out*/,
            Stream* previewStream /*out*/, std::vector<HalStream>* halStreams /*out*/,
            bool* supportsPartialResults /*out*/, int32_t* partialResultCount /*out*/,
            bool* useHalBufManager /*out*/, std::shared_ptr<DeviceCb>* cb /*out*/,
            uint32_t streamConfigCounter = 0);

    void verifyLogicalOrUltraHighResCameraMetadata(const std::string& cameraName,
                                                   const std::shared_ptr<ICameraDevice>& device,
                                                   const CameraMetadata& chars,
                                                   const std::vector<std::string>& deviceNames);

    static void verifyCameraCharacteristics(const CameraMetadata& chars);

    static void verifyExtendedSceneModeCharacteristics(const camera_metadata_t* metadata);

    void verifyHighSpeedRecordingCharacteristics(const std::string& cameraName,
                                                 const CameraMetadata& chars);

    static void verifyZoomCharacteristics(const camera_metadata_t* metadata);

    static void verifyRecommendedConfigs(const CameraMetadata& chars);

    static void verifyMonochromeCharacteristics(const CameraMetadata& chars);

    static void verifyManualFlashStrengthControlCharacteristics(
            const camera_metadata_t* staticMeta);

    static void verifyMonochromeCameraResult(
            const ::android::hardware::camera::common::V1_0::helper::CameraMetadata& metadata);

    static void verifyStreamUseCaseCharacteristics(const camera_metadata_t* metadata);

    static void verifySettingsOverrideCharacteristics(const camera_metadata_t* metadata);

    static void verifyStreamCombination(const std::shared_ptr<ICameraDevice>& device,
                                        const StreamConfiguration& config, bool expectedStatus);

    static void verifySessionCharacteristics(const CameraMetadata& session_chars,
                                             const CameraMetadata& camera_chars);

    static void verifyLogicalCameraResult(const camera_metadata_t* staticMetadata,
                                          const std::vector<uint8_t>& resultMetadata);

    static void verifyLensIntrinsicsResult(const std::vector<uint8_t>& resultMetadata);

    static void verifyBuffersReturned(const std::shared_ptr<ICameraDeviceSession>& session,
                                      int32_t streamId, const std::shared_ptr<DeviceCb>& cb,
                                      uint32_t streamConfigCounter = 0);

    void verifyBuffersReturned(const std::shared_ptr<ICameraDeviceSession>& session,
                               const std::vector<int32_t>& streamIds,
                               const std::shared_ptr<DeviceCb>& cb,
                               uint32_t streamConfigCounter = 0);

    static void verifySessionReconfigurationQuery(
            const std::shared_ptr<ICameraDeviceSession>& session, camera_metadata* oldSessionParams,
            camera_metadata* newSessionParams);

    static void verifyRequestTemplate(const camera_metadata_t* metadata,
                                      RequestTemplate requestTemplate);

    static void overrideRotateAndCrop(CameraMetadata* settings /*in/out*/);

    static bool isDepthOnly(const camera_metadata_t* staticMeta);

    static bool isUltraHighResolution(const camera_metadata_t* staticMeta);

    static Status getAvailableOutputStreams(const camera_metadata_t* staticMeta,
                                            std::vector<AvailableStream>& outputStreams,
                                            const AvailableStream* threshold = nullptr,
                                            bool maxResolution = false);

    static Status getMaxOutputSizeForFormat(const camera_metadata_t* staticMeta, PixelFormat format,
                                            Size* size, bool maxResolution = false);

    static Status getMandatoryConcurrentStreams(const camera_metadata_t* staticMeta,
                                                std::vector<AvailableStream>* outputStreams);

    static bool supportsPreviewStabilization(const std::string& name,
                                             const std::shared_ptr<ICameraProvider>& provider);

    static Status getJpegBufferSize(camera_metadata_t* staticMeta, int32_t* outBufSize);

    static Status isConstrainedModeAvailable(camera_metadata_t* staticMeta);

    static Status isLogicalMultiCamera(const camera_metadata_t* staticMeta);

    static bool isTorchSupported(const camera_metadata_t* staticMeta);

    static bool isTorchStrengthControlSupported(const camera_metadata_t* staticMeta);

    static Status isOfflineSessionSupported(const camera_metadata_t* staticMeta);

    static bool isReadoutTimestampSupported(const camera_metadata_t* staticMeta);

    static Status getPhysicalCameraIds(const camera_metadata_t* staticMeta,
                                       std::unordered_set<std::string>* physicalIds /*out*/);

    static Status getSupportedKeys(camera_metadata_t* staticMeta, uint32_t tagId,
                                   std::unordered_set<int32_t>* requestIDs /*out*/);

    static void fillOutputStreams(camera_metadata_ro_entry_t* entry,
                                  std::vector<AvailableStream>& outputStreams,
                                  const AvailableStream* threshold = nullptr,
                                  const int32_t availableConfigOutputTag = 0u);

    static void constructFilteredSettings(
            const std::shared_ptr<ICameraDeviceSession>& session,
            const std::unordered_set<int32_t>& availableKeys, RequestTemplate reqTemplate,
            android::hardware::camera::common::V1_0::helper::CameraMetadata*
                    defaultSettings /*out*/,
            android::hardware::camera::common::V1_0::helper::CameraMetadata* filteredSettings
            /*out*/);

    static Status pickConstrainedModeSize(camera_metadata_t* staticMeta,
                                          AvailableStream& hfrStream);

    static Status isZSLModeAvailable(const camera_metadata_t* staticMeta);

    static Status isZSLModeAvailable(const camera_metadata_t* staticMeta, ReprocessType reprocType);

    static Status getZSLInputOutputMap(camera_metadata_t* staticMeta,
                                       std::vector<AvailableZSLInputOutput>& inputOutputMap);

    static Status findLargestSize(const std::vector<AvailableStream>& streamSizes, int32_t format,
                                  AvailableStream& result);

    static Status isMonochromeCamera(const camera_metadata_t* staticMeta);

    static Status getSystemCameraKind(const camera_metadata_t* staticMeta,
                                      SystemCameraKind* systemCameraKind);

    static void getMultiResolutionStreamConfigurations(
            camera_metadata_ro_entry* multiResStreamConfigs,
            camera_metadata_ro_entry* streamConfigs,
            camera_metadata_ro_entry* maxResolutionStreamConfigs,
            const camera_metadata_t* staticMetadata);

    static void getPrivacyTestPatternModes(
            const camera_metadata_t* staticMetadata,
            std::unordered_set<int32_t>* privacyTestPatternModes /*out*/);

    static Dataspace getDataspace(PixelFormat format);

    void processCaptureRequestInternal(uint64_t bufferUsage, RequestTemplate reqTemplate,
                                       bool useSecureOnlyCameras);

    void processPreviewStabilizationCaptureRequestInternal(
            bool previewStabilizationOn,
            /*inout*/ std::unordered_map<std::string, nsecs_t>& cameraDeviceToTimeLag);

    static bool is10BitDynamicRangeCapable(const camera_metadata_t* staticMeta);

    static void get10BitDynamicRangeProfiles(
            const camera_metadata_t* staticMeta,
            std::vector<RequestAvailableDynamicRangeProfilesMap>* profiles);

    static bool reportsColorSpaces(const camera_metadata_t* staticMeta);

    static void getColorSpaceProfiles(
            const camera_metadata_t* staticMeta,
            std::vector<aidl::android::hardware::camera::metadata::
                                RequestAvailableColorSpaceProfilesMap>* profiles);

    static bool isColorSpaceCompatibleWithDynamicRangeAndPixelFormat(
            const camera_metadata_t* staticMeta, RequestAvailableColorSpaceProfilesMap colorSpace,
            RequestAvailableDynamicRangeProfilesMap dynamicRangeProfile,
            aidl::android::hardware::graphics::common::PixelFormat pixelFormat);

    static const char* getColorSpaceProfileString(RequestAvailableColorSpaceProfilesMap colorSpace);

    static const char* getDynamicRangeProfileString(
            RequestAvailableDynamicRangeProfilesMap dynamicRangeProfile);

    static int32_t halFormatToPublicFormat(
            aidl::android::hardware::graphics::common::PixelFormat pixelFormat);

    // Used by switchToOffline where a new result queue is created for offline reqs
    void updateInflightResultQueue(const std::shared_ptr<ResultMetadataQueue>& resultQueue);

    static Size getMinSize(Size a, Size b);

    void processColorSpaceRequest(RequestAvailableColorSpaceProfilesMap colorSpace,
                                  RequestAvailableDynamicRangeProfilesMap dynamicRangeProfile);

    void processZoomSettingsOverrideRequests(
            int32_t frameCount, const bool *overrideSequence, const bool *expectedResults);

    bool supportZoomSettingsOverride(const camera_metadata_t* staticMeta);
    static bool supportsStreamUseCaseCap(const camera_metadata_t* staticMeta);
    static bool supportsCroppedRawUseCase(const camera_metadata_t* staticMeta);
    bool isPerFrameControl(const camera_metadata_t* staticMeta);

    void getSupportedSizes(const camera_metadata_t* ch, uint32_t tag, int32_t format,
                           std::vector<std::tuple<size_t, size_t>>* sizes /*out*/);

    void getSupportedDurations(const camera_metadata_t* ch, uint32_t tag, int32_t format,
                               const std::vector<std::tuple<size_t, size_t>>& sizes,
                               std::vector<int64_t>* durations /*out*/);

  protected:
    // In-flight queue for tracking completion of capture requests.
    struct InFlightRequest {
        // Set by notify() SHUTTER call.
        nsecs_t shutterTimestamp;
        nsecs_t shutterReadoutTimestamp;

        bool errorCodeValid;
        ErrorCode errorCode;

        // Is partial result supported
        bool usePartialResult;

        // Partial result count expected
        int32_t numPartialResults;

        // Message queue
        std::shared_ptr<ResultMetadataQueue> resultQueue;

        // Set by process_capture_result call with valid metadata
        bool haveResultMetadata;

        // Decremented by calls to process_capture_result with valid output
        // and input buffers
        ssize_t numBuffersLeft;

        // A 64bit integer to index the frame number associated with this result.
        int64_t frameNumber;

        // The partial result count (index) for this capture result.
        int32_t partialResultCount;

        // For buffer drop errors, the stream ID for the stream that lost a buffer.
        // For physical sub-camera result errors, the Id of the physical stream
        // for the physical sub-camera.
        // Otherwise -1.
        int32_t errorStreamId;

        // If this request has any input buffer
        bool hasInputBuffer;

        // Result metadata
        ::android::hardware::camera::common::V1_0::helper::CameraMetadata collectedResult;

        // Inflight buffers
        using OutstandingBuffers = std::unordered_map<uint64_t, buffer_handle_t>;
        std::vector<OutstandingBuffers> mOutstandingBufferIds;

        // A copy-able StreamBuffer using buffer_handle_t instead of AIDLs NativeHandle
        struct NativeStreamBuffer {
            int32_t streamId;
            int64_t bufferId;
            buffer_handle_t buffer;
            aidl::android::hardware::camera::device::BufferStatus status;
            buffer_handle_t acquireFence;
            buffer_handle_t releaseFence;
        };

        // Buffers are added by process_capture_result when output buffers
        // return from HAL but framework.
        struct StreamBufferAndTimestamp {
            NativeStreamBuffer buffer;
            nsecs_t timeStamp;
        };
        std::vector<StreamBufferAndTimestamp> resultOutputBuffers;

        std::unordered_set<std::string> expectedPhysicalResults;

        InFlightRequest()
            : shutterTimestamp(0),
              shutterReadoutTimestamp(0),
              errorCodeValid(false),
              errorCode(ErrorCode::ERROR_BUFFER),
              usePartialResult(false),
              numPartialResults(0),
              resultQueue(nullptr),
              haveResultMetadata(false),
              numBuffersLeft(0),
              frameNumber(0),
              partialResultCount(0),
              errorStreamId(-1),
              hasInputBuffer(false),
              collectedResult(1, 10) {}

        InFlightRequest(ssize_t numBuffers, bool hasInput, bool partialResults,
                        int32_t partialCount, std::shared_ptr<ResultMetadataQueue> queue = nullptr)
            : shutterTimestamp(0),
              shutterReadoutTimestamp(0),
              errorCodeValid(false),
              errorCode(ErrorCode::ERROR_BUFFER),
              usePartialResult(partialResults),
              numPartialResults(partialCount),
              resultQueue(queue),
              haveResultMetadata(false),
              numBuffersLeft(numBuffers),
              frameNumber(0),
              partialResultCount(0),
              errorStreamId(-1),
              hasInputBuffer(hasInput),
              collectedResult(1, 10) {}

        InFlightRequest(ssize_t numBuffers, bool hasInput, bool partialResults,
                        int32_t partialCount,
                        const std::unordered_set<std::string>& extraPhysicalResult,
                        std::shared_ptr<ResultMetadataQueue> queue = nullptr)
            : shutterTimestamp(0),
              shutterReadoutTimestamp(0),
              errorCodeValid(false),
              errorCode(ErrorCode::ERROR_BUFFER),
              usePartialResult(partialResults),
              numPartialResults(partialCount),
              resultQueue(queue),
              haveResultMetadata(false),
              numBuffersLeft(numBuffers),
              frameNumber(0),
              partialResultCount(0),
              errorStreamId(-1),
              hasInputBuffer(hasInput),
              collectedResult(1, 10),
              expectedPhysicalResults(extraPhysicalResult) {}

        ~InFlightRequest() {
            for (auto& buffer : resultOutputBuffers) {
                native_handle_t* acquireFenceHandle = const_cast<native_handle_t*>(
                        buffer.buffer.acquireFence);
                native_handle_close(acquireFenceHandle);
                native_handle_delete(acquireFenceHandle);

                native_handle_t* releaseFenceHandle = const_cast<native_handle_t*>(
                        buffer.buffer.releaseFence);
                native_handle_close(releaseFenceHandle);
                native_handle_delete(releaseFenceHandle);
            }
        }
    };

    static bool matchDeviceName(const std::string& deviceName, const std::string& providerType,
                                std::string* deviceVersion, std::string* cameraId);

    static void verify10BitMetadata(HandleImporter& importer, const InFlightRequest& request,
                                    RequestAvailableDynamicRangeProfilesMap profile);

    static void waitForReleaseFence(
            std::vector<InFlightRequest::StreamBufferAndTimestamp>& resultOutputBuffers);

    static void validateDefaultRequestMetadata(RequestTemplate reqTemplate,
                                               const CameraMetadata& rawMetadata);

    // Map from frame number to the in-flight request state
    typedef std::unordered_map<uint32_t, std::shared_ptr<InFlightRequest>> InFlightMap;

    std::mutex mLock;                          // Synchronize access to member variables
    std::condition_variable mResultCondition;  // Condition variable for incoming results
    InFlightMap mInflightMap;                  // Map of all inflight requests

    std::vector<NotifyMsg> mNotifyMessages;  // Current notification message

    std::mutex mTorchLock;               // Synchronize access to torch status
    std::condition_variable mTorchCond;  // Condition variable for torch status
    TorchModeStatus mTorchStatus;        // Current torch status

    // Camera provider service
    std::shared_ptr<ICameraProvider> mProvider;

    // Camera device session used by the tests
    // Tests should take care of closing this session and setting it back to nullptr in successful
    // case. Declared as a field to allow TeadDown function to close the session if a test assertion
    // fails.
    std::shared_ptr<ICameraDeviceSession> mSession;

    // Camera provider type.
    std::string mProviderType;

    HandleImporter mHandleImporter;

    bool mSupportReadoutTimestamp;

    friend class DeviceCb;
    friend class SimpleDeviceCb;
    friend class TorchProviderCb;
};

namespace {
// device@<major>.<minor>/<type>/id
const char* kDeviceNameRE = "device@([0-9]+\\.[0-9]+)/\\s+/(.+)";
const std::string CAMERA_DEVICE_API_VERSION_1 = "1.1";
const int32_t CAMERA_DEVICE_API_MINOR_VERSION_3 = 3;

const int32_t kMaxVideoWidth = 4096;
const int32_t kMaxVideoHeight = 2160;

const int64_t kStreamBufferTimeoutSec = 3;
const int64_t kTorchTimeoutSec = 1;
const char* kDumpOutput = "/dev/null";
const uint32_t kMaxPreviewWidth = 1920;
const uint32_t kMaxPreviewHeight = 1080;
}  // namespace
#endif  // HARDWARE_INTERFACES_CAMERA_PROVIDER_AIDL_VTS_CAMERA_AIDL_TEST_H_
