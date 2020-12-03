# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof codec_adapter.c codec/generic.c)

if(CONFIG_CADENCE_CODEC)
add_local_sources(sof codec/cadence.c)
endif()

if(CONFIG_WAVES_CODEC)
set(DUMMY_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/codec/Dummy/Dummy.c
    ${CMAKE_CURRENT_LIST_DIR}/codec/Dummy/Process/Dummy_Process.c
    ${CMAKE_CURRENT_LIST_DIR}/codec/Dummy/Process/Dummy_Reset.c
    ${CMAKE_CURRENT_LIST_DIR}/codec/Dummy/Initialize/Dummy_Effect_Initialize.c
    ${CMAKE_CURRENT_LIST_DIR}/codec/Dummy/Initialize/Dummy_Effect_InitializeFromScenario.c
    ${CMAKE_CURRENT_LIST_DIR}/codec/Dummy/Control/Direct/Dummy_Coefficient.c
    ${CMAKE_CURRENT_LIST_DIR}/codec/Dummy/Control/Direct/Dummy_Deferred.c
    ${CMAKE_CURRENT_LIST_DIR}/codec/Dummy/Control/Direct/Dummy_Meters.c
    ${CMAKE_CURRENT_LIST_DIR}/codec/Dummy/Control/Direct/Dummy_Parameter.c
    ${CMAKE_CURRENT_LIST_DIR}/codec/Dummy/Control/Direct/Dummy_Revision.c
    ${CMAKE_CURRENT_LIST_DIR}/codec/Dummy/Control/RPC/Dummy_RPC_Server.c)

add_local_sources(sof codec/waves.c ${DUMMY_SOURCES})
# Waves interface needs relative path starting from parent directory of MaxxEffect folder
# Adding folder with Waves interface to includes
# to be fixed
target_include_directories(sof PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include/sof/audio/waves)
endif()