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

#ifndef CELIX_CONFIGURATOR_SERVICE_H_
#define CELIX_CONFIGURATOR_SERVICE_H_

#include "celix_errno.h"
#include "celix_properties.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file celix_configurator_service.h
 * @brief Celix Configurator Service API
 * 
 * The Configurator service provides a way to provision configurations
 * according to the OSGi Configurator specification (Compendium 8.0).
 * It reads configuration files and applies them to services via Configuration Admin.
 */

#define CELIX_CONFIGURATOR_SERVICE_NAME "celix.configurator.ConfiguratorService"
#define CELIX_CONFIGURATOR_SERVICE_VERSION "1.0.0"
#define CELIX_CONFIGURATOR_SERVICE_USE_RANGE "[1.0.0,2.0.0)"

/**
 * @brief Configurator service handle
 */
typedef struct celix_configurator celix_configurator_t;

/**
 * @brief Configurator service structure
 */
typedef struct celix_configurator_service {
    /**
     * @brief Service handle
     */
    void* handle;
    
    /**
     * @brief Applies a configuration from a JSON string
     * 
     * @param handle Service handle
     * @param jsonConfig JSON configuration string following OSGi Configurator format
     * @return CELIX_SUCCESS on success, error code otherwise
     */
    celix_status_t (*applyConfiguration)(void* handle, const char* jsonConfig);
    
    /**
     * @brief Applies configurations from a JSON file
     * 
     * @param handle Service handle
     * @param configFile Path to JSON configuration file
     * @return CELIX_SUCCESS on success, error code otherwise
     */
    celix_status_t (*applyConfigurationFile)(void* handle, const char* configFile);
    
} celix_configurator_service_t;

#ifdef __cplusplus
}
#endif

#endif /* CELIX_CONFIGURATOR_SERVICE_H_ */
