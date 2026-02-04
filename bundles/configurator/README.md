# Celix Configurator Service

## Overview

The Celix Configurator service implements the OSGi Compendium 8.0 Configurator specification adapted for Apache Celix. It processes configuration instructions and applies them to the Configuration Admin service, enabling centralized configuration management with sophisticated lifecycle control.

## Features

- **Configuration Instructions**: Apply configurations via JSON or properties-based instructions
- **PID and Factory PID Support**: Configure both regular services and factory-created instances
- **Configuration Policies**: Control component activation based on configuration availability
  - `optional` (default): Component can start without configuration
  - `require`: Component cannot start without configuration; deactivates when configuration is removed
- **Grace Period**: Allow transient configuration removals without immediately deactivating components
- **OSGi Compatibility**: Integrates with Celix's Configuration Admin service

## Service API

### Service Name
`celix_configurator_service`

### Methods

#### applyInstruction
```c
celix_status_t (*applyInstruction)(void* handle, const celix_properties_t* instruction);
```
Applies a single configuration instruction.

#### applyInstructionsFromJson
```c
celix_status_t (*applyInstructionsFromJson)(void* handle, const char* jsonInstructions);
```
Applies multiple configuration instructions from a JSON array.

#### removeConfiguration
```c
celix_status_t (*removeConfiguration)(void* handle, const char* pid);
```
Removes configuration for the specified PID.

## Configuration Instruction Format

### Required Properties

- **pid**: The persistent identifier for the configuration (mutually exclusive with factoryPid)
- **factoryPid**: The factory PID for factory configurations (mutually exclusive with pid)

### Optional Properties

- **configurationPolicy**: Controls component lifecycle behavior
  - `"optional"` (default): Component starts regardless of configuration availability
  - `"require"`: Component blocked from starting until configuration is available; deactivated when configuration is removed

- **gracePeriod**: Time in milliseconds to wait before deactivating a component when its required configuration is removed
  - Default: Not set (immediate deactivation)
  - If set, allows transient configuration removals without disrupting the component
  - If configuration is re-applied before the grace period expires, deactivation is canceled

### Configuration Properties

All other properties in the instruction are treated as configuration properties and passed to the Configuration Admin service.

## JSON Format

Configuration instructions can be provided as a JSON array:

```json
[
  {
    "pid": "com.example.service",
    "configurationPolicy": "optional",
    "key1": "value1",
    "key2": "value2"
  },
  {
    "pid": "com.example.critical.service",
    "configurationPolicy": "require",
    "gracePeriod": 5000,
    "timeout": 30,
    "retries": 3
  },
  {
    "factoryPid": "com.example.factory",
    "configurationPolicy": "optional",
    "instance": "default"
  }
]
```

## Usage Examples

### Example 1: Optional Configuration

```c
celix_properties_t* instruction = celix_properties_create();
celix_properties_set(instruction, CELIX_CONFIGURATOR_PID, "my.service");
celix_properties_set(instruction, CELIX_CONFIGURATOR_POLICY, CELIX_CONFIGURATOR_POLICY_OPTIONAL);
celix_properties_set(instruction, "host", "localhost");
celix_properties_setLong(instruction, "port", 8080);

configuratorService->applyInstruction(configuratorService->handle, instruction);
celix_properties_destroy(instruction);
```

### Example 2: Required Configuration with Grace Period

```c
celix_properties_t* instruction = celix_properties_create();
celix_properties_set(instruction, CELIX_CONFIGURATOR_PID, "critical.service");
celix_properties_set(instruction, CELIX_CONFIGURATOR_POLICY, CELIX_CONFIGURATOR_POLICY_REQUIRE);
celix_properties_setLong(instruction, CELIX_CONFIGURATOR_GRACE_PERIOD, 10000); // 10 seconds
celix_properties_set(instruction, "database", "jdbc:mysql://localhost/db");

configuratorService->applyInstruction(configuratorService->handle, instruction);
celix_properties_destroy(instruction);
```

### Example 3: JSON-based Configuration

```c
const char* json = R"([
    {
        "pid": "http.server",
        "configurationPolicy": "require",
        "gracePeriod": 5000,
        "port": 8080,
        "maxConnections": 100
    }
])";

configuratorService->applyInstructionsFromJson(configuratorService->handle, json);
```

## Component Lifecycle Behavior

### Optional Policy
- Component starts immediately, regardless of configuration availability
- Configuration updates are applied when available
- Configuration removal does not affect component activation

### Require Policy
- Component **cannot start** until configuration is present
- Configuration removal **immediately deactivates** the component (unless grace period is set)
- With grace period: Component remains active during the grace period; if configuration is re-applied before expiry, deactivation is canceled

### Grace Period Behavior

When a required configuration is removed:

1. **No Grace Period Set**: Component is immediately deactivated
2. **Grace Period = 0**: Component is immediately deactivated
3. **Grace Period > 0**: 
   - Deactivation is scheduled after the specified time
   - If configuration is re-applied before the timer expires, deactivation is canceled
   - Timer resolution: 100ms intervals

## Dependencies

- Configuration Admin Service (optional): If not available, instructions are tracked but not applied
- The configurator will wait for Configuration Admin to become available and apply pending configurations

## Building

The configurator is built as part of the standard Celix build:

```bash
mkdir build && cd build
cmake ..
make configurator
```

## Testing

Run the configurator tests:

```bash
cd build
ctest --output-on-failure -R test_configurator
```

## License

Licensed under the Apache License, Version 2.0. See the LICENSE file for details.
