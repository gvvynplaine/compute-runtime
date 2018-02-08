/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "driver_diagnostics_tests.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"

using namespace OCLRT;

bool containsHint(const char *providedHint, char *userData) {
    for (auto i = 0; i < maxHintCounter; i++) {
        if (strcmp(providedHint, userData + i * DriverDiagnostics::maxHintStringSize) == 0) {
            return true;
        }
    }
    return false;
}

void CL_CALLBACK callbackFunction(const char *providedHint, const void *flags, size_t size, void *userData) {
    int offset = 0;
    while (((char *)userData + offset)[0] != 0) {
        offset += DriverDiagnostics::maxHintStringSize;
    }
    strcpy_s((char *)userData + offset, DriverDiagnostics::maxHintStringSize, providedHint);
}

cl_diagnostics_verbose_level diagnosticsVerboseLevels[] = {
    CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL,
    CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL,
    CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL};

TEST_P(VerboseLevelTest, GivenVerboseLevelWhenProvidedHintLevelIsSameOrAllThenCallbackFunctionTakesProvidedHint) {
    cl_device_id deviceID = devices[0];
    cl_diagnostics_verbose_level diagnosticsLevel = GetParam();
    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, (cl_context_properties)diagnosticsLevel, 0};
    retVal = CL_SUCCESS;

    auto context = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), callbackFunction, (void *)userData, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    for (auto hintLevel : validLevels) {
        memset(userData, 0, maxHintCounter * DriverDiagnostics::maxHintStringSize);
        context->providePerformanceHint(hintLevel, hintId);
        if (hintLevel == diagnosticsLevel || hintLevel == CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL) {
            EXPECT_TRUE(containsHint(expectedHint, userData));
        } else {
            EXPECT_FALSE(containsHint(expectedHint, userData));
        }
    }
    delete context;
}

TEST_P(VerboseLevelTest, GivenVerboseLevelAllWhenAnyHintIsProvidedThenCallbackFunctionTakesProvidedHint) {
    cl_device_id deviceID = devices[0];
    cl_diagnostics_verbose_level providedHintLevel = GetParam();
    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    retVal = CL_SUCCESS;

    auto context = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), callbackFunction, (void *)userData, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    context->providePerformanceHint(providedHintLevel, hintId);
    EXPECT_TRUE(containsHint(expectedHint, userData));
    delete context;
}

TEST_P(PerformanceHintBufferTest, GivenHostPtrAndSizeAlignmentsWhenBufferIsCreatingThenContextProvidesHintsAboutAlignmentsAndAllocatingMemory) {
    uintptr_t addressForBuffer = (uintptr_t)address;
    size_t sizeForBuffer = MemoryConstants::cacheLineSize;
    if (!alignedAddress) {
        addressForBuffer++;
    }
    if (!alignedSize) {
        sizeForBuffer--;
    }
    buffer = Buffer::create(
        context,
        CL_MEM_USE_HOST_PTR,
        sizeForBuffer,
        (void *)addressForBuffer,
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_BUFFER_DOESNT_MEET_ALIGNMENT_RESTRICTIONS], addressForBuffer, sizeForBuffer, MemoryConstants::pageSize, MemoryConstants::pageSize);
    EXPECT_EQ(!(alignedSize && alignedAddress), containsHint(expectedHint, userData));
    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_BUFFER_NEEDS_ALLOCATE_MEMORY], 0);
    EXPECT_EQ(!(alignedSize && alignedAddress), containsHint(expectedHint, userData));
}

TEST_P(PerformanceHintCommandQueueTest, GivenProfilingFlagAndPreemptionFlagWhenCommandQueueIsCreatingThenContextProvidesProperHints) {
    cl_command_queue_properties properties = 0;
    if (profilingEnabled) {
        properties = CL_QUEUE_PROFILING_ENABLE;
    }
    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[DRIVER_CALLS_INTERNAL_CL_FLUSH], 0);
    EXPECT_TRUE(containsHint(expectedHint, userData));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[PROFILING_ENABLED], 0);
    EXPECT_EQ(profilingEnabled, containsHint(expectedHint, userData));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[PROFILING_ENABLED_WITH_DISABLED_PREEMPTION], 0);
    if (device->getHardwareInfo().pPlatform->eProductFamily < IGFX_SKYLAKE && preemptionSupported && profilingEnabled) {
        EXPECT_TRUE(containsHint(expectedHint, userData));
    } else {
        EXPECT_FALSE(containsHint(expectedHint, userData));
    }
}

