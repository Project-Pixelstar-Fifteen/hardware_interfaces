/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include <DefaultConfig.h>
#include <FakeVehicleHardware.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <utils/SystemClock.h>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

namespace {

using ::aidl::android::hardware::automotive::vehicle::GetValueRequest;
using ::aidl::android::hardware::automotive::vehicle::GetValueResult;
using ::aidl::android::hardware::automotive::vehicle::RawPropValues;
using ::aidl::android::hardware::automotive::vehicle::SetValueRequest;
using ::aidl::android::hardware::automotive::vehicle::SetValueResult;
using ::aidl::android::hardware::automotive::vehicle::StatusCode;
using ::aidl::android::hardware::automotive::vehicle::VehiclePropConfig;
using ::aidl::android::hardware::automotive::vehicle::VehicleProperty;
using ::aidl::android::hardware::automotive::vehicle::VehiclePropertyStatus;
using ::aidl::android::hardware::automotive::vehicle::VehiclePropValue;
using ::testing::ContainerEq;
using ::testing::Eq;
using ::testing::WhenSortedBy;

constexpr int INVALID_PROP_ID = 0;

}  // namespace

class FakeVehicleHardwareTest : public ::testing::Test {
  protected:
    void SetUp() override {}

    FakeVehicleHardware* getHardware() { return &mHardware; }

    StatusCode setValues(const std::vector<SetValueRequest>& requests) {
        return getHardware()->setValues(
                [this](const std::vector<SetValueResult> results) { return onSetValues(results); },
                requests);
    }

    StatusCode getValues(const std::vector<GetValueRequest>& requests) {
        return getHardware()->getValues(
                [this](const std::vector<GetValueResult> results) { return onGetValues(results); },
                requests);
    }

    void onSetValues(const std::vector<SetValueResult> results) {
        for (auto& result : results) {
            mSetValueResults.push_back(result);
        }
    }

    const std::vector<SetValueResult>& getSetValueResults() { return mSetValueResults; }

    void onGetValues(const std::vector<GetValueResult> results) {
        for (auto& result : results) {
            mGetValueResults.push_back(result);
        }
    }

    const std::vector<GetValueResult>& getGetValueResults() { return mGetValueResults; }

    void onPropertyChangeEvent(const std::vector<VehiclePropValue>& values) {
        for (auto& value : values) {
            mChangedProperties.push_back(value);
        }
    }

    const std::vector<VehiclePropValue>& getChangedProperties() { return mChangedProperties; }

    static void addSetValueRequest(std::vector<SetValueRequest>& requests,
                                   std::vector<SetValueResult>& expectedResults, int64_t requestId,
                                   const VehiclePropValue& value, StatusCode expectedStatus) {
        SetValueRequest request;
        request.requestId = requestId;
        request.value = value;
        request.value.timestamp = elapsedRealtimeNano();
        requests.push_back(std::move(request));

        SetValueResult result;
        result.requestId = requestId;
        result.status = expectedStatus;
        expectedResults.push_back(std::move(result));
    }

    static void addGetValueRequest(std::vector<GetValueRequest>& requests,
                                   std::vector<GetValueResult>& expectedResults, int64_t requestId,
                                   const VehiclePropValue& value, StatusCode expectedStatus) {
        GetValueRequest request;
        request.requestId = requestId;
        request.prop.prop = value.prop;
        request.prop.areaId = value.areaId;
        requests.push_back(std::move(request));

        GetValueResult result;
        result.requestId = requestId;
        result.status = expectedStatus;
        if (expectedStatus == StatusCode::OK) {
            result.prop = value;
        }
        expectedResults.push_back(std::move(result));
    }

