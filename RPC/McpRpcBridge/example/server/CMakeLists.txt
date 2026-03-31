# Example Server - 演示如何暴露 RPC 服务为 MCP 工具

set(KRPC_EXAMPLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../Krpc/example)

add_executable(mcp_example_server
    main.cc
    ${KRPC_EXAMPLE_DIR}/user.pb.cc
)

target_include_directories(mcp_example_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${KRPC_EXAMPLE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../Krpc/src/include
        ${Protobuf_INCLUDE_DIRS}
)

target_link_libraries(mcp_example_server
    PRIVATE
        mcp_rpc_bridge
        krpc_core
        ${Protobuf_LIBRARIES}
        pthread
        zookeeper_mt
        muduo_net
        muduo_base
        glog
)
