add_library(stream_support STATIC
  audio_recorder.cc
  inference_stats.cc
  sliding_feature_window.cc
  wakeup_notifier.cc
  wakeup_postprocessor.cc
)
target_include_directories(stream_support PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(stream_support PUBLIC pthread m)

if(APPLE)
  target_link_libraries(stream_support PUBLIC portaudio_static)
endif()
