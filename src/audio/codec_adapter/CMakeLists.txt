# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof codec_adapter.c codec/generic.c)

if(CONFIG_CADENCE_CODEC)
add_local_sources(sof codec/cadence.c)
endif()

if(CONFIG_DUMMY_CODEC)
add_local_sources(sof codec/dummy.c)
endif()

if(CONFIG_WAVES_CODEC)
add_local_sources(sof codec/waves.c)

if(WAVES_CODEC_DUMMY)
# enabling WAVES_CODEC_DUMMY will build mock MaxxEffect implementation
# that has no audio processing algorithms and is used for debug purposes
set(DUMMY_SOURCES
	${CMAKE_CURRENT_LIST_DIR}/Dummy/Dummy.c
	${CMAKE_CURRENT_LIST_DIR}/Dummy/Process/Dummy_Process.c
	${CMAKE_CURRENT_LIST_DIR}/Dummy/Process/Dummy_Reset.c
	${CMAKE_CURRENT_LIST_DIR}/Dummy/Initialize/Dummy_Effect_Initialize.c
	${CMAKE_CURRENT_LIST_DIR}/Dummy/Initialize/Dummy_Effect_InitializeFromScenario.c
	${CMAKE_CURRENT_LIST_DIR}/Dummy/Control/Direct/Dummy_Coefficient.c
	${CMAKE_CURRENT_LIST_DIR}/Dummy/Control/Direct/Dummy_Deferred.c
	${CMAKE_CURRENT_LIST_DIR}/Dummy/Control/Direct/Dummy_Meters.c
	${CMAKE_CURRENT_LIST_DIR}/Dummy/Control/Direct/Dummy_Parameter.c
	${CMAKE_CURRENT_LIST_DIR}/Dummy/Control/Direct/Dummy_Revision.c
	${CMAKE_CURRENT_LIST_DIR}/Dummy/Control/RPC/Dummy_RPC_Server.c)
add_local_sources(sof ${DUMMY_SOURCES})
else()
sof_add_static_library(MaxxChrome ${CMAKE_CURRENT_LIST_DIR}/lib/release/libMaxxChrome.a)
endif()

endif()
