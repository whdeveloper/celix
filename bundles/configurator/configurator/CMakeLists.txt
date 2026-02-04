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
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

find_package(jansson REQUIRED)

set(CONFIGURATOR_SRC
    src/celix_configurator_activator.c
    src/celix_configurator.c
)

set(CONFIGURATOR_DEPS
    Celix::configurator_api
    Celix::log_helper
    Celix::framework
    Celix::utils
    jansson::jansson
)

add_celix_bundle(configurator
    SYMBOLIC_NAME "apache_celix_configurator"
    VERSION "1.0.0"
    NAME "Apache Celix Configurator"
    GROUP "Celix/Configurator"
    FILENAME celix_configurator
    SOURCES
        ${CONFIGURATOR_SRC}
)

target_include_directories(configurator PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${PROJECT_SOURCE_DIR}/misc/experimental/bundles/config_admin/service/public/include
)

target_link_libraries(configurator PRIVATE ${CONFIGURATOR_DEPS})

install_celix_bundle(configurator EXPORT celix COMPONENT configurator)

# Setup target alias to match external usage
add_library(Celix::configurator ALIAS configurator)

if (ENABLE_TESTING)
    add_library(configurator_cut STATIC ${CONFIGURATOR_SRC})
    target_include_directories(configurator_cut PUBLIC 
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${PROJECT_SOURCE_DIR}/misc/experimental/bundles/config_admin/service/public/include
    )
    target_link_libraries(configurator_cut PUBLIC ${CONFIGURATOR_DEPS})
    add_subdirectory(gtest)
endif()
