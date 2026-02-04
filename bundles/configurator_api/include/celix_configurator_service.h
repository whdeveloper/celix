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

#ifndef CELIX_CONFIGURATOR_SERVICE_H
#define CELIX_CONFIGURATOR_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "celix_properties.h"
#include "celix_errno.h"

/**
 * @brief The Configurator service name
 */
#define CELIX_CONFIGURATOR_SERVICE_NAME "celix_configurator_service"

/**
 * @brief The Configurator service version
 */
#define CELIX_CONFIGURATOR_SERVICE_VERSION "1.0.0"
#define CELIX_CONFIGURATOR_SERVICE_USE_RANGE "[1.0.0,2)"

/**
 * @brief Configuration policy values
 */
#define CELIX_CONFIGURATOR_POLICY_OPTIONAL "optional"
#define CELIX_CONFIGURATOR_POLICY_REQUIRE "require"

/**
 * @brief Configuration instruction property keys
 */
#define CELIX_CONFIGURATOR_PID "pid"
#define CELIX_CONFIGURATOR_FACTORY_PID "factoryPid"
#define CELIX_CONFIGURATOR_POLICY "configurationPolicy"
#define CELIX_CONFIGURATOR_GRACE_PERIOD "gracePeriod"

/**
 * @brief The Configurator service
 * 
 * The Configurator service applies configuration instructions to the Configuration Admin service.
 * It supports per-instruction configuration policies (optional/require) and grace periods for
 * deferred component deactivation.
 * 
 * @see OSGi Compendium 8.0 Configurator specification
 */
typedef struct celix_configurator_service {
    void* handle;
    
    /**
     * @brief Apply a single configuration instruction
     * 
     * @param[in] handle The handle as provided by the service registration
     * @param[in] instruction A properties object containing the configuration instruction.
     *                       Must contain either "pid" or "factoryPid" property.
     *                       Optional properties: "configurationPolicy", "gracePeriod"
     * @return CELIX_SUCCESS if successful, error code otherwise
     */
    celix_status_t (*applyInstruction)(void* handle, const celix_properties_t* instruction);
    
    /**
     * @brief Apply multiple configuration instructions from JSON string
     * 
     * @param[in] handle The handle as provided by the service registration
     * @param[in] jsonInstructions JSON string containing array of configuration instructions
     * @return CELIX_SUCCESS if successful, error code otherwise
     */
    celix_status_t (*applyInstructionsFromJson)(void* handle, const char* jsonInstructions);
    
    /**
     * @brief Remove configuration for a PID
     * 
     * @param[in] handle The handle as provided by the service registration
     * @param[in] pid The PID to remove configuration for
     * @return CELIX_SUCCESS if successful, error code otherwise
     */
    celix_status_t (*removeConfiguration)(void* handle, const char* pid);
    
} celix_configurator_service_t;

#ifdef __cplusplus
}
#endif

#endif /* CELIX_CONFIGURATOR_SERVICE_H */
