add_definitions( -DSQLITE_HAS_CODEC )

add_test_snippet(wallet_test wallet)
add_test_snippet(wallet_db_test wallet)

add_executable(offline offline.cpp)
add_dependencies(offline node wallet)
target_link_libraries(offline node wallet)

add_executable(wallet_api_client wallet_api_client.cpp)
add_dependencies(wallet_api_client wallet utility)
target_link_libraries(wallet_api_client wallet utility)