    std::vector<VehiclePropValue> getTestPropValues() {
        VehiclePropValue fuelCapacity = {
                .prop = toInt(VehicleProperty::INFO_FUEL_CAPACITY),
                .value = {.floatValues = {1.0}},
        };

        VehiclePropValue leftTirePressure = {
                .prop = toInt(VehicleProperty::TIRE_PRESSURE),
                .value = {.floatValues = {170.0}},
                .areaId = WHEEL_FRONT_LEFT,
        };

        VehiclePropValue rightTirePressure = {
                .prop = toInt(VehicleProperty::TIRE_PRESSURE),
                .value = {.floatValues = {180.0}},
                .areaId = WHEEL_FRONT_RIGHT,
        };

        return {fuelCapacity, leftTirePressure, rightTirePressure};
    }

    struct PropValueCmp {
        bool operator()(const VehiclePropValue& a, const VehiclePropValue& b) const {
            return (a.prop < b.prop) || ((a.prop == b.prop) && (a.value < b.value)) ||
                   ((a.prop == b.prop) && (a.value == b.value) && (a.areaId < b.areaId));
        }
    } mPropValueCmp;

  private:
    FakeVehicleHardware mHardware;
    std::vector<SetValueResult> mSetValueResults;
    std::vector<GetValueResult> mGetValueResults;
    std::vector<VehiclePropValue> mChangedProperties;
};

TEST_F(FakeVehicleHardwareTest, testGetAllPropertyConfigs) {
    std::vector<VehiclePropConfig> configs = getHardware()->getAllPropertyConfigs();

    ASSERT_EQ(configs.size(), defaultconfig::getDefaultConfigs().size());
}

TEST_F(FakeVehicleHardwareTest, testGetDefaultValues) {
    std::vector<GetValueRequest> getValueRequests;
    std::vector<GetValueResult> expectedGetValueResults;
    int64_t requestId = 1;

    for (auto& config : defaultconfig::getDefaultConfigs()) {
        int propId = config.config.prop;
        if (isGlobalProp(propId)) {
            if (config.initialValue == RawPropValues{}) {
                addGetValueRequest(getValueRequests, expectedGetValueResults, requestId++,
                                   VehiclePropValue{.prop = propId}, StatusCode::NOT_AVAILABLE);
                continue;
            }
            addGetValueRequest(getValueRequests, expectedGetValueResults, requestId++,
                               VehiclePropValue{
                                       .prop = propId,
                                       .value = config.initialValue,
                               },
                               StatusCode::OK);
            continue;
        }
        for (auto areaConfig : config.config.areaConfigs) {
            StatusCode status = StatusCode::OK;
            VehiclePropValue propValue{
                    .prop = propId,
                    .areaId = areaConfig.areaId,
            };
            if (config.initialAreaValues.empty()) {
                if (config.initialValue == RawPropValues{}) {
                    status = StatusCode::NOT_AVAILABLE;
                } else {
                    propValue.value = config.initialValue;
                }
            } else if (auto valueForAreaIt = config.initialAreaValues.find(areaConfig.areaId);
                       valueForAreaIt != config.initialAreaValues.end()) {
                propValue.value = valueForAreaIt->second;
            } else {
                status = StatusCode::NOT_AVAILABLE;
            }
            addGetValueRequest(getValueRequests, expectedGetValueResults, requestId++, propValue,
                               status);
        }
    }

    // In our implementation, this would finish immediately.
    StatusCode status = getValues(getValueRequests);

    ASSERT_EQ(status, StatusCode::OK);

    std::vector<GetValueResult> getValueResultsWithNoTimestamp;
    for (auto& result : getGetValueResults()) {
        GetValueResult resultCopy = result;
        resultCopy.prop->timestamp = 0;
        getValueResultsWithNoTimestamp.push_back(std::move(resultCopy));
    }
    ASSERT_THAT(getValueResultsWithNoTimestamp, ContainerEq(expectedGetValueResults));
}

TEST_F(FakeVehicleHardwareTest, testSetValues) {
    std::vector<SetValueRequest> requests;
    std::vector<SetValueResult> expectedResults;

    int64_t requestId = 1;
    for (auto& value : getTestPropValues()) {
        addSetValueRequest(requests, expectedResults, requestId++, value, StatusCode::OK);
    }

    StatusCode status = setValues(requests);

    ASSERT_EQ(status, StatusCode::OK);

    // Although callback might be called asynchronously, in our implementation, the callback would
    // be called before setValues returns.
    ASSERT_THAT(getSetValueResults(), ContainerEq(expectedResults));
}

