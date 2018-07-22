set(MODULE_NAME rsc)
add_library(${MODULE_NAME} MODULE
  MaySleeping.cpp
  RSC.cpp
  )
set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")
target_link_libraries(${MODULE_NAME} librsc)
install(TARGETS ${MODULE_NAME} DESTINATION .)
