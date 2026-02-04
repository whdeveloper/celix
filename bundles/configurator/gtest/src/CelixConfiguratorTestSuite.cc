/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *  KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "celix_configurator_service.h"
#include "celix_bundle_activator.h"
#include "celix_framework_factory.h"
#include "celix_constants.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

class CelixConfiguratorTestSuite : public ::testing::Test {
public:
    CelixConfiguratorTestSuite() {
        auto props = celix_properties_create();
        celix_properties_set(props, CELIX_FRAMEWORK_CLEAN_CACHE_DIR_ON_CREATE, "true");
        celix_properties_set(props, CELIX_FRAMEWORK_CACHE_DIR, ".configurator_test_cache");
        auto fwPtr = celix_frameworkFactory_createFramework(props);
        fw = std::shared_ptr<celix_framework_t>{fwPtr, [](celix_framework_t* f) {celix_frameworkFactory_destroyFramework(f);}};
        ctx = std::shared_ptr<celix_bundle_context_t>{celix_framework_getFrameworkContext(fw.get()), [](celix_bundle_context_t*){/*nop*/}};
    }

    ~CelixConfiguratorTestSuite() override = default;

    std::shared_ptr<celix_framework_t> fw{};
    std::shared_ptr<celix_bundle_context_t> ctx{};
};

TEST_F(CelixConfiguratorTestSuite, ActivatorStartTest) {
    void *act{};
    auto status = celix_bundleActivator_create(ctx.get(), &act);
    ASSERT_EQ(CELIX_SUCCESS, status);
    status = celix_bundleActivator_start(act, ctx.get());
    ASSERT_EQ(CELIX_SUCCESS, status);

    celix_bundleContext_waitForEvents(ctx.get());
    long svcId = celix_bundleContext_findService(ctx.get(), CELIX_CONFIGURATOR_SERVICE_NAME);
    EXPECT_TRUE(svcId >= 0);

    status = celix_bundleActivator_stop(act, ctx.get());
    ASSERT_EQ(CELIX_SUCCESS, status);
    status = celix_bundleActivator_destroy(act, ctx.get());
    ASSERT_EQ(CELIX_SUCCESS, status);
}

TEST_F(CelixConfiguratorTestSuite, ApplyValidInstructionTest) {
    void *act{};
    celix_bundleActivator_create(ctx.get(), &act);
    celix_bundleActivator_start(act, ctx.get());
    celix_bundleContext_waitForEvents(ctx.get());

    // Get configurator service
    celix_configurator_service_t* svc = nullptr;
    celix_service_use_options_t opts{};
    opts.filter.serviceName = CELIX_CONFIGURATOR_SERVICE_NAME;
    opts.callbackHandle = &svc;
    opts.use = [](void* handle, void* service) {
        auto** svcPtr = static_cast<celix_configurator_service_t**>(handle);
        *svcPtr = static_cast<celix_configurator_service_t*>(service);
    };
    
    bool found = celix_bundleContext_useServiceWithOptions(ctx.get(), &opts);
    ASSERT_TRUE(found);
    ASSERT_NE(nullptr, svc);

    // Create instruction with optional policy
    auto instruction = celix_properties_create();
    celix_properties_set(instruction, CELIX_CONFIGURATOR_PID, "test.pid");
    celix_properties_set(instruction, CELIX_CONFIGURATOR_POLICY, CELIX_CONFIGURATOR_POLICY_OPTIONAL);
    celix_properties_set(instruction, "key1", "value1");
    celix_properties_set(instruction, "key2", "value2");

    auto status = svc->applyInstruction(svc->handle, instruction);
    // Note: Without Config Admin, this will log warnings but track the policy internally
    EXPECT_EQ(CELIX_SUCCESS, status);

    celix_properties_destroy(instruction);
    
    celix_bundleActivator_stop(act, ctx.get());
    celix_bundleActivator_destroy(act, ctx.get());
}

TEST_F(CelixConfiguratorTestSuite, ApplyInvalidInstructionTest) {
    void *act{};
    celix_bundleActivator_create(ctx.get(), &act);
    celix_bundleActivator_start(act, ctx.get());
    celix_bundleContext_waitForEvents(ctx.get());

    celix_configurator_service_t* svc = nullptr;
    celix_service_use_options_t opts{};
    opts.filter.serviceName = CELIX_CONFIGURATOR_SERVICE_NAME;
    opts.callbackHandle = &svc;
    opts.use = [](void* handle, void* service) {
        auto** svcPtr = static_cast<celix_configurator_service_t**>(handle);
        *svcPtr = static_cast<celix_configurator_service_t*>(service);
    };
    
    celix_bundleContext_useServiceWithOptions(ctx.get(), &opts);

    // Create instruction without PID (invalid)
    auto instruction = celix_properties_create();
    celix_properties_set(instruction, "key1", "value1");

    auto status = svc->applyInstruction(svc->handle, instruction);
    EXPECT_NE(CELIX_SUCCESS, status);

    celix_properties_destroy(instruction);
    
    celix_bundleActivator_stop(act, ctx.get());
    celix_bundleActivator_destroy(act, ctx.get());
}

