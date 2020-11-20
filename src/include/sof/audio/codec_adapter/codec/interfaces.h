/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 *
 * \file interfaces.h
 * \brief Description of supported codecs
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 */
#ifndef __SOF_AUDIO_CODEC_INTERFACES__
#define __SOF_AUDIO_CODEC_INTERFACES__

#include <sof/audio/codec_adapter/codec/generic.h>
#if CONFIG_CADENCE_CODEC
#include <sof/audio/codec_adapter/codec/cadence.h>
#endif
#if CONFIG_WAVES_CODEC
#include <sof/audio/codec_adapter/codec/waves.h>
#endif

/*****************************************************************************/
/* Linked codecs interfaces								*/
/*****************************************************************************/
//TODO: move it to a specific section
static struct codec_interface interfaces[] = {
#if CONFIG_CADENCE_CODEC
	{
		.id = 0xCADE01, /**< Cadence interface */
		.init  = cadence_codec_init,
		.prepare = cadence_codec_prepare,
		.process = cadence_codec_process,
		.apply_config = cadence_codec_apply_config,
		.reset = cadence_codec_reset,
		.free = cadence_codec_free
	},
#endif
#if CONFIG_WAVES_CODEC
	{
		.id = 0x57410001,
		.init  = waves_codec_init,
		.prepare = waves_codec_prepare,
		.process = waves_codec_process,
		.apply_config = waves_codec_apply_config,
		.reset = waves_codec_reset,
		.free = waves_codec_free
	},
#endif
};

#endif /* __SOF_AUDIO_CODEC_INTERFACES__ */
