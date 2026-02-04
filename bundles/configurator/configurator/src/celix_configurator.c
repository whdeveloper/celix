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

#include "celix_configurator.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <jansson.h>
#include <assert.h>

#include "celix_log_helper.h"
#include "celix_properties.h"
#include "celix_stdlib_cleanup.h"
#include "celix_stdio_cleanup.h"
#include "celix_utils.h"
#include "celix_err.h"
#include "configuration_admin.h"
#include "configuration.h"

struct celix_configurator {
    celix_bundle_context_t* ctx;
    celix_log_helper_t* logHelper;
    long configAdminTrkId;
    configuration_admin_service_pt configAdminSvc;
    celix_thread_mutex_t mutex; // protects configAdminSvc
};

static celix_status_t celix_configurator_parseJsonObject(celix_configurator_t* configurator,
                                                          json_t* root,
                                                          const char* pid,
                                                          bool isFactory);

static celix_status_t celix_configurator_jsonToProperties(json_t* jsonProps,
                                                           celix_properties_t** properties);

static void celix_configurator_setConfigAdminService(void* handle, void* svc) {
    celix_configurator_t* configurator = handle;
    celixThreadMutex_lock(&configurator->mutex);
    configurator->configAdminSvc = svc;
    celixThreadMutex_unlock(&configurator->mutex);
}

celix_configurator_t* celix_configurator_create(celix_bundle_context_t* ctx) {
    celix_autofree celix_configurator_t* configurator = calloc(1, sizeof(*configurator));
    if (!configurator) {
        return NULL;
    }
    
    configurator->ctx = ctx;
    configurator->logHelper = celix_logHelper_create(ctx, "celix_configurator");
    if (!configurator->logHelper) {
        return NULL;
    }
    
    celix_status_t status = celixThreadMutex_create(&configurator->mutex, NULL);
    if (status != CELIX_SUCCESS) {
        celix_logHelper_destroy(configurator->logHelper);
        return NULL;
    }
    
    return celix_steal_ptr(configurator);
}

void celix_configurator_destroy(celix_configurator_t* configurator) {
    if (configurator) {
        celixThreadMutex_destroy(&configurator->mutex);
        celix_logHelper_destroy(configurator->logHelper);
        free(configurator);
    }
}

celix_status_t celix_configurator_start(celix_configurator_t* configurator) {
    assert(configurator != NULL);
    
    // Track Configuration Admin service
    celix_service_tracking_options_t opts = CELIX_EMPTY_SERVICE_TRACKING_OPTIONS;
    opts.filter.serviceName = CONFIGURATION_ADMIN_SERVICE_NAME;
    opts.callbackHandle = configurator;
    opts.set = celix_configurator_setConfigAdminService;
    
    configurator->configAdminTrkId = celix_bundleContext_trackServicesWithOptions(configurator->ctx, &opts);
    if (configurator->configAdminTrkId < 0) {
        celix_logHelper_error(configurator->logHelper, "Failed to track Configuration Admin service");
        return CELIX_BUNDLE_EXCEPTION;
    }
    
    celix_logHelper_info(configurator->logHelper, "Configurator started");
    return CELIX_SUCCESS;
}

celix_status_t celix_configurator_stop(celix_configurator_t* configurator) {
    assert(configurator != NULL);
    
    if (configurator->configAdminTrkId >= 0) {
        celix_bundleContext_stopTracker(configurator->ctx, configurator->configAdminTrkId);
        configurator->configAdminTrkId = -1;
    }
    
    celix_logHelper_info(configurator->logHelper, "Configurator stopped");
    return CELIX_SUCCESS;
}

celix_status_t celix_configurator_applyConfiguration(celix_configurator_t* configurator, const char* jsonConfig) {
    assert(configurator != NULL);
    assert(jsonConfig != NULL);
    
    celixThreadMutex_lock(&configurator->mutex);
    configuration_admin_service_pt configAdminSvc = configurator->configAdminSvc;
    celixThreadMutex_unlock(&configurator->mutex);
    
    if (!configAdminSvc) {
        celix_logHelper_warning(configurator->logHelper, "Configuration Admin service not available");
        return CELIX_ILLEGAL_STATE;
    }
    
    json_error_t error;
    json_t* root = json_loads(jsonConfig, 0, &error);
    if (!root) {
        celix_logHelper_error(configurator->logHelper, 
                              "Failed to parse JSON configuration: %s (line %d)", 
                              error.text, error.line);
        return CELIX_ILLEGAL_ARGUMENT;
    }
    
    if (!json_is_object(root)) {
        celix_logHelper_error(configurator->logHelper, "JSON root must be an object");
        json_decref(root);
        return CELIX_ILLEGAL_ARGUMENT;
    }
    
    // Iterate over all keys in the JSON object
    const char* key;
    json_t* value;
    json_object_foreach(root, key, value) {
        bool isFactory = false;
        const char* pid = key;
        
        // Check if this is a factory PID (ends with '~')
        size_t len = strlen(key);
        if (len > 0 && key[len - 1] == '~') {
            isFactory = true;
        }
        
        if (json_is_object(value)) {
            celix_status_t status = celix_configurator_parseJsonObject(configurator, value, pid, isFactory);
            if (status != CELIX_SUCCESS) {
                celix_logHelper_warning(configurator->logHelper, 
                                        "Failed to apply configuration for PID '%s'", pid);
            }
        } else if (json_is_array(value)) {
            // Array of configurations (for factory PIDs)
            size_t index;
            json_t* configValue;
            json_array_foreach(value, index, configValue) {
                if (json_is_object(configValue)) {
                    celix_status_t status = celix_configurator_parseJsonObject(configurator, 
                                                                                configValue, 
                                                                                pid, 
                                                                                true);
                    if (status != CELIX_SUCCESS) {
                        celix_logHelper_warning(configurator->logHelper,
                                                "Failed to apply configuration %zu for factory PID '%s'", 
                                                index, pid);
                    }
                } else {
                    celix_logHelper_warning(configurator->logHelper,
                                            "Array element %zu for PID '%s' is not an object", 
                                            index, pid);
                }
            }
        } else {
            celix_logHelper_warning(configurator->logHelper, 
                                    "Configuration for PID '%s' must be an object or array", pid);
        }
    }
    
    json_decref(root);
    return CELIX_SUCCESS;
}

