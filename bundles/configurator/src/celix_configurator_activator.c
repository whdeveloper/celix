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

#include <assert.h>
#include <errno.h>

#include "celix_errno.h"
#include "celix_bundle_activator.h"
#include "celix_configurator.h"
#include "celix_configurator_service.h"
#include "configuration_admin.h"
#include "celix_dm_component.h"

typedef struct celix_configurator_activator {
    celix_configurator_t* configurator;
    celix_configurator_service_t configuratorService;
    long configAdminTrackerId;
} celix_configurator_activator_t;

static celix_status_t celix_configuratorActivator_applyInstruction(void* handle, const celix_properties_t* instruction) {
    celix_configurator_t* configurator = (celix_configurator_t*)handle;
    return celix_configurator_applyInstruction(configurator, instruction);
}

static celix_status_t celix_configuratorActivator_applyInstructionsFromJson(void* handle, const char* jsonInstructions) {
    celix_configurator_t* configurator = (celix_configurator_t*)handle;
    return celix_configurator_applyInstructionsFromJson(configurator, jsonInstructions);
}

static celix_status_t celix_configuratorActivator_removeConfiguration(void* handle, const char* pid) {
    celix_configurator_t* configurator = (celix_configurator_t*)handle;
    return celix_configurator_removeConfiguration(configurator, pid);
}

static void celix_configuratorActivator_addConfigAdmin(void* handle, void* svc, const celix_properties_t* props) {
    (void)props;
    celix_configurator_t* configurator = (celix_configurator_t*)handle;
    configurator->configAdminService = (configuration_admin_service_pt)svc;
}

static void celix_configuratorActivator_removeConfigAdmin(void* handle, void* svc, const celix_properties_t* props) {
    (void)svc;
    (void)props;
    celix_configurator_t* configurator = (celix_configurator_t*)handle;
    configurator->configAdminService = NULL;
}

celix_status_t celix_configuratorActivator_start(celix_configurator_activator_t* act, celix_bundle_context_t* ctx) {
    assert(act != NULL);
    assert(ctx != NULL);
    celix_status_t status = CELIX_SUCCESS;
    
    celix_autoptr(celix_dm_component_t) configuratorCmp = celix_dmComponent_create(ctx, "CONFIGURATOR_CMP");
    if (configuratorCmp == NULL) {
        return CELIX_ENOMEM;
    }
    
    act->configurator = celix_configurator_create(ctx);
    if (act->configurator == NULL) {
        return CELIX_BUNDLE_EXCEPTION;
    }
    celix_dmComponent_setImplementation(configuratorCmp, act->configurator);
    CELIX_DM_COMPONENT_SET_CALLBACKS(configuratorCmp, celix_configurator_t, NULL, celix_configurator_start, celix_configurator_stop, NULL);
    CELIX_DM_COMPONENT_SET_IMPLEMENTATION_DESTROY_FUNCTION(configuratorCmp, celix_configurator_t, celix_configurator_destroy);
    
    // Add dependency on Configuration Admin service (optional)
    {
        celix_autoptr(celix_dm_service_dependency_t) configAdminDep = celix_dmServiceDependency_create();
        if (configAdminDep == NULL) {
            return CELIX_ENOMEM;
        }
        status = celix_dmServiceDependency_setService(configAdminDep, CONFIGURATION_ADMIN_SERVICE_NAME, NULL, NULL);
        if (status != CELIX_SUCCESS) {
            return status;
        }
        celix_dmServiceDependency_setRequired(configAdminDep, false);
        celix_dmServiceDependency_setStrategy(configAdminDep, DM_SERVICE_DEPENDENCY_STRATEGY_LOCKING);
        celix_dm_service_dependency_callback_options_t opts = CELIX_EMPTY_DM_SERVICE_DEPENDENCY_CALLBACK_OPTIONS;
        opts.addWithProps = celix_configuratorActivator_addConfigAdmin;
        opts.removeWithProps = celix_configuratorActivator_removeConfigAdmin;
        celix_dmServiceDependency_setCallbacksWithOptions(configAdminDep, &opts);
        status = celix_dmComponent_addServiceDependency(configuratorCmp, configAdminDep);
        if (status != CELIX_SUCCESS) {
            return status;
        }
        celix_steal_ptr(configAdminDep);
    }
    
    // Register configurator service
    act->configuratorService.handle = act->configurator;
    act->configuratorService.applyInstruction = celix_configuratorActivator_applyInstruction;
    act->configuratorService.applyInstructionsFromJson = celix_configuratorActivator_applyInstructionsFromJson;
    act->configuratorService.removeConfiguration = celix_configuratorActivator_removeConfiguration;
    status = celix_dmComponent_addInterface(configuratorCmp, CELIX_CONFIGURATOR_SERVICE_NAME, CELIX_CONFIGURATOR_SERVICE_VERSION, &act->configuratorService, NULL);
    if (status != CELIX_SUCCESS) {
        return status;
    }
    
    // Add component to dependency manager
    celix_dependency_manager_t* dm = celix_bundleContext_getDependencyManager(ctx);
    status = celix_dependencyManager_addAsync(dm, configuratorCmp);
    if (status == CELIX_SUCCESS) {
        celix_steal_ptr(configuratorCmp);
    }
    
    return status;
}

celix_status_t celix_configuratorActivator_stop(celix_configurator_activator_t* act, celix_bundle_context_t* ctx) {
    (void)act;
    (void)ctx;
    return CELIX_SUCCESS;
}

CELIX_GEN_BUNDLE_ACTIVATOR(celix_configurator_activator_t, celix_configuratorActivator_start, celix_configuratorActivator_stop)