TEST_F(CelixConfiguratorTestSuite, ApplyInstructionsFromJsonTest) {
    void *act{};
    celix_bundleActivator_create(ctx.get(), &act);
    celix_bundleActivator_start(act, ctx.get());
    celix_bundleContext_waitForEvents(ctx.get());

    celix_configurator_service_t* svc = nullptr;
    celix_service_use_options_t opts{};
    opts.filter.serviceName = CELIX_CONFIGURATOR_SERVICE_NAME;
    opts.callbackHandle = &svc;
    opts.use = [](void* handle, void* service) {
        auto** svcPtr = static_cast<celix_configurator_service_t**>(handle);
        *svcPtr = static_cast<celix_configurator_service_t*>(service);
    };
    
    celix_bundleContext_useServiceWithOptions(ctx.get(), &opts);

    // Valid JSON with multiple instructions
    const char* json = R"([
        {
            "pid": "test.pid1",
            "configurationPolicy": "optional",
            "key1": "value1"
        },
        {
            "pid": "test.pid2",
            "configurationPolicy": "require",
            "gracePeriod": 1000,
            "key2": "value2"
        }
    ])";

    auto status = svc->applyInstructionsFromJson(svc->handle, json);
    EXPECT_EQ(CELIX_SUCCESS, status);
    
    celix_bundleActivator_stop(act, ctx.get());
    celix_bundleActivator_destroy(act, ctx.get());
}

TEST_F(CelixConfiguratorTestSuite, ApplyInvalidJsonTest) {
    void *act{};
    celix_bundleActivator_create(ctx.get(), &act);
    celix_bundleActivator_start(act, ctx.get());
    celix_bundleContext_waitForEvents(ctx.get());

    celix_configurator_service_t* svc = nullptr;
    celix_service_use_options_t opts{};
    opts.filter.serviceName = CELIX_CONFIGURATOR_SERVICE_NAME;
    opts.callbackHandle = &svc;
    opts.use = [](void* handle, void* service) {
        auto** svcPtr = static_cast<celix_configurator_service_t**>(handle);
        *svcPtr = static_cast<celix_configurator_service_t*>(service);
    };
    
    celix_bundleContext_useServiceWithOptions(ctx.get(), &opts);

    // Invalid JSON
    const char* json = R"({invalid json})";

    auto status = svc->applyInstructionsFromJson(svc->handle, json);
    EXPECT_NE(CELIX_SUCCESS, status);
    
    celix_bundleActivator_stop(act, ctx.get());
    celix_bundleActivator_destroy(act, ctx.get());
}

TEST_F(CelixConfiguratorTestSuite, OptionalPolicyTest) {
    void *act{};
    celix_bundleActivator_create(ctx.get(), &act);
    celix_bundleActivator_start(act, ctx.get());
    celix_bundleContext_waitForEvents(ctx.get());

    celix_configurator_service_t* svc = nullptr;
    celix_service_use_options_t opts{};
    opts.filter.serviceName = CELIX_CONFIGURATOR_SERVICE_NAME;
    opts.callbackHandle = &svc;
    opts.use = [](void* handle, void* service) {
        auto** svcPtr = static_cast<celix_configurator_service_t**>(handle);
        *svcPtr = static_cast<celix_configurator_service_t*>(service);
    };
    
    celix_bundleContext_useServiceWithOptions(ctx.get(), &opts);

    // Apply with optional policy
    auto instruction = celix_properties_create();
    celix_properties_set(instruction, CELIX_CONFIGURATOR_PID, "optional.pid");
    celix_properties_set(instruction, CELIX_CONFIGURATOR_POLICY, CELIX_CONFIGURATOR_POLICY_OPTIONAL);
    celix_properties_set(instruction, "key", "value");

    auto status = svc->applyInstruction(svc->handle, instruction);
    EXPECT_EQ(CELIX_SUCCESS, status);

    // Remove configuration - with optional policy, component should continue
    status = svc->removeConfiguration(svc->handle, "optional.pid");
    EXPECT_EQ(CELIX_SUCCESS, status);

    celix_properties_destroy(instruction);
    
    celix_bundleActivator_stop(act, ctx.get());
    celix_bundleActivator_destroy(act, ctx.get());
}

