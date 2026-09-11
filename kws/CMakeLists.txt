add_library(kws STATIC keyword_spotting.cc)
target_include_directories(kws PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(kws PUBLIC tflite_micro)
