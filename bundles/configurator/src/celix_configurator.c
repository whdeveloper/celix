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

#include "celix_configurator.h"
#include "celix_configurator_service.h"
#include "celix_bundle_context.h"
#include "celix_log_helper.h"
#include "celix_threads.h"
#include "celix_utils.h"
#include "celix_array_list.h"
#include "celix_string_hash_map.h"
#include "configuration_admin.h"
#include "managed_service.h"
#include <jansson.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

typedef struct celix_configurator_pid_info {
    char* pid;
    char* policy;               // "optional" or "require"
    long gracePeriodMs;         // grace period in milliseconds, -1 if not set
    bool isFactory;
    bool configPresent;         // whether configuration is currently present
    bool componentBlocked;      // whether component is blocked from activation
    struct timespec gracePeriodExpiry; // when grace period expires
    long gracePeriodTimerId;    // timer ID for grace period
} celix_configurator_pid_info_t;

struct celix_configurator {
    celix_bundle_context_t* ctx;
    celix_log_helper_t* logHelper;
    
    // Tracking configuration admin service
    long configAdminTrackerId;
    configuration_admin_service_pt configAdminService;
    
    // Tracking managed services to control their lifecycle
    long managedServiceTrackerId;
    
    // Map of PID -> celix_configurator_pid_info_t* for policy tracking
    celix_string_hash_map_t* pidInfoMap;
    celix_thread_rwlock_t pidInfoLock;
    
    // Thread for grace period timer management
    celix_thread_t timerThread;
    celix_thread_mutex_t timerMutex;
    celix_thread_cond_t timerCond;
    bool timerThreadRunning;
    celix_array_list_t* pendingGracePeriods; // list of PIDs with active grace periods
};

static void celix_configurator_pidInfoDestroy(celix_configurator_pid_info_t* info) {
    if (info) {
        free(info->pid);
        free(info->policy);
        free(info);
    }
}

static void* celix_configurator_timerThreadFunc(void* data);

celix_configurator_t* celix_configurator_create(celix_bundle_context_t* ctx) {
    celix_configurator_t* configurator = calloc(1, sizeof(*configurator));
    if (!configurator) {
        return NULL;
    }
    
    configurator->ctx = ctx;
    configurator->logHelper = celix_logHelper_create(ctx, "celix_configurator");
    configurator->pidInfoMap = celix_stringHashMap_create();
    celixThreadRwlock_create(&configurator->pidInfoLock, NULL);
    celixThreadMutex_create(&configurator->timerMutex, NULL);
    celixThreadCondition_init(&configurator->timerCond, NULL);
    configurator->pendingGracePeriods = celix_arrayList_create();
    configurator->timerThreadRunning = false;
    
    if (!configurator->logHelper || !configurator->pidInfoMap || !configurator->pendingGracePeriods) {
        celix_configurator_destroy(configurator);
        return NULL;
    }
    
    return configurator;
}

celix_status_t celix_configurator_start(celix_configurator_t* configurator) {
    if (!configurator) {
        return CELIX_ILLEGAL_ARGUMENT;
    }
    
    celix_logHelper_info(configurator->logHelper, "Starting Configurator service");
    
    // Start timer thread for grace period handling
    configurator->timerThreadRunning = true;
    int rc = celixThread_create(&configurator->timerThread, NULL, celix_configurator_timerThreadFunc, configurator);
    if (rc != 0) {
        celix_logHelper_error(configurator->logHelper, "Failed to create timer thread");
        configurator->timerThreadRunning = false;
        return CELIX_BUNDLE_EXCEPTION;
    }
    
    return CELIX_SUCCESS;
}

celix_status_t celix_configurator_stop(celix_configurator_t* configurator) {
    if (!configurator) {
        return CELIX_ILLEGAL_ARGUMENT;
    }
    
    celix_logHelper_info(configurator->logHelper, "Stopping Configurator service");
    
    // Stop timer thread
    if (configurator->timerThreadRunning) {
        celixThreadMutex_lock(&configurator->timerMutex);
        configurator->timerThreadRunning = false;
        celixThreadCondition_broadcast(&configurator->timerCond);
        celixThreadMutex_unlock(&configurator->timerMutex);
        celixThread_join(configurator->timerThread, NULL);
    }
    
    return CELIX_SUCCESS;
}

