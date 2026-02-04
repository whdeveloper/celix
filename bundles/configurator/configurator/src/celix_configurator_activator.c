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

#include <assert.h>

#include "celix_bundle_activator.h"
#include "celix_configurator.h"
#include "celix_configurator_service.h"

typedef struct celix_configurator_activator {
    celix_configurator_t* configurator;
    celix_configurator_service_t configuratorService;
    long svcId;
} celix_configurator_activator_t;

static celix_status_t celix_configuratorActivator_applyConfiguration(void* handle, const char* jsonConfig) {
    celix_configurator_t* configurator = handle;
    return celix_configurator_applyConfiguration(configurator, jsonConfig);
}

static celix_status_t celix_configuratorActivator_applyConfigurationFile(void* handle, const char* configFile) {
    celix_configurator_t* configurator = handle;
    return celix_configurator_applyConfigurationFile(configurator, configFile);
}

static celix_status_t celix_configuratorActivator_start(celix_configurator_activator_t* act, 
                                                         celix_bundle_context_t* ctx) {
    assert(act != NULL);
    assert(ctx != NULL);
    
    act->configurator = celix_configurator_create(ctx);
    if (!act->configurator) {
        return CELIX_ENOMEM;
    }
    
    celix_status_t status = celix_configurator_start(act->configurator);
    if (status != CELIX_SUCCESS) {
        celix_configurator_destroy(act->configurator);
        act->configurator = NULL;
        return status;
    }
    
    // Register configurator service
    act->configuratorService.handle = act->configurator;
    act->configuratorService.applyConfiguration = celix_configuratorActivator_applyConfiguration;
    act->configuratorService.applyConfigurationFile = celix_configuratorActivator_applyConfigurationFile;
    
    celix_service_registration_options_t opts = CELIX_EMPTY_SERVICE_REGISTRATION_OPTIONS;
    opts.serviceName = CELIX_CONFIGURATOR_SERVICE_NAME;
    opts.serviceVersion = CELIX_CONFIGURATOR_SERVICE_VERSION;
    opts.svc = &act->configuratorService;
    
    act->svcId = celix_bundleContext_registerServiceWithOptions(ctx, &opts);
    if (act->svcId < 0) {
        celix_configurator_stop(act->configurator);
        celix_configurator_destroy(act->configurator);
        act->configurator = NULL;
        return CELIX_BUNDLE_EXCEPTION;
    }
    
    return CELIX_SUCCESS;
}

static celix_status_t celix_configuratorActivator_stop(celix_configurator_activator_t* act, 
                                                        celix_bundle_context_t* ctx) {
    assert(act != NULL);
    
    if (act->svcId >= 0) {
        celix_bundleContext_unregisterService(ctx, act->svcId);
        act->svcId = -1;
    }
    
    if (act->configurator) {
        celix_configurator_stop(act->configurator);
        celix_configurator_destroy(act->configurator);
        act->configurator = NULL;
    }
    
    return CELIX_SUCCESS;
}

CELIX_GEN_BUNDLE_ACTIVATOR(celix_configurator_activator_t, 
                           celix_configuratorActivator_start, 
                           celix_configuratorActivator_stop)
