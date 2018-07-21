set(MODULE_NAME librsc)
add_library(${MODULE_NAME} STATIC
  Formula.cpp
  FormulaVisitor.cpp
  PathIterator.cpp
  PredefinedSummary.cpp
  Serialization.cpp
  Summary.cpp
  util.cpp
  )
set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")
target_link_libraries(${MODULE_NAME} boost_regex z3)