void celix_configurator_destroy(celix_configurator_t* configurator) {
    if (!configurator) {
        return;
    }
    
    if (configurator->pidInfoMap) {
        CELIX_STRING_HASH_MAP_ITERATE(configurator->pidInfoMap, iter) {
            celix_configurator_pidInfoDestroy((celix_configurator_pid_info_t*)iter.value.ptrValue);
        }
        celix_stringHashMap_destroy(configurator->pidInfoMap);
    }
    
    if (configurator->pendingGracePeriods) {
        celix_arrayList_destroy(configurator->pendingGracePeriods);
    }
    
    celixThreadRwlock_destroy(&configurator->pidInfoLock);
    celixThreadMutex_destroy(&configurator->timerMutex);
    celixThreadCondition_destroy(&configurator->timerCond);
    celix_logHelper_destroy(configurator->logHelper);
    free(configurator);
}

static celix_status_t celix_configurator_parseConfigProperties(const celix_properties_t* instruction, celix_properties_t** configProps) {
    // Extract configuration properties (all properties except our control properties)
    celix_properties_t* props = celix_properties_create();
    if (!props) {
        return CELIX_ENOMEM;
    }
    
    CELIX_PROPERTIES_ITERATE(instruction, iter) {
        const char* key = iter.key;
        // Skip control properties
        if (strcmp(key, CELIX_CONFIGURATOR_PID) != 0 &&
            strcmp(key, CELIX_CONFIGURATOR_FACTORY_PID) != 0 &&
            strcmp(key, CELIX_CONFIGURATOR_POLICY) != 0 &&
            strcmp(key, CELIX_CONFIGURATOR_GRACE_PERIOD) != 0) {
            celix_properties_set(props, key, iter.entry.value);
        }
    }
    
    *configProps = props;
    return CELIX_SUCCESS;
}

static void celix_configurator_scheduleGracePeriod(celix_configurator_t* configurator, const char* pid, long gracePeriodMs) {
    celixThreadMutex_lock(&configurator->timerMutex);
    
    // Add to pending grace periods
    char* pidCopy = celix_utils_strdup(pid);
    if (pidCopy) {
        celix_arrayList_add(configurator->pendingGracePeriods, pidCopy);
        celixThreadCondition_broadcast(&configurator->timerCond);
    }
    
    celixThreadMutex_unlock(&configurator->timerMutex);
}

static void celix_configurator_cancelGracePeriod(celix_configurator_t* configurator, const char* pid) {
    celixThreadMutex_lock(&configurator->timerMutex);
    
    // Remove from pending grace periods
    for (int i = 0; i < celix_arrayList_size(configurator->pendingGracePeriods); i++) {
        char* entry = (char*)celix_arrayList_get(configurator->pendingGracePeriods, i);
        if (strcmp(entry, pid) == 0) {
            celix_arrayList_removeAt(configurator->pendingGracePeriods, i);
            free(entry);
            break;
        }
    }
    
    celixThreadMutex_unlock(&configurator->timerMutex);
}

static void* celix_configurator_timerThreadFunc(void* data) {
    celix_configurator_t* configurator = (celix_configurator_t*)data;
    
    celixThreadMutex_lock(&configurator->timerMutex);
    
    while (configurator->timerThreadRunning) {
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        
        // Check for expired grace periods
        celixThreadRwlock_writeLock(&configurator->pidInfoLock);
        
        CELIX_STRING_HASH_MAP_ITERATE(configurator->pidInfoMap, iter) {
            celix_configurator_pid_info_t* info = (celix_configurator_pid_info_t*)iter.value.ptrValue;
            if (info->gracePeriodMs > 0 && !info->configPresent) {
                // Check if grace period has expired
                if (now.tv_sec > info->gracePeriodExpiry.tv_sec ||
                    (now.tv_sec == info->gracePeriodExpiry.tv_sec && now.tv_nsec >= info->gracePeriodExpiry.tv_nsec)) {
                    // Grace period expired, deactivate component
                    celix_logHelper_info(configurator->logHelper, "Grace period expired for PID %s, deactivating component", info->pid);
                    info->componentBlocked = true;
                    info->gracePeriodMs = -1; // Clear grace period
                    
                    // Remove from pending list
                    for (int i = 0; i < celix_arrayList_size(configurator->pendingGracePeriods); i++) {
                        char* entry = (char*)celix_arrayList_get(configurator->pendingGracePeriods, i);
                        if (strcmp(entry, info->pid) == 0) {
                            celix_arrayList_removeAt(configurator->pendingGracePeriods, i);
                            free(entry);
                            break;
                        }
                    }
                }
            }
        }
        
        celixThreadRwlock_unlock(&configurator->pidInfoLock);
        
        // Wait for next check (100ms intervals) or until signaled
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_nsec += 100000000; // 100ms
        if (timeout.tv_nsec >= 1000000000) {
            timeout.tv_sec += 1;
            timeout.tv_nsec -= 1000000000;
        }
        
        celixThreadCondition_waitUntil(&configurator->timerCond, &configurator->timerMutex, &timeout);
    }
    
    celixThreadMutex_unlock(&configurator->timerMutex);
    return NULL;
}