TEST_F(CelixConfiguratorTestSuite, RequirePolicyTest) {
    void *act{};
    celix_bundleActivator_create(ctx.get(), &act);
    celix_bundleActivator_start(act, ctx.get());
    celix_bundleContext_waitForEvents(ctx.get());

    celix_configurator_service_t* svc = nullptr;
    celix_service_use_options_t opts{};
    opts.filter.serviceName = CELIX_CONFIGURATOR_SERVICE_NAME;
    opts.callbackHandle = &svc;
    opts.use = [](void* handle, void* service) {
        auto** svcPtr = static_cast<celix_configurator_service_t**>(handle);
        *svcPtr = static_cast<celix_configurator_service_t*>(service);
    };
    
    celix_bundleContext_useServiceWithOptions(ctx.get(), &opts);

    // Apply with require policy
    auto instruction = celix_properties_create();
    celix_properties_set(instruction, CELIX_CONFIGURATOR_PID, "require.pid");
    celix_properties_set(instruction, CELIX_CONFIGURATOR_POLICY, CELIX_CONFIGURATOR_POLICY_REQUIRE);
    celix_properties_set(instruction, "key", "value");

    auto status = svc->applyInstruction(svc->handle, instruction);
    EXPECT_EQ(CELIX_SUCCESS, status);

    // Remove configuration - with require policy, component should be deactivated immediately
    status = svc->removeConfiguration(svc->handle, "require.pid");
    EXPECT_EQ(CELIX_SUCCESS, status);

    celix_properties_destroy(instruction);
    
    celix_bundleActivator_stop(act, ctx.get());
    celix_bundleActivator_destroy(act, ctx.get());
}

TEST_F(CelixConfiguratorTestSuite, GracePeriodTest) {
    void *act{};
    celix_bundleActivator_create(ctx.get(), &act);
    celix_bundleActivator_start(act, ctx.get());
    celix_bundleContext_waitForEvents(ctx.get());

    celix_configurator_service_t* svc = nullptr;
    celix_service_use_options_t opts{};
    opts.filter.serviceName = CELIX_CONFIGURATOR_SERVICE_NAME;
    opts.callbackHandle = &svc;
    opts.use = [](void* handle, void* service) {
        auto** svcPtr = static_cast<celix_configurator_service_t**>(handle);
        *svcPtr = static_cast<celix_configurator_service_t*>(service);
    };
    
    celix_bundleContext_useServiceWithOptions(ctx.get(), &opts);

    // Apply with require policy and grace period
    auto instruction = celix_properties_create();
    celix_properties_set(instruction, CELIX_CONFIGURATOR_PID, "grace.pid");
    celix_properties_set(instruction, CELIX_CONFIGURATOR_POLICY, CELIX_CONFIGURATOR_POLICY_REQUIRE);
    celix_properties_setLong(instruction, CELIX_CONFIGURATOR_GRACE_PERIOD, 500); // 500ms
    celix_properties_set(instruction, "key", "value");

    auto status = svc->applyInstruction(svc->handle, instruction);
    EXPECT_EQ(CELIX_SUCCESS, status);

    // Remove configuration - should schedule grace period
    status = svc->removeConfiguration(svc->handle, "grace.pid");
    EXPECT_EQ(CELIX_SUCCESS, status);

    // Wait for grace period to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    celix_properties_destroy(instruction);
    
    celix_bundleActivator_stop(act, ctx.get());
    celix_bundleActivator_destroy(act, ctx.get());
}