TEST_F(FakeVehicleHardwareTest, testSetValuesError) {
    std::vector<SetValueRequest> requests;
    std::vector<SetValueResult> expectedResults;

    int64_t requestId = 1;

    VehiclePropValue invalidProp = {
            .prop = INVALID_PROP_ID,
    };
    addSetValueRequest(requests, expectedResults, requestId++, invalidProp,
                       StatusCode::INVALID_ARG);

    for (auto& value : getTestPropValues()) {
        addSetValueRequest(requests, expectedResults, requestId++, value, StatusCode::OK);
    }

    StatusCode status = setValues(requests);

    ASSERT_EQ(status, StatusCode::OK);

    // Although callback might be called asynchronously, in our implementation, the callback would
    // be called before setValues returns.
    ASSERT_THAT(getSetValueResults(), ContainerEq(expectedResults));
}

TEST_F(FakeVehicleHardwareTest, testRegisterOnPropertyChangeEvent) {
    getHardware()->registerOnPropertyChangeEvent(std::bind(
            &FakeVehicleHardwareTest_testRegisterOnPropertyChangeEvent_Test::onPropertyChangeEvent,
            this, std::placeholders::_1));

    auto testValues = getTestPropValues();
    std::vector<SetValueRequest> requests;
    std::vector<SetValueResult> expectedResults;
    int64_t requestId = 1;
    for (auto& value : testValues) {
        addSetValueRequest(requests, expectedResults, requestId++, value, StatusCode::OK);
    }
    int64_t timestamp = elapsedRealtimeNano();

    StatusCode status = setValues(requests);

    ASSERT_EQ(status, StatusCode::OK);

    auto updatedValues = getChangedProperties();
    std::vector<VehiclePropValue> updatedValuesWithNoTimestamp;
    for (auto& value : updatedValues) {
        ASSERT_GE(value.timestamp, timestamp);
        VehiclePropValue valueCopy = value;
        valueCopy.timestamp = 0;
        updatedValuesWithNoTimestamp.push_back(std::move(valueCopy));
    }

    ASSERT_THAT(updatedValuesWithNoTimestamp, WhenSortedBy(mPropValueCmp, Eq(testValues)));
}

TEST_F(FakeVehicleHardwareTest, testReadValues) {
    std::vector<SetValueRequest> setValueRequests;
    std::vector<SetValueResult> expectedSetValueResults;

    int64_t requestId = 1;
    for (auto& value : getTestPropValues()) {
        addSetValueRequest(setValueRequests, expectedSetValueResults, requestId++, value,
                           StatusCode::OK);
    }
    int64_t timestamp = elapsedRealtimeNano();

    // In our implementation, this would finish immediately.
    StatusCode status = setValues(setValueRequests);

    ASSERT_EQ(status, StatusCode::OK);

    std::vector<GetValueRequest> getValueRequests;
    std::vector<GetValueResult> expectedGetValueResults;
    for (auto& value : getTestPropValues()) {
        addGetValueRequest(getValueRequests, expectedGetValueResults, requestId++, value,
                           StatusCode::OK);
    }

    // In our implementation, this would finish immediately.
    status = getValues(getValueRequests);

    ASSERT_EQ(status, StatusCode::OK);

    std::vector<GetValueResult> getValueResultsWithNoTimestamp;
    for (auto& result : getGetValueResults()) {
        ASSERT_GE(result.prop->timestamp, timestamp);
        GetValueResult resultCopy = result;
        resultCopy.prop->timestamp = 0;
        getValueResultsWithNoTimestamp.push_back(std::move(resultCopy));
    }
    ASSERT_THAT(getValueResultsWithNoTimestamp, ContainerEq(expectedGetValueResults));
}

