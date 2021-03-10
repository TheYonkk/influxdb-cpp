# influxdb-cpp-2.0

A header-only C++ query & write client for InfluxDB 2.0.

[![license](https://img.shields.io/badge/license-MIT-brightgreen.svg?style=flat)](https://github.com/orca-zhang/influxdb-cpp/blob/master/LICENSE)  [![Build Status](https://semaphoreci.com/api/v1/orca-zhang-91/influxdb-cpp/branches/master/shields_badge.svg)](https://semaphoreci.com/orca-zhang-91/influxdb-cpp)  [![Build status](https://ci.appveyor.com/api/projects/status/gusrrn0mn67q2yaj?svg=true)](https://ci.appveyor.com/project/orca-zhang/influxdb-cpp)

- Support versions:
  - InfluxDB v2.0
  - Incompatible with versions 1.x
  - Check yourself while using future versions.

### Why use influxdb-cpp?

- **Exactly-small**:
  - Less than 300 lines and only 10KB+.
- **Easy-to-use**:
  - It's designed to be used without extra studies.
- **Easy-to-assemble**:
  - Only a tiny header file needs to be included.
- **No-dependencies**:
  - Unless STL and std C libraries.

### Examples

#### Before using

- The very simple thing you should do before using is only:

    ```cpp
    #include "influxdb.hpp"
    ```

#### Writing example

- You should refer to the [write syntax](https://docs.influxdata.com/influxdb/v1.4/write_protocols/line_protocol_reference/) while writing series(metrics).

    ```
    measurement[,tag-key=tag-value...] field-key=field-value[,field2-key=field2-value...] [unix-nano-timestamp]
    ```


- You can rapidly start writing series according to the following example.

    ```cpp
    influxdb_cpp::server_info si("127.0.0.1", 8086, "org", "token", "bucket");
    influxdb_cpp::builder()
        .meas("foo")
        .tag("k", "v")
        .tag("x", "y")
        .field("x", 10)
        .field("y", 10.3, 4)
        .field("z", 10.3456)
        .field("b", !!10)
        .timestamp(1512722735522840439)
        .post_http(si);
    ```

  - **Remarks**: 
    - 3rd parameter `precision` of `field()` is optional for floating point value, and default precision is defined in the top of the header. 
    - The token must have access to write (and/or read) from the specified organization and bucket.
    - Although `bucket` is an optional parameter for a server info object, it is **required** for database writes.


- The series sent is:

    ```
    foo,k=v,x=y x=10i,y=10.30,z=10.346,b=t 1512722735522840439
    ```


- Bulk/batch/multiple insert also supports:

    ```cpp
    influxdb_cpp::builder()
        .meas("foo")  // series 1
        .field("x", 10)

        .meas("bar")  // series 2
        .field("y", 10.3)
        .post_http(si);
    ```

- The series sent are:

    ```
    foo x=10i
    bar y=10.30
    ```

#### Query example

- And you can query series using the Flux query language.

    ```cpp
    influxdb_cpp::server_info si("localhost", 8086, "spartans", "my_token", "");

    string resp;
    string query("from(bucket: \\\"SR-20\\\")|> range(start: -5m)|>filter(fn: (r)=>r[\\\"_measurement\\\"] == \\\"cpu\\\")|>filter(fn: (r) => r[\\\"_field\\\"] == \\\"usage_user\\\")");

    influxdb_cpp::query(resp, query, si);
    ```
- _**NOTE:**_ The query is sent in a JSON key-pair, therefore there must be no newlines/tabs, and `"` must be double-escaped as `\\\"`!

- The results are returned in CSV format. A column, `table`, separates different measurements

### **Remarks for Windows**

- You should **init socket environment by yourself** under Windows.
  - FYR: [MSDN doc for `WSAStartup`](https://msdn.microsoft.com/en-us/library/windows/desktop/ms742213(v=vs.85).aspx)

### TODO

- Add more test cases.
- Supports DSN initializatin for server_info.
- Do not need to connect every time.

### Misc

- Please feel free to use influxdb-cpp.
- Looking forward to your suggestions.
- If your project is using influxdb-cpp, you can show your project or company here by creating a issue or let me know.
