project(lib_linARfilterPred_project)

include_directories ("${PROJECT_SOURCE_DIR}/src")
include_directories ("${PROJECT_SOURCE_DIR}/..")
add_library(linARfilterPred SHARED linARfilterPred.c)

install(TARGETS linARfilterPred DESTINATION lib)
install(FILES linARfilterPred.h DESTINATION include)

