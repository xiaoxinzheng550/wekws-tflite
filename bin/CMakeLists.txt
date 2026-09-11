# 模型数据库
add_library(model_data STATIC
  ${CMAKE_SOURCE_DIR}/model/model_data.cc
)
target_include_directories(model_data PUBLIC ${CMAKE_SOURCE_DIR})

# 离线测试程序
add_executable(kws_main kws_main.cc)
target_link_libraries(kws_main PUBLIC
    kws
    frontend
    model_data
    tflite_micro
    pthread
    m
)

# 实时检测程序（可选）
if(WEKWS_BUILD_STREAM)
  add_executable(stream_kws_main stream_kws_main.cc)
  target_link_libraries(stream_kws_main PUBLIC
      kws
      frontend
      model_data
      tflite_micro
      pthread
      m
  )
  if(APPLE)
    target_link_libraries(stream_kws_main PUBLIC portaudio_static)
  endif()
endif()
