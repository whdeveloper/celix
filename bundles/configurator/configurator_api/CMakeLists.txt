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

add_library(configurator_api INTERFACE)
target_include_directories(configurator_api INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:include/celix/configurator_api>
)
target_link_libraries(configurator_api INTERFACE Celix::utils)

add_library(Celix::configurator_api ALIAS configurator_api)

install(TARGETS configurator_api EXPORT celix COMPONENT configurator)
install(DIRECTORY include/ DESTINATION include/celix/configurator_api COMPONENT configurator)
