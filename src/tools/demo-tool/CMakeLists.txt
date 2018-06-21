set(MODULE_NAME RSC-demo)

add_library(${MODULE_NAME} MODULE
	demo.cpp
)

install(TARGETS ${MODULE_NAME} DESTINATION .)