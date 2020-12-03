/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Waves Audio Ltd. All rights reserved.
 *
 * Author: Oleksandr Strelchenko <oleksandr.strelchenko@waves.com>
 */
#ifndef __SOF_AUDIO_WAVES_CODEC__
#define __SOF_AUDIO_WAVES_CODEC__

#include "MaxxEffect/MaxxEffect.h"
#include "MaxxEffect/MaxxStream.h"
#include "MaxxEffect/MaxxStatus.h"
#include "MaxxEffect/Initialize/MaxxEffect_Initialize.h"
#include "MaxxEffect/Process/MaxxEffect_Process.h"
#include "MaxxEffect/Process/MaxxEffect_Reset.h"
#include "MaxxEffect/Control/RPC/MaxxEffect_RPC_Server.h"
#include "MaxxEffect/Control/Direct/MaxxEffect_Revision.h"

struct waves_codec_data {
	uint32_t                sample_rate; // [Hz]
	uint32_t                buffer_bytes; // bytes
	uint32_t                buffer_samples; // multichannel samples
	uint32_t                sample_size_in_bytes;
	uint64_t                reserved;

	MaxxEffect_t            *effect;
	uint32_t                effect_size;
	MaxxStreamFormat_t      i_format;
	MaxxStreamFormat_t      o_format;
	MaxxStream_t            i_stream;
	MaxxStream_t            o_stream;
	MaxxBuffer_t            i_buffer;
	MaxxBuffer_t            o_buffer;
	uint32_t                response_max_bytes;
	uint32_t                request_max_bytes;
	void                    *response;
};

int waves_codec_init(struct comp_dev *dev);
int waves_codec_prepare(struct comp_dev *dev);
int waves_codec_process(struct comp_dev *dev);
int waves_codec_apply_config(struct comp_dev *dev);
int waves_codec_reset(struct comp_dev *dev);
int waves_codec_free(struct comp_dev *dev);

#endif /* __SOF_AUDIO_WAVES_CODEC__ */
