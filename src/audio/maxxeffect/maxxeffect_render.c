// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Waves Audio Ltd. All rights reserved.
//
// Author: Oleksandr Strelchenko <oleksandr.strelchenko@waves.com>
//

#include <sof/audio/maxxeffect/maxxeffect_render.h>
#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/lib/uuid.h>
#include <sof/lib/memory.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/stream.h>

static const struct comp_driver me_comp;

/* e72c7109-f69c-4bf3-b971-574156455300 */
DECLARE_SOF_RT_UUID("maxxeffect_reder", me_uuid, 0xe72c7109, 0xf69c, 0x4bf3,
					0xb9, 0x71, 0x57, 0x41, 0x56, 0x45, 0x53, 0x00);

DECLARE_TR_CTX(me_trace, SOF_UUID(me_uuid), LOG_LEVEL_INFO);

static struct comp_dev *me_new(
	const struct comp_driver *drv,
	struct sof_ipc_comp *comp)
{
	int ret;
	struct sof_ipc_comp_process *me_ipc = (struct sof_ipc_comp_process *)comp;
	struct comp_dev *dev = NULL;
	struct me_data *cd = NULL;

	comp_cl_info(&me_comp, "me_new(), enter");

	// sanity check
	if (!drv || !comp) {
		comp_cl_err(&me_comp, "me_new(), NULL pointer drv = %x comp = %x",
			(uint32_t)drv, (uint32_t)comp);
		goto err;
	}

	// allocate component common
	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev) {
		comp_cl_err(&me_comp, "me_new(), failed to allocate memory for dev");
		goto err;
	}
	dev->drv = drv;

	// copy assosiated ipc
	ret = memcpy_s(&dev->comp, sizeof(struct sof_ipc_comp_process),
		comp, sizeof(struct sof_ipc_comp_process));
	assert(!ret);

	// allocate component specific
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		comp_cl_err(&me_comp, "me_new(), failed to allocate memory for wr_data");
		goto err;
	}


	// bind allocated component memory to device
	comp_set_drvdata(dev, cd);

	// set default config
	// @TODO: into separate procedure
	{
		cd->config.codec_id = 0;
		cd->config.reserved = 0;
		cd->config.sample_rate = 48000;
		cd->config.sample_width = 32;
		cd->config.channels = 2;
	}

	// load incoming config from ipc
	// @TODO: into separate procedure
	if (me_ipc->size) {
		comp_cl_info(&me_comp, "me_new() size of ipc config %d", me_ipc->size);

		// copy input config from me_ipc->data, me_ipc->size
		if (me_ipc->size < sizeof(struct me_config)) {
			comp_cl_err(&me_comp, "me_new(), no input configuration");
			goto err;
		}
		ret = memcpy_s(&cd->config, sizeof(cd->config),
			me_ipc->data, sizeof(struct me_config));
		assert(!ret);

		// validate config
		if (cd->config.channels != 2) {
			comp_cl_err(&me_comp, "me_new(), wrong channels number %d",
				cd->config.channels);
			goto err;
		}
		if (cd->config.sample_rate != 48000) {
			comp_cl_err(&me_comp, "me_new(), wrong sample rate %d",
				cd->config.sample_rate);
			goto err;
		}
	}

	// Allocate memory for MaxxEffect
	// @TODO: into separate procedure
	{
		MaxxStatus_t status;

		status = MaxxEffect_GetEffectSize(&cd->effect_size);
		if (status) {
			comp_cl_err(&me_comp, "me_new(), MaxxEffect_GetEffectSize error %d",
				status);
			goto err;
		}
		comp_cl_info(&me_comp, "me_new(), effect_size %d", cd->effect_size);

		cd->effect = (MaxxEffect_t *)rzalloc(SOF_MEM_ZONE_RUNTIME, 0,
			SOF_MEM_CAPS_RAM, cd->effect_size);
		if (!cd->effect) {
			comp_cl_err(&me_comp, "me_new(), failed to allocate memory for effect");
			goto err;
		}
	}

	// Initialization of MaxxEffect
	// @TODO: into separate procedure
	{
		MaxxStatus_t status;
		MaxxStreamFormat_t *i_formats[1] = { &cd->i_format };
		MaxxStreamFormat_t *o_formats[1] = { &cd->o_format };

		cd->i_format.sampleRate = cd->config.sample_rate;
		cd->i_format.numChannels = cd->config.channels;
		cd->i_format.samplesFormat = MAXX_BUFFER_FORMAT_Q1_31;
		cd->i_format.samplesLayout = MAXX_BUFFER_LAYOUT_INTERLEAVED;
		cd->o_format = cd->i_format;

		status = MaxxEffect_Initialize(&cd->effect_size, i_formats, 1, o_formats, 1);

		if (status) {
			comp_cl_err(&me_comp, "me_new(), failed to initialize effect");
			rfree(cd->effect);
			goto err;
		}
	}

	dev->state = COMP_STATE_READY;
	cd->state = ME_INITIALIZED;

	comp_cl_info(&me_comp, "me_new() done");

	return dev;