TEST_F(FakeVehicleHardwareTest, testReadValuesErrorInvalidProp) {
    std::vector<SetValueRequest> setValueRequests;
    std::vector<SetValueResult> expectedSetValueResults;

    int64_t requestId = 1;
    for (auto& value : getTestPropValues()) {
        addSetValueRequest(setValueRequests, expectedSetValueResults, requestId++, value,
                           StatusCode::OK);
    }

    // In our implementation, this would finish immediately.
    StatusCode status = setValues(setValueRequests);

    ASSERT_EQ(status, StatusCode::OK);

    std::vector<GetValueRequest> getValueRequests;
    std::vector<GetValueResult> expectedGetValueResults;
    VehiclePropValue invalidProp = {
            .prop = INVALID_PROP_ID,
    };
    addGetValueRequest(getValueRequests, expectedGetValueResults, requestId++, invalidProp,
                       StatusCode::INVALID_ARG);

    // In our implementation, this would finish immediately.
    status = getValues(getValueRequests);

    ASSERT_EQ(status, StatusCode::OK);
    ASSERT_THAT(getGetValueResults(), ContainerEq(expectedGetValueResults));
}

TEST_F(FakeVehicleHardwareTest, testReadValuesErrorNotAvailable) {
    std::vector<GetValueRequest> getValueRequests;
    std::vector<GetValueResult> expectedGetValueResults;
    // VEHICLE_MAP_SERVICE does not have initial value, 'get' must always return
    // StatusCode::NOT_AVAILABLE.
    addGetValueRequest(getValueRequests, expectedGetValueResults, 0,
                       VehiclePropValue{
                               .prop = VEHICLE_MAP_SERVICE,
                       },
                       StatusCode::NOT_AVAILABLE);

    // In our implementation, this would finish immediately.
    StatusCode status = getValues(getValueRequests);

    ASSERT_EQ(status, StatusCode::OK);
    ASSERT_THAT(getGetValueResults(), ContainerEq(expectedGetValueResults));
}

TEST_F(FakeVehicleHardwareTest, testSetStatusMustIgnore) {
    VehiclePropValue testValue = getTestPropValues()[0];
    testValue.status = VehiclePropertyStatus::UNAVAILABLE;

    std::vector<SetValueRequest> setValueRequests;
    std::vector<SetValueResult> expectedSetValueResults;

    int64_t requestId = 1;
    addSetValueRequest(setValueRequests, expectedSetValueResults, requestId++, testValue,
                       StatusCode::OK);

    // In our implementation, this would finish immediately.
    StatusCode status = setValues(setValueRequests);

    ASSERT_EQ(status, StatusCode::OK);
    ASSERT_THAT(getSetValueResults(), ContainerEq(expectedSetValueResults));

    std::vector<GetValueRequest> getValueRequests;
    getValueRequests.push_back(GetValueRequest{
            .requestId = requestId++,
            .prop = testValue,
    });

    // In our implementation, this would finish immediately.
    status = getValues(getValueRequests);

    ASSERT_EQ(status, StatusCode::OK);
    ASSERT_EQ(getGetValueResults().size(), static_cast<size_t>(1));
    ASSERT_EQ(getGetValueResults()[0].status, StatusCode::OK);
    // The status should be by-default AVAILABLE for new status.
    ASSERT_EQ(getGetValueResults()[0].prop->status, VehiclePropertyStatus::AVAILABLE);

    // Try to set the property again. The status should not be overwritten.
    status = setValues(setValueRequests);

    ASSERT_EQ(status, StatusCode::OK);

    status = getValues(getValueRequests);

    ASSERT_EQ(status, StatusCode::OK);
    ASSERT_EQ(getGetValueResults().size(), static_cast<size_t>(2));
    ASSERT_EQ(getGetValueResults()[1].status, StatusCode::OK);
    ASSERT_EQ(getGetValueResults()[1].prop->status, VehiclePropertyStatus::AVAILABLE);
}

}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android