TEST_F(CelixConfiguratorTestSuite, GracePeriodCancelTest) {
    void *act{};
    celix_bundleActivator_create(ctx.get(), &act);
    celix_bundleActivator_start(act, ctx.get());
    celix_bundleContext_waitForEvents(ctx.get());

    celix_configurator_service_t* svc = nullptr;
    celix_service_use_options_t opts{};
    opts.filter.serviceName = CELIX_CONFIGURATOR_SERVICE_NAME;
    opts.callbackHandle = &svc;
    opts.use = [](void* handle, void* service) {
        auto** svcPtr = static_cast<celix_configurator_service_t**>(handle);
        *svcPtr = static_cast<celix_configurator_service_t*>(service);
    };
    
    celix_bundleContext_useServiceWithOptions(ctx.get(), &opts);

    // Apply with require policy and grace period
    auto instruction = celix_properties_create();
    celix_properties_set(instruction, CELIX_CONFIGURATOR_PID, "cancel.pid");
    celix_properties_set(instruction, CELIX_CONFIGURATOR_POLICY, CELIX_CONFIGURATOR_POLICY_REQUIRE);
    celix_properties_setLong(instruction, CELIX_CONFIGURATOR_GRACE_PERIOD, 1000); // 1000ms
    celix_properties_set(instruction, "key", "value");

    auto status = svc->applyInstruction(svc->handle, instruction);
    EXPECT_EQ(CELIX_SUCCESS, status);

    // Remove configuration - should schedule grace period
    status = svc->removeConfiguration(svc->handle, "cancel.pid");
    EXPECT_EQ(CELIX_SUCCESS, status);

    // Reapply configuration before grace period expires - should cancel deactivation
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    status = svc->applyInstruction(svc->handle, instruction);
    EXPECT_EQ(CELIX_SUCCESS, status);

    // Wait longer than original grace period
    std::this_thread::sleep_for(std::chrono::milliseconds(900));

    celix_properties_destroy(instruction);
    
    celix_bundleActivator_stop(act, ctx.get());
    celix_bundleActivator_destroy(act, ctx.get());
}

TEST_F(CelixConfiguratorTestSuite, FactoryPidTest) {
    void *act{};
    celix_bundleActivator_create(ctx.get(), &act);
    celix_bundleActivator_start(act, ctx.get());
    celix_bundleContext_waitForEvents(ctx.get());

    celix_configurator_service_t* svc = nullptr;
    celix_service_use_options_t opts{};
    opts.filter.serviceName = CELIX_CONFIGURATOR_SERVICE_NAME;
    opts.callbackHandle = &svc;
    opts.use = [](void* handle, void* service) {
        auto** svcPtr = static_cast<celix_configurator_service_t**>(handle);
        *svcPtr = static_cast<celix_configurator_service_t*>(service);
    };
    
    celix_bundleContext_useServiceWithOptions(ctx.get(), &opts);

    // Apply factory configuration
    auto instruction = celix_properties_create();
    celix_properties_set(instruction, CELIX_CONFIGURATOR_FACTORY_PID, "factory.pid");
    celix_properties_set(instruction, CELIX_CONFIGURATOR_POLICY, CELIX_CONFIGURATOR_POLICY_OPTIONAL);
    celix_properties_set(instruction, "key", "value");

    auto status = svc->applyInstruction(svc->handle, instruction);
    EXPECT_EQ(CELIX_SUCCESS, status);

    celix_properties_destroy(instruction);
    
    celix_bundleActivator_stop(act, ctx.get());
    celix_bundleActivator_destroy(act, ctx.get());
}

TEST_F(CelixConfiguratorTestSuite, ZeroGracePeriodTest) {
    void *act{};
    celix_bundleActivator_create(ctx.get(), &act);
    celix_bundleActivator_start(act, ctx.get());
    celix_bundleContext_waitForEvents(ctx.get());

    celix_configurator_service_t* svc = nullptr;
    celix_service_use_options_t opts{};
    opts.filter.serviceName = CELIX_CONFIGURATOR_SERVICE_NAME;
    opts.callbackHandle = &svc;
    opts.use = [](void* handle, void* service) {
        auto** svcPtr = static_cast<celix_configurator_service_t**>(handle);
        *svcPtr = static_cast<celix_configurator_service_t*>(service);
    };
    
    celix_bundleContext_useServiceWithOptions(ctx.get(), &opts);

    // Apply with require policy and zero grace period (immediate deactivation)
    auto instruction = celix_properties_create();
    celix_properties_set(instruction, CELIX_CONFIGURATOR_PID, "zero.grace.pid");
    celix_properties_set(instruction, CELIX_CONFIGURATOR_POLICY, CELIX_CONFIGURATOR_POLICY_REQUIRE);
    celix_properties_setLong(instruction, CELIX_CONFIGURATOR_GRACE_PERIOD, 0);
    celix_properties_set(instruction, "key", "value");

    auto status = svc->applyInstruction(svc->handle, instruction);
    EXPECT_EQ(CELIX_SUCCESS, status);

    // Remove configuration - should deactivate immediately (grace period = 0)
    status = svc->removeConfiguration(svc->handle, "zero.grace.pid");
    EXPECT_EQ(CELIX_SUCCESS, status);

    celix_properties_destroy(instruction);
    
    celix_bundleActivator_stop(act, ctx.get());
    celix_bundleActivator_destroy(act, ctx.get());
}