err:
	rfree(cd);
	rfree(dev);
	return NULL;
}

static int me_prepare(
	struct comp_dev *dev)
{
	// @TODO: maybe move initialization here?
	int ret;
	struct me_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "me_prepare() start");

	/* Init sink & source buffers */
	cd->sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	cd->source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);

	if (!cd->source) {
		comp_err(dev, "me_prepare() erro: source buffer not found");
		return -EINVAL;
	}

	if (!cd->sink) {
		comp_err(dev, "me_prepare() erro: sink buffer not found");
		return -EINVAL;
	}

	if (cd->source->buffer_fmt != SOF_IPC_BUFFER_INTERLEAVED) {
		comp_err(dev, "me_prepare() erro: source not interleaved");
		return -EINVAL;
	}

	if (cd->source->buffer_fmt != SOF_IPC_FRAME_S32_LE) {
		comp_err(dev, "me_prepare() erro: source not SOF_IPC_FRAME_S32_LE");
		return -EINVAL;
	}

	if (cd->sink->buffer_fmt != SOF_IPC_BUFFER_INTERLEAVED) {
		comp_err(dev, "me_prepare() erro: sink not interleaved");
		return -EINVAL;
	}

	if (cd->sink->buffer_fmt != SOF_IPC_FRAME_S32_LE) {
		comp_err(dev, "me_prepare() erro: sink not SOF_IPC_FRAME_S32_LE");
		return -EINVAL;
	}

	/* Are we already prepared? */
	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET) {
		comp_err(dev, "me_prepare() error %x: codec_adapter has already been prepared",
			ret);
		return PPL_STATUS_PATH_STOP;
	}

	comp_info(dev, "me_prepare() done");
	cd->state = ME_PREPARED;

	return 0;
}

static int me_params(
	struct comp_dev *dev,
	struct sof_ipc_stream_params *params)
{
	int ret = 0;

	if (dev->state == COMP_STATE_PREPARE) {
		comp_warn(dev, "me_params(): params has already been prepared.");
		goto end;
	}
	comp_info(dev, "me_params(): direction              %08x", params->direction);
	comp_info(dev, "me_params(): frame_fmt              %08x", params->frame_fmt);
	comp_info(dev, "me_params(): buffer_fmt             %08x", params->buffer_fmt);
	comp_info(dev, "me_params(): rate                   %08x", params->rate);
	comp_info(dev, "me_params(): stream_tag             %08x", params->stream_tag);
	comp_info(dev, "me_params(): channels               %08x", params->channels);
	comp_info(dev, "me_params(): sample_valid_bytes     %08x", params->sample_valid_bytes);
	comp_info(dev, "me_params(): sample_container_bytes %08x", params->sample_container_bytes);
	comp_info(dev, "me_params(): host_period_bytes      %08x", params->host_period_bytes);
	comp_info(dev, "me_params(): no_stream_position     %08x", params->no_stream_position);

	ret = 0;
	if (ret < 0) {
		comp_err(dev, "me_params(): pcm params verification failed");
		goto end;
	}

end:
	return ret;
}

static int me_copy(
	struct comp_dev *dev)
{
	int ret = 0;
	struct me_data *cd = comp_get_drvdata(dev);

	struct comp_buffer *source = cd->source;
	struct comp_buffer *sink = cd->sink;
	struct comp_copy_limits limits;

	comp_get_copy_limits_with_lock(source, sink, &limits);

	comp_info(dev, "me_copy(): frames                  %08x", limits.frames);
	comp_info(dev, "me_copy(): source_bytes            %08x", limits.source_bytes);
	comp_info(dev, "me_copy(): sink_bytes              %08x", limits.sink_bytes);
	comp_info(dev, "me_copy(): source_frame_bytes      %08x", limits.source_frame_bytes);
	comp_info(dev, "me_copy(): sink_frame_bytes        %08x", limits.sink_frame_bytes);

