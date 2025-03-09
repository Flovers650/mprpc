aux_source_directory(. SRC_LIST)
aux_source_directory(./muduo MUDUO_LIST)
add_library(mprpc ${SRC_LIST} ${MUDUO_LIST})
target_link_libraries(mprpc mymuduo pthread zookeeper_mt)