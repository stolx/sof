# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof codec_adapter.c codec/generic.c)

if (CONFIG_CADENCE_CODEC)
    add_local_sources(sof codec/cadence.c)
endif()

if (CONFIG_WAVES_CODEC)
    # MaxxEffect API
    target_include_directories(sof PRIVATE ./waves/)

    # MaxxEffect API implementation
    target_include_directories(sof PRIVATE ./waves/Dummy)

    add_local_sources(sof
        waves/waves.c
        waves/Dummy/Dummy.c
        waves/Dummy/Process/Dummy_Process.c
        waves/Dummy/Process/Dummy_Reset.c
        waves/Dummy/Initialize/Dummy_Effect_Initialize.c
        waves/Dummy/Initialize/Dummy_Effect_InitializeFromScenario.c
        waves/Dummy/Control/Direct/Dummy_Coefficient.c
        waves/Dummy/Control/Direct/Dummy_Deferred.c
        waves/Dummy/Control/Direct/Dummy_Meters.c
        waves/Dummy/Control/Direct/Dummy_Parameter.c
        waves/Dummy/Control/Direct/Dummy_Revision.c
        waves/Dummy/Control/RPC/Dummy_RPC_Server.c)
endif()