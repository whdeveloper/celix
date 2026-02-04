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

#ifndef CELIX_CONFIGURATOR_H
#define CELIX_CONFIGURATOR_H

#include "celix_bundle_context.h"
#include "celix_configurator_service.h"
#include "celix_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque configurator handle
 */
typedef struct celix_configurator celix_configurator_t;

/**
 * @brief Create a new configurator instance
 * 
 * @param ctx The bundle context
 * @return A new configurator instance or NULL on failure
 */
celix_configurator_t* celix_configurator_create(celix_bundle_context_t* ctx);

/**
 * @brief Start the configurator
 * 
 * @param configurator The configurator instance
 * @return CELIX_SUCCESS on success, error code otherwise
 */
celix_status_t celix_configurator_start(celix_configurator_t* configurator);

/**
 * @brief Stop the configurator
 * 
 * @param configurator The configurator instance
 * @return CELIX_SUCCESS on success, error code otherwise
 */
celix_status_t celix_configurator_stop(celix_configurator_t* configurator);

/**
 * @brief Destroy the configurator instance
 * 
 * @param configurator The configurator instance
 */
void celix_configurator_destroy(celix_configurator_t* configurator);

/**
 * @brief Apply a configuration instruction
 * 
 * @param configurator The configurator instance
 * @param instruction The configuration instruction as properties
 * @return CELIX_SUCCESS on success, error code otherwise
 */
celix_status_t celix_configurator_applyInstruction(celix_configurator_t* configurator, const celix_properties_t* instruction);

/**
 * @brief Apply multiple configuration instructions from JSON
 * 
 * @param configurator The configurator instance
 * @param jsonInstructions JSON string containing configuration instructions
 * @return CELIX_SUCCESS on success, error code otherwise
 */
celix_status_t celix_configurator_applyInstructionsFromJson(celix_configurator_t* configurator, const char* jsonInstructions);

/**
 * @brief Remove configuration for a PID
 * 
 * @param configurator The configurator instance
 * @param pid The PID to remove
 * @return CELIX_SUCCESS on success, error code otherwise
 */
celix_status_t celix_configurator_removeConfiguration(celix_configurator_t* configurator, const char* pid);

#ifdef __cplusplus
}
#endif

#endif /* CELIX_CONFIGURATOR_H */