	buffer_invalidate(source, limits.source_bytes);
	{
		// @TODO: maybe not use direct poiters but copy to temp buffers?
		//        this will help to handle processing errors
		MaxxBuffer_t i_buffer = source->stream.r_ptr;
		MaxxBuffer_t o_buffer = sink->stream.w_ptr;
		MaxxStream_t *i_streams[1] = { &cd->i_stream };
		MaxxStream_t *o_streams[1] = { &cd->o_stream };
		MaxxStatus_t status;

		cd->i_stream.buffersArray = &i_buffer;
		cd->i_stream.numAvailableSamples = limits.frames;
		cd->i_stream.numProcessedSamples = 0;
		cd->i_stream.maxNumSamples = limits.frames;

		cd->o_stream.buffersArray = &o_buffer;
		cd->o_stream.numAvailableSamples = 0;
		cd->o_stream.numProcessedSamples = 0;
		cd->o_stream.maxNumSamples = limits.frames;

		status = MaxxEffect_Process(cd->effect, i_streams, o_streams);
		if (status) {
			ret = status;
			comp_err(dev, "me_copy(): MaxxEffect_Process error %d", status);
			goto end;
		}

		comp_update_buffer_produce(sink, cd->o_stream.numAvailableSamples * 4);
		comp_update_buffer_consume(source, cd->i_stream.numProcessedSamples * 4);
	}

end:
	return ret;
}

static void me_free(struct comp_dev *dev)
{
	struct me_data *cd = comp_get_drvdata(dev);

	comp_cl_info(&me_comp, "me_free(), start");

	rfree(cd->effect);
	rfree(cd);
	rfree(dev);

	comp_cl_info(&me_comp, "me_free(), end");
}

static int me_trigger(struct comp_dev *dev, int cmd)
{
	comp_cl_info(&me_comp, "me_trigger(): got trigger cmd %x", cmd);

	return comp_set_state(dev, cmd);
}

static int me_reset(struct comp_dev *dev)
{
	struct me_data *cd = comp_get_drvdata(dev);

	comp_cl_info(&me_comp, "me_reset(), start");

	{
		MaxxStatus_t status;

		status = MaxxEffect_Reset(cd->effect);
		if (status)
			comp_cl_info(&me_comp, "me_reset(): MaxxEffect_Reset error %d", status);
	}

	comp_cl_info(&me_comp, "me_reset(), triggering reset");

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int me_set_data(struct comp_dev *dev, struct sof_ipc_ctrl_data *data)
{
	// wr_data_t *cd = comp_get_drvdata(dev);

	comp_cl_info(&me_comp, "me_set_data(), start");

	if (SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_VERSION, data->data->abi)) {
		comp_err(dev, "me_set_data(): ABI mismatch");
		return -EINVAL;
	}

	comp_cl_info(&me_comp, "me_set_data(), end");
	return 0;
}

static int me_cmd(struct comp_dev *dev, int cmd, void *data, int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	comp_cl_info(&me_comp, "me_cmd(), start");

	switch (cmd) {

	case COMP_CMD_SET_DATA:
		comp_cl_info(&me_comp, "me_cmd() COMP_CMD_SET_DATA");
		ret = me_set_data(dev, cdata);
		break;

	case COMP_CMD_GET_DATA:
		comp_cl_info(&me_comp, "me_cmd() COMP_CMD_GET_DATA not implemented");
		ret = 0;
		break;

	default:
		comp_cl_err(&me_comp, "me_cmd() cmd %d not supported", cmd);
		ret = -EINVAL;
		break;
	}

	comp_cl_info(&me_comp, "me_cmd(), end %d", ret);
	return ret;
}


static const struct comp_driver me_comp = {
	.type   = SOF_COMP_NONE,
	.uid    = SOF_RT_UUID(me_uuid),
	.tctx   = &me_trace,
	.ops    = {
		.create     = me_new,
		.prepare    = me_prepare,
		.params     = me_params,
		.copy       = me_copy,
		.free       = me_free,
		.trigger    = me_trigger,
		.reset      = me_reset,
		.cmd        = me_cmd,
	},
};

static SHARED_DATA struct comp_driver_info me_comp_info = {
	.drv = &me_comp,
};

UT_STATIC void me_sys_comp_init(void)
{
	comp_register(platform_shared_get(
		&me_comp_info,
		sizeof(me_comp_info)));
}

DECLARE_MODULE(me_sys_comp_init);
