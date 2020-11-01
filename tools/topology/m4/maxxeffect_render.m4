divert(-1)

dnl Define macro for MaxxEffect widget
DECLARE_SOF_RT_UUID("maxxeffect_render", me_uuid, 0xe72c7109, 0xf69c, 0x4bf3,
					0xb9, 0x71, 0x57, 0x41, 0x56, 0x45, 0x53, 0x00);

dnl N_MAXXEFFECT(name)
define(`N_MAXXEFFECT', `MAXXEFFECT'PIPELINE_ID`.'$1)

dnl W_MAXXEFFECT(name, format, periods_sink, periods_source, core, kcontrol0. kcontrol1...etc)
define(`W_MAXXEFFECT',
`SectionVendorTuples."'N_MAXXEFFECT($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(me_uuid)
`	}'
`}'
`SectionData."'N_MAXXEFFECT($1)`_data_uuid" {'
`	tuples "'N_MAXXEFFECT($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_MAXXEFFECT($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'				STR($5)
`	}'
`}'
`SectionData."'N_MAXXEFFECT($1)`_data_w" {'
`	tuples "'N_MAXXEFFECT($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_MAXXEFFECT($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_MAXXEFFECT($1)`_data_str" {'
`	tuples "'N_MAXXEFFECT($1)`_tuples_str"'
`}'
`SectionWidget."'N_MAXXEFFECT($1)`") {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_CODEC_ADAPTER($1)`_data_uuid"'
`		"'N_CODEC_ADAPTER($1)`_data_w"'
`		"'N_CODEC_ADAPTER($1)`_data_str"'
`	]'
`	bytes ['
		$6
`	]'

`}')

divert(0)dnl