celix_status_t celix_configurator_applyInstruction(celix_configurator_t* configurator, const celix_properties_t* instruction) {
    if (!configurator || !instruction) {
        return CELIX_ILLEGAL_ARGUMENT;
    }
    
    // Extract PID or factory PID
    const char* pid = celix_properties_get(instruction, CELIX_CONFIGURATOR_PID, NULL);
    const char* factoryPid = celix_properties_get(instruction, CELIX_CONFIGURATOR_FACTORY_PID, NULL);
    bool isFactory = (factoryPid != NULL);
    
    if (!pid && !factoryPid) {
        celix_logHelper_error(configurator->logHelper, "Configuration instruction missing both pid and factoryPid");
        return CELIX_ILLEGAL_ARGUMENT;
    }
    
    const char* effectivePid = pid ? pid : factoryPid;
    
    // Extract policy and grace period
    const char* policy = celix_properties_get(instruction, CELIX_CONFIGURATOR_POLICY, CELIX_CONFIGURATOR_POLICY_OPTIONAL);
    long gracePeriodMs = celix_properties_getAsLong(instruction, CELIX_CONFIGURATOR_GRACE_PERIOD, -1);
    
    // Validate policy
    if (strcmp(policy, CELIX_CONFIGURATOR_POLICY_OPTIONAL) != 0 &&
        strcmp(policy, CELIX_CONFIGURATOR_POLICY_REQUIRE) != 0) {
        celix_logHelper_warning(configurator->logHelper, "Invalid configurationPolicy '%s', using 'optional'", policy);
        policy = CELIX_CONFIGURATOR_POLICY_OPTIONAL;
    }
    
    celix_logHelper_info(configurator->logHelper, "Applying configuration for PID %s (policy=%s, gracePeriod=%ldms)", 
                         effectivePid, policy, gracePeriodMs);
    
    // Extract configuration properties
    celix_properties_t* configProps = NULL;
    celix_status_t status = celix_configurator_parseConfigProperties(instruction, &configProps);
    if (status != CELIX_SUCCESS) {
        return status;
    }
    
    // Apply to Config Admin if available
    if (configurator->configAdminService) {
        configuration_pt config = NULL;
        if (isFactory) {
            status = configurator->configAdminService->createFactoryConfiguration(
                configurator->configAdminService->configAdmin, (char*)factoryPid, &config);
        } else {
            status = configurator->configAdminService->getConfiguration(
                configurator->configAdminService->configAdmin, (char*)pid, &config);
        }
        
        if (status == CELIX_SUCCESS && config) {
            status = config->configuration_update(config->handle, configProps);
            if (status == CELIX_SUCCESS) {
                celix_logHelper_info(configurator->logHelper, "Successfully applied configuration for PID %s", effectivePid);
            } else {
                celix_logHelper_error(configurator->logHelper, "Failed to update configuration for PID %s", effectivePid);
                celix_properties_destroy(configProps);
                return status;
            }
        } else {
            celix_logHelper_warning(configurator->logHelper, "Config Admin service not available or failed to get configuration");
            celix_properties_destroy(configProps);
            return CELIX_ILLEGAL_STATE;
        }
    } else {
        celix_logHelper_warning(configurator->logHelper, "Config Admin service not available, configuration not applied");
        celix_properties_destroy(configProps);
    }
    
    // Track PID info for lifecycle control
    celixThreadRwlock_writeLock(&configurator->pidInfoLock);
    
    celix_configurator_pid_info_t* info = celix_stringHashMap_get(configurator->pidInfoMap, effectivePid);
    if (!info) {
        info = calloc(1, sizeof(*info));
        if (info) {
            info->pid = celix_utils_strdup(effectivePid);
            info->policy = celix_utils_strdup(policy);
            info->isFactory = isFactory;
            info->gracePeriodMs = gracePeriodMs;
            info->configPresent = true;
            info->componentBlocked = false;
            celix_stringHashMap_put(configurator->pidInfoMap, effectivePid, info);
        }
    } else {
        // Update existing info
        free(info->policy);
        info->policy = celix_utils_strdup(policy);
        info->gracePeriodMs = gracePeriodMs;
        info->configPresent = true;
        info->componentBlocked = false;
        
        // Cancel any pending grace period
        celix_configurator_cancelGracePeriod(configurator, effectivePid);
    }
    
    celixThreadRwlock_unlock(&configurator->pidInfoLock);
    
    return CELIX_SUCCESS;
}

