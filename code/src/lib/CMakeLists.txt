set(MODULE_NAME librsc)
add_library(${MODULE_NAME} STATIC
  util.cpp
  )
set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")
target_link_libraries(${MODULE_NAME} boost_regex z3)