TEST_P(PerformanceHintCommandQueueTest, GivenEnabledProfilingFlagAndSupportedPreemptionFlagWhenCommandQueueIsCreatingWithPropertiesThenContextProvidesProperHints) {
    cl_command_queue_properties properties[3] = {0};
    if (profilingEnabled) {
        properties[0] = CL_QUEUE_PROPERTIES;
        properties[1] = CL_QUEUE_PROFILING_ENABLE;
    }
    cmdQ = clCreateCommandQueueWithProperties(context, device, properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[DRIVER_CALLS_INTERNAL_CL_FLUSH], 0);
    EXPECT_TRUE(containsHint(expectedHint, userData));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[PROFILING_ENABLED], 0);
    EXPECT_EQ(profilingEnabled, containsHint(expectedHint, userData));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[PROFILING_ENABLED_WITH_DISABLED_PREEMPTION], 0);
    if (device->getHardwareInfo().pPlatform->eProductFamily < IGFX_SKYLAKE && preemptionSupported && profilingEnabled) {
        EXPECT_TRUE(containsHint(expectedHint, userData));
    } else {
        EXPECT_FALSE(containsHint(expectedHint, userData));
    }
}

TEST_F(PerformanceHintTest, GivenAlignedHostPtrWhenSubbufferIsCreatingThenContextProvidesHintAboutSharingMemoryWithParentBuffer) {

    cl_mem_flags flg = CL_MEM_USE_HOST_PTR;
    cl_buffer_region region = {0, MemoryConstants::cacheLineSize - 1};
    void *address = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    auto buffer = clCreateBuffer(context, flg, MemoryConstants::cacheLineSize, address, &retVal);
    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto subBuffer = clCreateSubBuffer(buffer, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION,
                                       &region, &retVal);
    EXPECT_NE(nullptr, subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[SUBBUFFER_SHARES_MEMORY], buffer);
    EXPECT_TRUE(containsHint(expectedHint, userData));

    retVal = clReleaseMemObject(subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    alignedFree(address);
}

TEST_F(PerformanceHintTest, GivenContextWhenSVMAllocIsCreatingThenContextProvidesHintAboutAlignment) {

    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        cl_mem_flags flg = CL_MEM_READ_WRITE;

        size_t size = 4096;

        auto SVMPtr = clSVMAlloc(context, flg, size, 128);
        EXPECT_NE(nullptr, SVMPtr);

        snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_SVM_ALLOC_MEETS_ALIGNMENT_RESTRICTIONS], SVMPtr, size);
        EXPECT_TRUE(containsHint(expectedHint, userData));

        clSVMFree(context, SVMPtr);
    }
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeNDIsDefaultWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, 0, emptyDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeNDIsTrueWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(true);
    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, 0, emptyDispatchInfo);
    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeNDIsFalseWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, 0, emptyDispatchInfo);
    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}

TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeSquaredIsDefaultWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, 0, emptyDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeSquaredIsTrueWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, 0, emptyDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeSquaredIsFalseWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, 0, emptyDispatchInfo);
}

TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeNDIsDefaultWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, 0, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeNDIsTrueWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(true);
    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, 0, invalidDispatchInfo);
    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}
TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeNDIsFalseWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, 0, invalidDispatchInfo);
    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}

TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeSquaredIsDefaultWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, 0, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeSquaredIsTrueWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, 0, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeSquaredIsFalseWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, 0, invalidDispatchInfo);
}

TEST_F(PerformanceHintTest, GivenContextAndDispatchinfoAndEnableComputeWorkSizeSquaredIsDefaultWhenProvideLocalWorkGroupSizeIsCalledReturnValue) {

    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, 0, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenContextAndDispatchinfoAndEnableComputeWorkSizeSquaredIsTrueWhenProvideLocalWorkGroupSizeIsCalledReturnValue) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 2, {32, 32, 1}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, 0, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenContextAndDispatchinfoAndEnableComputeWorkSizeSquaredIsFalseWhenProvideLocalWorkGroupSizeIsCalledReturnValue) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 2, {32, 32, 1}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, 0, invalidDispatchInfo);
}

