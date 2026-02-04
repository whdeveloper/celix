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

#ifndef CELIX_CONFIGURATOR_H_
#define CELIX_CONFIGURATOR_H_

#include "celix_bundle_context.h"
#include "celix_errno.h"
#include "celix_log_helper.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configurator implementation structure
 */
typedef struct celix_configurator celix_configurator_t;

/**
 * @brief Creates a new configurator instance
 * 
 * @param ctx Bundle context
 * @return New configurator instance or NULL on failure
 */
celix_configurator_t* celix_configurator_create(celix_bundle_context_t* ctx);

/**
 * @brief Destroys a configurator instance
 * 
 * @param configurator Configurator instance
 */
void celix_configurator_destroy(celix_configurator_t* configurator);

/**
 * @brief Starts the configurator service
 * 
 * @param configurator Configurator instance
 * @return CELIX_SUCCESS on success, error code otherwise
 */
celix_status_t celix_configurator_start(celix_configurator_t* configurator);

/**
 * @brief Stops the configurator service
 * 
 * @param configurator Configurator instance
 * @return CELIX_SUCCESS on success, error code otherwise
 */
celix_status_t celix_configurator_stop(celix_configurator_t* configurator);

/**
 * @brief Applies a configuration from a JSON string
 * 
 * @param configurator Configurator instance
 * @param jsonConfig JSON configuration string
 * @return CELIX_SUCCESS on success, error code otherwise
 */
celix_status_t celix_configurator_applyConfiguration(celix_configurator_t* configurator, const char* jsonConfig);

/**
 * @brief Applies configurations from a JSON file
 * 
 * @param configurator Configurator instance
 * @param configFile Path to JSON configuration file
 * @return CELIX_SUCCESS on success, error code otherwise
 */
celix_status_t celix_configurator_applyConfigurationFile(celix_configurator_t* configurator, const char* configFile);

#ifdef __cplusplus
}
#endif

#endif /* CELIX_CONFIGURATOR_H_ */