celix_status_t celix_configurator_applyInstructionsFromJson(celix_configurator_t* configurator, const char* jsonInstructions) {
    if (!configurator || !jsonInstructions) {
        return CELIX_ILLEGAL_ARGUMENT;
    }
    
    json_error_t error;
    json_t* root = json_loads(jsonInstructions, 0, &error);
    if (!root) {
        celix_logHelper_error(configurator->logHelper, "Failed to parse JSON: %s", error.text);
        return CELIX_ILLEGAL_ARGUMENT;
    }
    
    if (!json_is_array(root)) {
        celix_logHelper_error(configurator->logHelper, "JSON root must be an array");
        json_decref(root);
        return CELIX_ILLEGAL_ARGUMENT;
    }
    
    size_t index;
    json_t* value;
    celix_status_t overallStatus = CELIX_SUCCESS;
    
    json_array_foreach(root, index, value) {
        if (!json_is_object(value)) {
            celix_logHelper_warning(configurator->logHelper, "Skipping non-object instruction at index %zu", index);
            continue;
        }
        
        // Convert JSON object to properties
        celix_properties_t* props = celix_properties_create();
        if (!props) {
            overallStatus = CELIX_ENOMEM;
            break;
        }
        
        const char* key;
        json_t* val;
        json_object_foreach(value, key, val) {
            if (json_is_string(val)) {
                celix_properties_set(props, key, json_string_value(val));
            } else if (json_is_integer(val)) {
                celix_properties_setLong(props, key, json_integer_value(val));
            } else if (json_is_real(val)) {
                celix_properties_setDouble(props, key, json_real_value(val));
            } else if (json_is_boolean(val)) {
                celix_properties_setBool(props, key, json_boolean_value(val));
            }
            // Skip arrays and nested objects
        }
        
        celix_status_t status = celix_configurator_applyInstruction(configurator, props);
        celix_properties_destroy(props);
        
        if (status != CELIX_SUCCESS) {
            celix_logHelper_warning(configurator->logHelper, "Failed to apply instruction at index %zu", index);
            overallStatus = status;
        }
    }
    
    json_decref(root);
    return overallStatus;
}

celix_status_t celix_configurator_removeConfiguration(celix_configurator_t* configurator, const char* pid) {
    if (!configurator || !pid) {
        return CELIX_ILLEGAL_ARGUMENT;
    }
    
    celix_logHelper_info(configurator->logHelper, "Removing configuration for PID %s", pid);
    
    // Remove from Config Admin
    if (configurator->configAdminService) {
        configuration_pt config = NULL;
        celix_status_t status = configurator->configAdminService->getConfiguration(
            configurator->configAdminService->configAdmin, (char*)pid, &config);
        
        if (status == CELIX_SUCCESS && config) {
            status = config->configuration_delete(config->handle);
            if (status == CELIX_SUCCESS) {
                celix_logHelper_info(configurator->logHelper, "Successfully removed configuration for PID %s", pid);
            }
        }
    }
    
    // Handle lifecycle based on policy
    celixThreadRwlock_writeLock(&configurator->pidInfoLock);
    
    celix_configurator_pid_info_t* info = celix_stringHashMap_get(configurator->pidInfoMap, pid);
    if (info) {
        info->configPresent = false;
        
        // Check if component should be deactivated
        if (strcmp(info->policy, CELIX_CONFIGURATOR_POLICY_REQUIRE) == 0) {
            if (info->gracePeriodMs > 0) {
                // Schedule grace period
                struct timespec now;
                clock_gettime(CLOCK_REALTIME, &now);
                info->gracePeriodExpiry.tv_sec = now.tv_sec + (info->gracePeriodMs / 1000);
                info->gracePeriodExpiry.tv_nsec = now.tv_nsec + ((info->gracePeriodMs % 1000) * 1000000);
                if (info->gracePeriodExpiry.tv_nsec >= 1000000000) {
                    info->gracePeriodExpiry.tv_sec += 1;
                    info->gracePeriodExpiry.tv_nsec -= 1000000000;
                }
                
                celix_logHelper_info(configurator->logHelper, "Scheduling grace period of %ldms for PID %s", info->gracePeriodMs, pid);
                celix_configurator_scheduleGracePeriod(configurator, pid, info->gracePeriodMs);
            } else {
                // Immediate deactivation
                celix_logHelper_info(configurator->logHelper, "Immediately deactivating component for PID %s (required configuration removed)", pid);
                info->componentBlocked = true;
            }
        }
    }
    
    celixThreadRwlock_unlock(&configurator->pidInfoLock);
    
    return CELIX_SUCCESS;
}