TEST_F(PerformanceHintTest, GivenZeroCopyImageAndContextWhenCreateImageThenContextProvidesHintAboutAlignment) {

    std::unique_ptr<Image> image(ImageHelper<Image1dDefaults>::create(context));
    EXPECT_TRUE(image->isMemObjZeroCopy());

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_IMAGE_MEETS_ALIGNMENT_RESTRICTIONS], static_cast<cl_mem>(image.get()));
    EXPECT_TRUE(containsHint(expectedHint, userData));
}

TEST_F(PerformanceHintTest, GivenNonZeroCopyImageAndContextWhenCreateImageThenContextDoesntProvidesHintAboutAlignment) {

    std::unique_ptr<Image> image(ImageHelper<ImageUseHostPtr<Image1dDefaults>>::create(context));
    EXPECT_FALSE(image->isMemObjZeroCopy());

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_IMAGE_MEETS_ALIGNMENT_RESTRICTIONS], static_cast<cl_mem>(image.get()));
    EXPECT_FALSE(containsHint(expectedHint, userData));
}

TEST_P(PerformanceHintKernelTest, GivenSpillFillWhenKernelIsInitializedThenContextProvidesProperHint) {

    auto pDevice = castToObject<Device>(devices[0]);
    auto size = zeroSized ? 0 : 1024;
    MockKernelWithInternals mockKernel(*pDevice, context);
    SPatchMediaVFEState mediaVFEstate;

    mediaVFEstate.PerThreadScratchSpace = size;

    mockKernel.kernelInfo.patchInfo.mediavfestate = &mediaVFEstate;
    size *= pDevice->getDeviceInfo().computeUnitsUsedForScratch * mockKernel.mockKernel->getKernelInfo().getMaxSimdSize();

    mockKernel.mockKernel->initialize();

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[REGISTER_PRESSURE_TOO_HIGH],
             mockKernel.mockKernel->getKernelInfo().name.c_str(), size);
    EXPECT_EQ(!zeroSized, containsHint(expectedHint, userData));
}

TEST_P(PerformanceHintKernelTest, GivenPrivateSurfaceWhenKernelIsInitializedThenContextProvidesProperHint) {

    auto pDevice = castToObject<Device>(devices[0]);
    auto size = zeroSized ? 0 : 1024;
    MockKernelWithInternals mockKernel(*pDevice, context);
    SPatchAllocateStatelessPrivateSurface allocateStatelessPrivateMemorySurface;

    allocateStatelessPrivateMemorySurface.PerThreadPrivateMemorySize = size;
    allocateStatelessPrivateMemorySurface.SurfaceStateHeapOffset = 128;
    allocateStatelessPrivateMemorySurface.DataParamOffset = 16;
    allocateStatelessPrivateMemorySurface.DataParamSize = 8;

    mockKernel.kernelInfo.patchInfo.pAllocateStatelessPrivateSurface = &allocateStatelessPrivateMemorySurface;
    size *= pDevice->getDeviceInfo().computeUnitsUsedForScratch * mockKernel.mockKernel->getKernelInfo().getMaxSimdSize();

    mockKernel.mockKernel->initialize();

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[PRIVATE_MEMORY_USAGE_TOO_HIGH],
             mockKernel.mockKernel->getKernelInfo().name.c_str(), size);
    EXPECT_EQ(!zeroSized, containsHint(expectedHint, userData));
}

INSTANTIATE_TEST_CASE_P(
    DriverDiagnosticsTests,
    VerboseLevelTest,
    testing::ValuesIn(diagnosticsVerboseLevels));

INSTANTIATE_TEST_CASE_P(
    DriverDiagnosticsTests,
    PerformanceHintBufferTest,
    testing::Combine(
        ::testing::Bool(),
        ::testing::Bool()));

INSTANTIATE_TEST_CASE_P(
    DriverDiagnosticsTests,
    PerformanceHintCommandQueueTest,
    testing::Combine(
        ::testing::Bool(),
        ::testing::Bool()));

INSTANTIATE_TEST_CASE_P(
    DriverDiagnosticsTests,
    PerformanceHintKernelTest,
    testing::Bool());
