# restinio-crud-example

restinio-crud-example is a simple example of CRUD application based on
[RESTinio](https://github.com/stiffstream/restinio) library.

# How to obtain and build

## Prerequisites

restinio-crud-example requires C++ compiler with C++14 support, vcpkg and CMake.

## Obtaining

Just clone restinio-crud-example from GitHub:

```sh
git clone https://github.com/stiffstream/restinio-crud-example
cd restinio-crud-example
```

## Building

### Obtaining the dependencies

restinio-crud-example uses the following libraries directly: RESTinio, json_dto, SQLiteCpp and optional-lite. Those libraries also use RapidJson, fmtlib and sqlite3. All those dependencies should be obtained via vcpkg:

```sh
vcpkg install restinio json-dto sqlitecpp optional-lite
```

### Building

The build of restinio-crud-example is performed via CMake. Please note the usage of vcpkg's toolchain file:

```sh
cd restinio-crud-example
mkdir cmake_build
cd cmake_build
cmake -DCMAKE_TOOLCHAIN_FILE=<path-to-your-vcpkg>/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release .
cmake --build .
```

# Running

Just launch `crud_example` executable. The DB file (`pets.db3`) will be created in the current path.

