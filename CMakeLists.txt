project(lib_linARfilterPred_project)

include_directories ("${PROJECT_SOURCE_DIR}/src")
include_directories ("${PROJECT_SOURCE_DIR}/..")
add_library(linARfilterPred SHARED linARfilterPred.c)


if(USE_CUDA)
find_package(CUDA 9.0 REQUIRED)
message(STATUS "Found CUDA ${CUDA_VERSION_STRING} at ${CUDA_TOOLKIT_ROOT_DIR}")
message("---- CUDA_INCLUDE_DIRS = ${CUDA_INCLUDE_DIRS}")
message("---- CUDA_LIBRARIES    = ${CUDA_LIBRARIES}" )
target_include_directories(linARfilterPred PUBLIC ${CUDA_INCLUDE_DIRS})
target_link_libraries(linARfilterPred PRIVATE ${CUDA_LIBRARIES})
target_compile_options(linARfilterPred PUBLIC -DHAVE_CUDA)
endif(USE_CUDA)


if(USE_MAGMA)
target_include_directories(linARfilterPred PUBLIC ${MAGMA_INCLUDE_DIRS})
set_target_properties(linARfilterPred PROPERTIES COMPILE_FLAGS "-DHAVE_CUDA -DHAVE_MAGMA")
endif(USE_MAGMA)

install(TARGETS linARfilterPred DESTINATION lib)
install(FILES linARfilterPred.h DESTINATION include)

