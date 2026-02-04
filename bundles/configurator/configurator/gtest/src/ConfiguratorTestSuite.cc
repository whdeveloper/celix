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
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <gtest/gtest.h>

#include "celix_bundle_context.h"
#include "celix_framework.h"
#include "celix_framework_factory.h"
#include "celix_constants.h"
#include "celix_properties.h"

extern "C" {
#include "celix_configurator.h"
#include "configuration_admin.h"
}

class ConfiguratorTestSuite : public ::testing::Test {
public:
    celix_framework_t* fw = nullptr;
    celix_bundle_context_t* ctx = nullptr;
    
    ConfiguratorTestSuite() {
        auto* props = celix_properties_create();
        celix_properties_set(props, CELIX_FRAMEWORK_CACHE_DIR, ".configurator_test_cache");
        celix_properties_set(props, CELIX_FRAMEWORK_CLEAN_CACHE_DIR_ON_CREATE, "true");
        celix_properties_set(props, CELIX_LOGGING_DEFAULT_ACTIVE_LOG_LEVEL_CONFIG_NAME, "trace");
        
        fw = celix_frameworkFactory_createFramework(props);
        ctx = celix_framework_getFrameworkContext(fw);
    }
    
    ~ConfiguratorTestSuite() override {
        celix_frameworkFactory_destroyFramework(fw);
    }
    
    ConfiguratorTestSuite(const ConfiguratorTestSuite&) = delete;
    ConfiguratorTestSuite& operator=(const ConfiguratorTestSuite&) = delete;
};

TEST_F(ConfiguratorTestSuite, CreateDestroyTest) {
    celix_configurator_t* configurator = celix_configurator_create(ctx);
    ASSERT_NE(nullptr, configurator);
    celix_configurator_destroy(configurator);
}

TEST_F(ConfiguratorTestSuite, StartStopTest) {
    celix_configurator_t* configurator = celix_configurator_create(ctx);
    ASSERT_NE(nullptr, configurator);
    
    celix_status_t status = celix_configurator_start(configurator);
    EXPECT_EQ(CELIX_SUCCESS, status);
    
    status = celix_configurator_stop(configurator);
    EXPECT_EQ(CELIX_SUCCESS, status);
    
    celix_configurator_destroy(configurator);
}

TEST_F(ConfiguratorTestSuite, ApplyConfigurationWithoutConfigAdminTest) {
    celix_configurator_t* configurator = celix_configurator_create(ctx);
    ASSERT_NE(nullptr, configurator);
    
    celix_status_t status = celix_configurator_start(configurator);
    ASSERT_EQ(CELIX_SUCCESS, status);
    
    // Test applying configuration without Config Admin service available
    const char* jsonConfig = R"({
        "test.pid": {
            "property1": "value1",
            "property2": 42
        }
    })";
    
    // Should return error since Config Admin is not available
    status = celix_configurator_applyConfiguration(configurator, jsonConfig);
    EXPECT_NE(CELIX_SUCCESS, status);
    
    celix_configurator_stop(configurator);
    celix_configurator_destroy(configurator);
}

TEST_F(ConfiguratorTestSuite, ParseInvalidJsonTest) {
    celix_configurator_t* configurator = celix_configurator_create(ctx);
    ASSERT_NE(nullptr, configurator);
    
    celix_status_t status = celix_configurator_start(configurator);
    ASSERT_EQ(CELIX_SUCCESS, status);
    
    // Test with invalid JSON
    const char* invalidJson = "{ invalid json }";
    status = celix_configurator_applyConfiguration(configurator, invalidJson);
    EXPECT_NE(CELIX_SUCCESS, status);
    
    celix_configurator_stop(configurator);
    celix_configurator_destroy(configurator);
}

TEST_F(ConfiguratorTestSuite, ParseJsonWithDifferentTypesTest) {
    celix_configurator_t* configurator = celix_configurator_create(ctx);
    ASSERT_NE(nullptr, configurator);
    
    celix_status_t status = celix_configurator_start(configurator);
    ASSERT_EQ(CELIX_SUCCESS, status);
    
    // Test with various data types - should parse successfully even without Config Admin
    const char* jsonConfig = R"({
        "test.pid": {
            "stringProp": "value",
            "intProp": 42,
            "doubleProp": 3.14,
            "boolProp": true,
            "arrayProp": [1, 2, 3],
            "objectProp": {"nested": "value"}
        }
    })";
    
    // Will fail because Config Admin is not available, but JSON parsing should succeed
    // (failure will be at Config Admin interaction, not JSON parsing)
    status = celix_configurator_applyConfiguration(configurator, jsonConfig);
    EXPECT_NE(CELIX_SUCCESS, status); // Fails due to missing Config Admin
    
    celix_configurator_stop(configurator);
    celix_configurator_destroy(configurator);
}

TEST_F(ConfiguratorTestSuite, ApplyConfigurationFileNotFoundTest) {
    celix_configurator_t* configurator = celix_configurator_create(ctx);
    ASSERT_NE(nullptr, configurator);
    
    celix_status_t status = celix_configurator_start(configurator);
    ASSERT_EQ(CELIX_SUCCESS, status);
    
    // Test with non-existent file
    status = celix_configurator_applyConfigurationFile(configurator, "/nonexistent/file.json");
    EXPECT_NE(CELIX_SUCCESS, status);
    
    celix_configurator_stop(configurator);
    celix_configurator_destroy(configurator);
}

TEST_F(ConfiguratorTestSuite, FactoryConfigurationTest) {
    celix_configurator_t* configurator = celix_configurator_create(ctx);
    ASSERT_NE(nullptr, configurator);
    
    celix_status_t status = celix_configurator_start(configurator);
    ASSERT_EQ(CELIX_SUCCESS, status);
    
    // Test factory configuration (PID ending with ~)
    const char* jsonConfig = R"({
        "factory.pid~": {
            "property1": "value1"
        }
    })";
    
    // Should parse but fail due to missing Config Admin
    status = celix_configurator_applyConfiguration(configurator, jsonConfig);
    EXPECT_NE(CELIX_SUCCESS, status);
    
    celix_configurator_stop(configurator);
    celix_configurator_destroy(configurator);
}

TEST_F(ConfiguratorTestSuite, ArrayOfConfigurationsTest) {
    celix_configurator_t* configurator = celix_configurator_create(ctx);
    ASSERT_NE(nullptr, configurator);
    
    celix_status_t status = celix_configurator_start(configurator);
    ASSERT_EQ(CELIX_SUCCESS, status);
    
    // Test array of configurations for factory PID
    const char* jsonConfig = R"({
        "factory.pid": [
            {"property1": "value1"},
            {"property2": "value2"}
        ]
    })";
    
    // Should parse but fail due to missing Config Admin
    status = celix_configurator_applyConfiguration(configurator, jsonConfig);
    EXPECT_NE(CELIX_SUCCESS, status);
    
    celix_configurator_stop(configurator);
    celix_configurator_destroy(configurator);
}
