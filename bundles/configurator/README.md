# Apache Celix Configurator

## Overview

The Configurator bundle provides a service that provisions configurations according to the OSGi Configurator specification (Compendium 8.0). It reads JSON configuration files and applies them to services via the Configuration Admin service.

## Features

- Parses JSON configuration files following OSGi Configurator format
- Supports both singleton and factory configurations
- Integrates with Configuration Admin service
- Handles various property types: strings, integers, doubles, booleans, arrays, and nested objects
- Provides programmatic API for applying configurations

## Configuration Format

The configurator uses JSON format for configuration files. The JSON root object contains PIDs as keys, with their configurations as values.

### Example: Basic Configuration

```json
{
  "my.service.pid": {
    "property1": "value1",
    "property2": 42,
    "property3": true,
    "property4": 3.14
  }
}
```

### Example: Factory Configuration

Factory configurations are identified by a PID ending with `~`:

```json
{
  "my.factory.pid~": {
    "instance.property": "value"
  }
}
```

Or as an array for multiple instances:

```json
{
  "my.factory.pid": [
    {"name": "instance1", "value": 1},
    {"name": "instance2", "value": 2}
  ]
}
```

### Supported Property Types

- **String**: `"property": "value"`
- **Integer**: `"property": 42`
- **Double**: `"property": 3.14`
- **Boolean**: `"property": true`
- **Array**: `"property": [1, 2, 3]` (stored as JSON string)
- **Object**: `"property": {"nested": "value"}` (stored as JSON string)

## Usage

### As a Bundle

Include the configurator bundle in your Celix container:

```cmake
add_celix_container(my_container
    BUNDLES
        Celix::configurator
        # other bundles...
)
```

### Programmatic API

The configurator provides a service interface for programmatic configuration:

```c
#include "celix_configurator_service.h"

// Apply configuration from JSON string
celix_configurator_service_t* svc = /* obtain service */;
const char* json = "{\"my.pid\": {\"prop\": \"value\"}}";
status = svc->applyConfiguration(svc->handle, json);

// Apply configuration from file
status = svc->applyConfigurationFile(svc->handle, "/path/to/config.json");
```

## Dependencies

- **Configuration Admin**: The configurator requires a Configuration Admin service to be available. The experimental Configuration Admin service in `misc/experimental/bundles/config_admin` can be used.
- **Jansson**: JSON parsing library (automatically resolved by CMake)

## Building

The configurator is built as part of the Celix project when the `CONFIGURATOR` option is enabled (default: ON):

```bash
cmake -DCONFIGURATOR=ON ...
```

## Testing

Unit tests are included and can be run with:

```bash
ctest --test-dir build/bundles/configurator/configurator/gtest
```

## OSGi Compatibility

This implementation follows the OSGi Configurator specification from OSGi Compendium Release 8. Key aspects:

- JSON-based configuration format
- Support for singleton and factory configurations
- Integration with Configuration Admin service
- Property type handling

## License

Licensed under the Apache License, Version 2.0. See LICENSE file for details.
