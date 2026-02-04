# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
# 
#   http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

add_celix_bundle(configurator
    VERSION 1.0.0
    SOURCES
        src/celix_configurator.c
        src/celix_configurator_activator.c
)

target_include_directories(configurator PRIVATE src)
target_link_libraries(configurator PRIVATE
    Celix::configurator_api
    Celix::framework
    Celix::log_helper
    Celix::utils
    Celix::threads
    Celix::dm
    jansson::jansson
)

# Link to config_admin API
target_include_directories(configurator PRIVATE 
    ${CMAKE_SOURCE_DIR}/misc/experimental/bundles/config_admin/service/public/include
)

install_celix_bundle(configurator EXPORT celix COMPONENT configurator)

if (ENABLE_TESTING)
    add_subdirectory(gtest)
endif()
