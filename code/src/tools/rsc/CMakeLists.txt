set(MODULE_NAME rsc)
add_library(${MODULE_NAME} MODULE
  RSC.cpp
  )
set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")
install(TARGETS ${MODULE_NAME} DESTINATION .)