celix_status_t celix_configurator_applyConfigurationFile(celix_configurator_t* configurator, 
                                                          const char* configFile) {
    assert(configurator != NULL);
    assert(configFile != NULL);
    
    json_error_t error;
    json_t* root = json_load_file(configFile, 0, &error);
    if (!root) {
        celix_logHelper_error(configurator->logHelper,
                              "Failed to load configuration file '%s': %s (line %d)",
                              configFile, error.text, error.line);
        return CELIX_FILE_IO_EXCEPTION;
    }
    
    // Convert back to string and use applyConfiguration
    char* jsonString = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    
    if (!jsonString) {
        celix_logHelper_error(configurator->logHelper, "Failed to serialize JSON");
        return CELIX_ENOMEM;
    }
    
    celix_status_t status = celix_configurator_applyConfiguration(configurator, jsonString);
    free(jsonString);
    return status;
}

static celix_status_t celix_configurator_parseJsonObject(celix_configurator_t* configurator,
                                                          json_t* root,
                                                          const char* pid,
                                                          bool isFactory) {
    celix_properties_t* properties = NULL;
    celix_status_t status = celix_configurator_jsonToProperties(root, &properties);
    if (status != CELIX_SUCCESS) {
        celix_logHelper_error(configurator->logHelper, 
                              "Failed to convert JSON to properties for PID '%s'", pid);
        return status;
    }
    
    celixThreadMutex_lock(&configurator->mutex);
    configuration_admin_service_pt configAdminSvc = configurator->configAdminSvc;
    celixThreadMutex_unlock(&configurator->mutex);
    
    if (!configAdminSvc || !configAdminSvc->configAdmin) {
        celix_properties_destroy(properties);
        return CELIX_ILLEGAL_STATE;
    }
    
    configuration_pt config = NULL;
    
    if (isFactory) {
        // For factory configurations, we strip the trailing '~' if present
        celix_autofree char* factoryPid = strdup(pid);
        if (!factoryPid) {
            celix_properties_destroy(properties);
            return CELIX_ENOMEM;
        }
        size_t len = strlen(factoryPid);
        if (len > 0 && factoryPid[len - 1] == '~') {
            factoryPid[len - 1] = '\0';
        }
        
        status = configAdminSvc->createFactoryConfiguration(configAdminSvc->configAdmin, 
                                                             factoryPid, 
                                                             &config);
    } else {
        status = configAdminSvc->getConfiguration(configAdminSvc->configAdmin, 
                                                   (char*)pid, 
                                                   &config);
    }
    
    if (status != CELIX_SUCCESS || !config) {
        celix_logHelper_error(configurator->logHelper,
                              "Failed to get configuration for PID '%s'", pid);
        celix_properties_destroy(properties);
        return status;
    }
    
    // Update the configuration with the new properties
    status = config->configuration_update(config->handle, properties);
    if (status != CELIX_SUCCESS) {
        celix_logHelper_error(configurator->logHelper, 
                              "Failed to update configuration for PID '%s'", pid);
        celix_properties_destroy(properties);
        return status;
    }
    
    celix_logHelper_info(configurator->logHelper, 
                         "Applied configuration for %s '%s'",
                         isFactory ? "factory PID" : "PID", pid);
    
    return CELIX_SUCCESS;
}

static celix_status_t celix_configurator_jsonToProperties(json_t* jsonProps,
                                                           celix_properties_t** properties) {
    assert(jsonProps != NULL);
    assert(properties != NULL);
    
    *properties = celix_properties_create();
    if (!*properties) {
        return CELIX_ENOMEM;
    }
    
    const char* key;
    json_t* value;
    json_object_foreach(jsonProps, key, value) {
        if (json_is_string(value)) {
            const char* strValue = json_string_value(value);
            celix_properties_set(*properties, key, strValue);
        } else if (json_is_integer(value)) {
            long longValue = json_integer_value(value);
            celix_properties_setLong(*properties, key, longValue);
        } else if (json_is_real(value)) {
            double doubleValue = json_real_value(value);
            celix_properties_setDouble(*properties, key, doubleValue);
        } else if (json_is_boolean(value)) {
            bool boolValue = json_is_true(value);
            celix_properties_setBool(*properties, key, boolValue);
        } else if (json_is_array(value)) {
            // Convert array to comma-separated string
            char* arrayStr = json_dumps(value, JSON_COMPACT);
            if (arrayStr) {
                celix_properties_set(*properties, key, arrayStr);
                free(arrayStr);
            }
        } else if (json_is_object(value)) {
            // Convert nested object to JSON string
            char* objStr = json_dumps(value, JSON_COMPACT);
            if (objStr) {
                celix_properties_set(*properties, key, objStr);
                free(objStr);
            }
        } else if (json_is_null(value)) {
            // Skip null values
            continue;
        }
    }
    
    return CELIX_SUCCESS;
}
