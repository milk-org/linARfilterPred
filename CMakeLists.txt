project(lib_linARfilterPred_project)

include_directories ("${PROJECT_SOURCE_DIR}/src")
include_directories ("${PROJECT_SOURCE_DIR}/..")
add_library(linARfilterPred SHARED linARfilterPred.c)




if(USE_MAGMA)
target_include_directories(linARfilterPred PUBLIC ${MAGMA_INCLUDE_DIRS})
set_target_properties(linARfilterPred PROPERTIES COMPILE_FLAGS "-DHAVE_CUDA -DHAVE_MAGMA")
endif(USE_MAGMA)

install(TARGETS linARfilterPred DESTINATION lib)
install(FILES linARfilterPred.h DESTINATION include)

