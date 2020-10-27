# SOF coponent
add_local_sources(sof waves_render.c)

# MaxxEffect API
target_include_directories(sof PRIVATE ./)

# MaxxEffect API implementation
# @TODO: link as a static library
target_include_directories(sof PRIVATE ./Dummy)
add_local_sources(sof
    ./Dummy/Dummy.c
    ./Dummy/Process/Dummy_Process.c
    ./Dummy/Process/Dummy_Reset.c
    ./Dummy/Initialize/Dummy_Effect_Initialize.c
    ./Dummy/Initialize/Dummy_Effect_InitializeFromScenario.c
    ./Dummy/Control/Direct/Dummy_Coefficient.c
    ./Dummy/Control/Direct/Dummy_Deferred.c
    ./Dummy/Control/Direct/Dummy_Meters.c
    ./Dummy/Control/Direct/Dummy_Parameter.c
    ./Dummy/Control/Direct/Dummy_Revision.c
    ./Dummy/Control/RPC/Dummy_RPC_Server.c)
