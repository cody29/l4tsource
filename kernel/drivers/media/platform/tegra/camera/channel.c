/*
 * NVIDIA Tegra Video Input Device
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Bryan Wu <pengw@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/atomic.h>
#include <linux/bitmap.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/nvhost.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/lcm.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <media/camera_common.h>

#include <mach/clk.h>
#include <mach/io_dpd.h>

#include "mc_common.h"
#include "vi.h"

void tegra_channel_write(struct tegra_channel *chan,
			unsigned int addr, u32 val)
{
	writel(val, chan->vi->iomem + addr);
}

/* CSI registers */
static void csi_write(struct tegra_channel *chan, unsigned int addr, u32 val)
{
	writel(val, chan->csibase + addr);
}

static u32 csi_read(struct tegra_channel *chan, unsigned int addr)
{
	return readl(chan->csibase + addr);
}

static void tegra_channel_fmts_bitmap_init(struct tegra_channel *chan)
{
	int ret, pixel_format_index = 0, init_code = 0;
	struct v4l2_subdev *subdev = chan->subdev[0];
	struct v4l2_mbus_framefmt mbus_fmt;
	struct v4l2_subdev_mbus_code_enum code = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};

	bitmap_zero(chan->fmts_bitmap, MAX_FORMAT_NUM);

	/*
	 * Initialize all the formats available from
	 * the sub-device and extract the corresponding
	 * index from the pre-defined video formats and initialize
	 * the channel default format with the active code
	 * Index zero as the only sub-device is sensor
	 */
	while (1) {
		ret = v4l2_subdev_call(subdev, pad, enum_mbus_code,
				       NULL, &code);
		if (ret < 0)
			/* no more formats */
			break;

		pixel_format_index = tegra_core_get_idx_by_code(code.code);
		if (pixel_format_index >= 0) {
			bitmap_set(chan->fmts_bitmap, pixel_format_index, 1);
			if (!init_code)
				init_code = code.code;
		}

		code.index++;
	}

	if (!init_code) {
		/* Get the format based on active code of the sub-device */
		ret = v4l2_subdev_call(subdev, video, g_mbus_fmt, &mbus_fmt);
		if (ret)
			return;

		chan->fmtinfo = tegra_core_get_format_by_code(init_code);
		chan->format.pixelformat = chan->fmtinfo->fourcc;
		chan->format.colorspace = mbus_fmt.colorspace;
		chan->format.field = mbus_fmt.field;
		chan->format.width = mbus_fmt.width;
		chan->format.height = mbus_fmt.height;
		chan->format.bytesperline = chan->format.width *
						chan->fmtinfo->bpp;
		chan->format.sizeimage = chan->format.bytesperline *
						chan->format.height;
	}
}

/*
 * -----------------------------------------------------------------------------
 * Tegra channel frame setup and capture operations
 * -----------------------------------------------------------------------------
 */

static int tegra_channel_capture_setup(struct tegra_channel *chan)
{
	u32 height = chan->format.height;
	u32 width = chan->format.width;
	u32 format = chan->fmtinfo->img_fmt;
	u32 data_type = chan->fmtinfo->img_dt;
	u32 word_count = tegra_core_get_word_count(width, chan->fmtinfo);

	csi_write(chan, TEGRA_VI_CSI_ERROR_STATUS, 0xFFFFFFFF);
	csi_write(chan, TEGRA_VI_CSI_IMAGE_DEF,
		  ((chan->vi->pg_mode ? 0 : 1) << BYPASS_PXL_TRANSFORM_OFFSET) |
		  (format << IMAGE_DEF_FORMAT_OFFSET) |
		  IMAGE_DEF_DEST_MEM);
	csi_write(chan, TEGRA_VI_CSI_IMAGE_DT, data_type);
	csi_write(chan, TEGRA_VI_CSI_IMAGE_SIZE_WC, word_count);
	csi_write(chan, TEGRA_VI_CSI_IMAGE_SIZE,
		  (height << IMAGE_SIZE_HEIGHT_OFFSET) | width);
	return 0;
}

static void tegra_channel_capture_error(struct tegra_channel *chan)
{
	u32 val;

	val = csi_read(chan, TEGRA_VI_CSI_ERROR_STATUS);
	dev_err(&chan->video.dev, "TEGRA_VI_CSI_ERROR_STATUS 0x%08x\n", val);
	tegra_csi_status(chan->vi->csi, chan->port);
}

static void tegra_channel_capture_frame(struct tegra_channel *chan,
				       struct tegra_channel_buffer *buf)
{
	int err = 0;
	u32 thresh = 0, val, frame_start;
	int bytes_per_line = chan->format.bytesperline;

	/* Program buffer address by using surface 0 */
	csi_write(chan, TEGRA_VI_CSI_SURFACE0_OFFSET_MSB, 0x0);
	csi_write(chan, TEGRA_VI_CSI_SURFACE0_OFFSET_LSB, buf->addr);
	csi_write(chan, TEGRA_VI_CSI_SURFACE0_STRIDE, bytes_per_line);

	/* Program syncpoint */
	thresh = nvhost_syncpt_incr_max_ext(chan->vi->ndev,
					chan->syncpt, 1);
	frame_start = VI_CSI_PP_FRAME_START(chan->port);
	val = VI_CFG_VI_INCR_SYNCPT_COND(frame_start) | chan->syncpt;
	tegra_channel_write(chan, TEGRA_VI_CFG_VI_INCR_SYNCPT, val);

	csi_write(chan, TEGRA_VI_CSI_SINGLE_SHOT, SINGLE_SHOT_CAPTURE);

	err = nvhost_syncpt_wait_timeout_ext(chan->vi->ndev,
			chan->syncpt, thresh,
			TEGRA_VI_SYNCPT_WAIT_TIMEOUT,
			NULL,
			NULL);
	/* Move buffer to capture done queue */
	spin_lock(&chan->done_lock);
	list_add_tail(&buf->queue, &chan->done);
	spin_unlock(&chan->done_lock);

	/* Wait up kthread for capture done */
	wake_up_interruptible(&chan->done_wait);

	if (err) {
		dev_err(&chan->video.dev, "frame start syncpt timeout!\n");
		tegra_channel_capture_error(chan);
	}
}

static void tegra_channel_capture_done(struct tegra_channel *chan,
				       struct tegra_channel_buffer *buf)
{
	struct vb2_buffer *vb = &buf->buf;
	int err = 0;
	u32 thresh = 0, val, mw_ack_done;

	/* Program syncpoint */
	thresh = nvhost_syncpt_incr_max_ext(chan->vi->ndev,
				chan->syncpt, 1);

	mw_ack_done = VI_CSI_MW_ACK_DONE(chan->port);
	val = VI_CFG_VI_INCR_SYNCPT_COND(mw_ack_done) | chan->syncpt;
	tegra_channel_write(chan, TEGRA_VI_CFG_VI_INCR_SYNCPT, val);

	err = nvhost_syncpt_wait_timeout_ext(chan->vi->ndev,
			chan->syncpt, thresh,
			TEGRA_VI_SYNCPT_WAIT_TIMEOUT,
			NULL,
			NULL);
	if (err)
		dev_err(&chan->video.dev, "MW_ACK_DONE syncpoint time out!\n");

	/* Captured one frame */
	vb->v4l2_buf.sequence = chan->sequence++;
	vb->v4l2_buf.field = V4L2_FIELD_NONE;
	v4l2_get_timestamp(&vb->v4l2_buf.timestamp);
	vb2_set_plane_payload(vb, 0, chan->format.sizeimage);
	vb2_buffer_done(vb, err < 0 ? VB2_BUF_STATE_ERROR : VB2_BUF_STATE_DONE);
}

static int tegra_channel_kthread_capture_start(void *data)
{
	struct tegra_channel *chan = data;
	struct tegra_channel_buffer *buf;

	set_freezable();

	while (1) {
		try_to_freeze();
		wait_event_interruptible(chan->start_wait,
					 !list_empty(&chan->capture) ||
					 kthread_should_stop());
		if (kthread_should_stop())
			break;

		spin_lock(&chan->start_lock);
		if (list_empty(&chan->capture)) {
			spin_unlock(&chan->start_lock);
			continue;
		}

		buf = list_entry(chan->capture.next,
				 struct tegra_channel_buffer, queue);
		list_del_init(&buf->queue);
		spin_unlock(&chan->start_lock);

		tegra_channel_capture_frame(chan, buf);
	}

	return 0;
}

static int tegra_channel_kthread_capture_done(void *data)
{
	struct tegra_channel *chan = data;
	struct tegra_channel_buffer *buf;

	set_freezable();

	while (1) {
		try_to_freeze();
		wait_event_interruptible(chan->done_wait,
					 !list_empty(&chan->done) ||
					 kthread_should_stop());
		if (kthread_should_stop() && list_empty(&chan->done))
			break;

		spin_lock(&chan->done_lock);
		if (list_empty(&chan->done)) {
			spin_unlock(&chan->done_lock);
			continue;
		}

		buf = list_entry(chan->done.next,
				 struct tegra_channel_buffer, queue);
		list_del_init(&buf->queue);
		spin_unlock(&chan->done_lock);

		tegra_channel_capture_done(chan, buf);
	}

	return 0;
}

/*
 * -----------------------------------------------------------------------------
 * videobuf2 queue operations
 * -----------------------------------------------------------------------------
 */
static int
tegra_channel_queue_setup(struct vb2_queue *vq, const struct v4l2_format *fmt,
		     unsigned int *nbuffers, unsigned int *nplanes,
		     unsigned int sizes[], void *alloc_ctxs[])
{
	struct tegra_channel *chan = vb2_get_drv_priv(vq);

	/* Make sure the image size is large enough. */
	if (fmt && fmt->fmt.pix.sizeimage < chan->format.sizeimage)
		return -EINVAL;

	*nplanes = 1;

	sizes[0] = fmt ? fmt->fmt.pix.sizeimage : chan->format.sizeimage;
	alloc_ctxs[0] = chan->alloc_ctx;

	if (!*nbuffers)
		*nbuffers = 2;

	return 0;
}

static int tegra_channel_buffer_prepare(struct vb2_buffer *vb)
{
	struct tegra_channel *chan = vb2_get_drv_priv(vb->vb2_queue);
	struct tegra_channel_buffer *buf = to_tegra_channel_buffer(vb);

	buf->chan = chan;
	vb2_set_plane_payload(vb, 0, chan->format.sizeimage);
	buf->addr = vb2_dma_contig_plane_dma_addr(vb, 0);

	return 0;
}

static void tegra_channel_buffer_queue(struct vb2_buffer *vb)
{
	struct tegra_channel *chan = vb2_get_drv_priv(vb->vb2_queue);
	struct tegra_channel_buffer *buf = to_tegra_channel_buffer(vb);

	/* for bypass mode - do nothing */
	if (chan->bypass)
		return;

	/* Put buffer into the capture queue */
	spin_lock(&chan->start_lock);
	list_add_tail(&buf->queue, &chan->capture);
	spin_unlock(&chan->start_lock);

	/* Wait up kthread for capture */
	wake_up_interruptible(&chan->start_wait);
}

/* Return all queued buffers back to videobuf2 */
static void tegra_channel_queued_buf_done(struct tegra_channel *chan,
					  enum vb2_buffer_state state)
{
	struct tegra_channel_buffer *buf, *nbuf;

	spin_lock(&chan->start_lock);
	list_for_each_entry_safe(buf, nbuf, &chan->capture, queue) {
		vb2_buffer_done(&buf->buf, state);
		list_del(&buf->queue);
	}
	spin_unlock(&chan->start_lock);
}

/*
 * -----------------------------------------------------------------------------
 * subdevice set/unset operations
 * -----------------------------------------------------------------------------
 */
static int tegra_channel_set_stream(struct tegra_channel *chan, bool on)
{
	int num_sd = 0;
	int ret = 0;
	struct v4l2_subdev *subdev = chan->subdev[num_sd];

	while (subdev != NULL) {
		ret = v4l2_subdev_call(subdev, video, s_stream, on);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;

		num_sd++;
		if (num_sd >= chan->num_subdevs)
			break;

		subdev = chan->subdev[num_sd];
	}

	return 0;
}

static int tegra_channel_set_power(struct tegra_channel *chan, bool on)
{
	int num_sd = 0;
	int ret = 0;
	struct v4l2_subdev *subdev = chan->subdev[num_sd];

	while (subdev != NULL) {
		ret = v4l2_subdev_call(subdev, core, s_power, on);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;

		num_sd++;
		if (num_sd >= chan->num_subdevs)
			break;

		subdev = chan->subdev[num_sd];
	}

	return 0;
}


static int tegra_channel_start_streaming(struct vb2_queue *vq, u32 count)
{
	struct tegra_channel *chan = vb2_get_drv_priv(vq);
	struct media_pipeline *pipe = chan->video.entity.pipe;
	int ret = 0;

	ret = media_entity_pipeline_start(&chan->video.entity, pipe);
	if (ret < 0)
		goto error_pipeline_start;

	if (!chan->vi->pg_mode) {
		/* Start the pipeline. */
		ret = tegra_channel_set_stream(chan, true);
		if (ret < 0)
			goto error_set_stream;
	}

	if (chan->bypass)
		return ret;

	tegra_csi_start_streaming(chan->vi->csi, chan->port);
	/* Note: Program VI registers after TPG, sensors and CSI streaming */
	ret = tegra_channel_capture_setup(chan);
	if (ret < 0)
		goto error_capture_setup;

	chan->sequence = 0;
	/* Start kthread to capture data to buffer */
	chan->kthread_capture_start = kthread_run(
					tegra_channel_kthread_capture_start,
					chan, chan->video.name);
	if (IS_ERR(chan->kthread_capture_start)) {
		dev_err(&chan->video.dev,
			"failed to run kthread for capture start\n");
		ret = PTR_ERR(chan->kthread_capture_start);
		goto error_capture_setup;
	}

	chan->kthread_capture_done = kthread_run(
					tegra_channel_kthread_capture_done,
					chan, chan->video.name);
	if (IS_ERR(chan->kthread_capture_done)) {
		dev_err(&chan->video.dev,
			"failed to run kthread for capture done\n");
		ret = PTR_ERR(chan->kthread_capture_done);
		goto error_capture_setup;
	}

	return 0;

error_capture_setup:
	tegra_channel_set_stream(chan, false);
error_set_stream:
	media_entity_pipeline_stop(&chan->video.entity);
error_pipeline_start:
	vq->start_streaming_called = 0;
	tegra_channel_queued_buf_done(chan, VB2_BUF_STATE_QUEUED);

	return ret;
}

static int tegra_channel_stop_streaming(struct vb2_queue *vq)
{
	struct tegra_channel *chan = vb2_get_drv_priv(vq);

	if (!chan->bypass) {
		/* Stop the kthread for capture */
		kthread_stop(chan->kthread_capture_start);
		chan->kthread_capture_start = NULL;
		kthread_stop(chan->kthread_capture_done);
		chan->kthread_capture_done = NULL;
	}


	if (!chan->vi->pg_mode)
		tegra_channel_set_stream(chan, false);

	if (!chan->bypass) {
		tegra_csi_stop_streaming(chan->vi->csi, chan->port);
		tegra_channel_queued_buf_done(chan, VB2_BUF_STATE_ERROR);
	}

	media_entity_pipeline_stop(&chan->video.entity);
	return 0;
}

static const struct vb2_ops tegra_channel_queue_qops = {
	.queue_setup = tegra_channel_queue_setup,
	.buf_prepare = tegra_channel_buffer_prepare,
	.buf_queue = tegra_channel_buffer_queue,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.start_streaming = tegra_channel_start_streaming,
	.stop_streaming = tegra_channel_stop_streaming,
};

/* -----------------------------------------------------------------------------
 * V4L2 ioctls
 */

static int
tegra_channel_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	struct v4l2_fh *vfh = file->private_data;
	struct tegra_channel *chan = to_tegra_channel(vfh->vdev);

	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	strlcpy(cap->driver, "tegra-video", sizeof(cap->driver));
	strlcpy(cap->card, chan->video.name, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s:%u",
		 dev_name(chan->vi->dev), chan->port);

	return 0;
}

static int
tegra_channel_enum_format(struct file *file, void *fh, struct v4l2_fmtdesc *f)
{
	struct v4l2_fh *vfh = file->private_data;
	struct tegra_channel *chan = to_tegra_channel(vfh->vdev);
	unsigned int index = 0, i;
	unsigned long *fmts_bitmap = NULL;

	if (chan->vi->pg_mode)
		fmts_bitmap = chan->vi->tpg_fmts_bitmap;
	else
		fmts_bitmap = chan->fmts_bitmap;

	if (f->index >= bitmap_weight(fmts_bitmap, MAX_FORMAT_NUM))
		return -EINVAL;

	for (i = 0; i < f->index + 1; i++, index++)
		index = find_next_bit(fmts_bitmap, MAX_FORMAT_NUM, index);

	index -= 1;
	f->pixelformat = tegra_core_get_fourcc_by_idx(index);

	return 0;
}

static void tegra_channel_fmt_align(struct v4l2_pix_format *pix,
			unsigned int channel_align, unsigned int bpp)
{
	unsigned int min_width;
	unsigned int max_width;
	unsigned int min_bpl;
	unsigned int max_bpl;
	unsigned int width;
	unsigned int align;
	unsigned int bpl;

	/* The transfer alignment requirements are expressed in bytes. Compute
	 * the minimum and maximum values, clamp the requested width and convert
	 * it back to pixels.
	 */
	align = lcm(channel_align, bpp);
	min_width = roundup(TEGRA_MIN_WIDTH, align);
	max_width = rounddown(TEGRA_MAX_WIDTH, align);
	width = roundup(pix->width * bpp, align);

	pix->width = clamp(width, min_width, max_width) / bpp;
	pix->height = clamp(pix->height, TEGRA_MIN_HEIGHT, TEGRA_MAX_HEIGHT);

	/* Clamp the requested bytes per line value. If the maximum bytes per
	 * line value is zero, the module doesn't support user configurable line
	 * sizes. Override the requested value with the minimum in that case.
	 */
	min_bpl = pix->width * bpp;
	max_bpl = rounddown(TEGRA_MAX_WIDTH, channel_align);
	bpl = roundup(pix->bytesperline, channel_align);

	pix->bytesperline = clamp(bpl, min_bpl, max_bpl);
	pix->sizeimage = pix->bytesperline * pix->height;
}

static int tegra_channel_setup_controls(struct tegra_channel *chan)
{
	int num_sd = 0;
	struct v4l2_subdev *sd = NULL;

	/* Initialize the subdev and controls here at first open */
	sd = chan->subdev[num_sd];
	while ((sd = chan->subdev[num_sd++]) &&
		(num_sd <= chan->num_subdevs)) {
		/* Add control handler for the subdevice */
		v4l2_ctrl_add_handler(&chan->ctrl_handler,
					sd->ctrl_handler, NULL);
		if (chan->ctrl_handler.error)
			dev_err(chan->vi->dev,
				"Failed to add sub-device controls\n");
	}

	/* Add the vi ctrl handler */
	v4l2_ctrl_add_handler(&chan->ctrl_handler,
			&chan->vi->ctrl_handler, NULL);
	if (chan->ctrl_handler.error)
		dev_err(chan->vi->dev,
			"Failed to add vi controls\n");

	/* setup the controls */
	return v4l2_ctrl_handler_setup(&chan->ctrl_handler);
}

int tegra_channel_init_subdevices(struct tegra_channel *chan)
{
	struct media_entity *entity;
	struct media_pad *pad;
	int index = 0;
	int num_sd = 0;

	/* set_stream of CSI */
	entity = &chan->video.entity;
	pad = media_entity_remote_source(&chan->pad);
	if (!pad)
		return -ENODEV;

	entity = pad->entity;
	chan->subdev[num_sd++] = media_entity_to_v4l2_subdev(entity);

	index = pad->index - 1;
	while (index >= 0) {
		pad = &entity->pads[index];
		if (!(pad->flags & MEDIA_PAD_FL_SINK))
			break;

		pad = media_entity_remote_source(pad);
		if (pad == NULL ||
		    media_entity_type(pad->entity) != MEDIA_ENT_T_V4L2_SUBDEV)
			break;

		if (num_sd >= MAX_SUBDEVICES)
			break;

		entity = pad->entity;
		chan->subdev[num_sd++] = media_entity_to_v4l2_subdev(entity);

		index = pad->index - 1;
	}
	chan->num_subdevs = num_sd;

	/* initialize the available formats */
	if (chan->num_subdevs)
		tegra_channel_fmts_bitmap_init(chan);

	return tegra_channel_setup_controls(chan);
}

static int
tegra_channel_get_format(struct file *file, void *fh,
			struct v4l2_format *format)
{
	struct v4l2_fh *vfh = file->private_data;
	struct tegra_channel *chan = to_tegra_channel(vfh->vdev);
	struct v4l2_mbus_framefmt mf;
	struct v4l2_pix_format *pix = &format->fmt.pix;
	int ret = 0;
	int num_sd = 0;

	for (num_sd = 0; num_sd < chan->num_subdevs; num_sd++) {
		struct v4l2_subdev *sd = chan->subdev[num_sd];
		ret = v4l2_subdev_call(sd, video, g_mbus_fmt, &mf);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return ret;

		pix->width = mf.width;
		pix->height = mf.height;
		pix->field = mf.field;
		pix->colorspace = mf.colorspace;
		pix->pixelformat = chan->format.pixelformat;
		pix->bytesperline = mf.width * chan->fmtinfo->bpp;
		pix->sizeimage = pix->bytesperline * mf.height;

		return 0;
	}

	return -ENOIOCTLCMD;
}

static int
__tegra_channel_try_format(struct tegra_channel *chan,
			struct v4l2_pix_format *pix,
			const struct tegra_video_format **fmtinfo)
{
	const struct tegra_video_format *info;
	struct v4l2_mbus_framefmt mf;
	int ret = 0;
	int num_sd = 0;
	struct v4l2_subdev *sd = chan->subdev[num_sd];

	/* Retrieve format information and select the default format if the
	 * requested format isn't supported.
	 */
	info = tegra_core_get_format_by_fourcc(pix->pixelformat);
	if (!info)
		info = tegra_core_get_format_by_code(TEGRA_VF_DEF);

	pix->pixelformat = info->fourcc;
	/* Change this when start adding interlace format support */
	pix->field = V4L2_FIELD_NONE;
	pix->colorspace = chan->format.colorspace;

	tegra_channel_fmt_align(pix, chan->align, info->bpp);

	/* verify with subdevice the format supported */
	mf.width = pix->width;
	mf.height = pix->height;
	mf.field = pix->field;
	mf.colorspace = pix->colorspace;
	mf.code = info->code;

	while (sd != NULL) {
		ret = v4l2_subdev_call(sd, video, try_mbus_fmt, &mf);
		if (IS_ERR_VALUE(ret)) {
			if (ret == -ENOIOCTLCMD) {
				num_sd++;
				if (num_sd < chan->num_subdevs) {
					sd = chan->subdev[num_sd];
					continue;
				} else
					break;
			} else
				return ret;
		}

		pix->width = mf.width;
		pix->height = mf.height;
		pix->colorspace = mf.colorspace;

		tegra_channel_fmt_align(pix, chan->align, info->bpp);
		if (fmtinfo) {
			*fmtinfo = info;
			ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &mf);
		}

		return ret;
	}

	return -ENODEV;
}

static int
tegra_channel_try_format(struct file *file, void *fh,
			struct v4l2_format *format)
{
	struct v4l2_fh *vfh = file->private_data;
	struct tegra_channel *chan = to_tegra_channel(vfh->vdev);
	int ret = 0;

	ret = __tegra_channel_try_format(chan, &format->fmt.pix, NULL);

	return ret;
}

static int
tegra_channel_set_format(struct file *file, void *fh,
			struct v4l2_format *format)
{
	struct v4l2_fh *vfh = file->private_data;
	struct tegra_channel *chan = to_tegra_channel(vfh->vdev);
	const struct tegra_video_format *info = NULL;
	int ret = 0;

	if (vb2_is_busy(&chan->queue))
		return -EBUSY;

	ret = __tegra_channel_try_format(chan, &format->fmt.pix, &info);

	chan->format = format->fmt.pix;
	chan->fmtinfo = info;

	return ret;
}

static const struct v4l2_ioctl_ops tegra_channel_ioctl_ops = {
	.vidioc_querycap		= tegra_channel_querycap,
	.vidioc_enum_fmt_vid_cap	= tegra_channel_enum_format,
	.vidioc_g_fmt_vid_cap		= tegra_channel_get_format,
	.vidioc_s_fmt_vid_cap		= tegra_channel_set_format,
	.vidioc_try_fmt_vid_cap		= tegra_channel_try_format,
	.vidioc_reqbufs			= vb2_ioctl_reqbufs,
	.vidioc_querybuf		= vb2_ioctl_querybuf,
	.vidioc_qbuf			= vb2_ioctl_qbuf,
	.vidioc_dqbuf			= vb2_ioctl_dqbuf,
	.vidioc_create_bufs		= vb2_ioctl_create_bufs,
	.vidioc_expbuf			= vb2_ioctl_expbuf,
	.vidioc_streamon		= vb2_ioctl_streamon,
	.vidioc_streamoff		= vb2_ioctl_streamoff,
};

static int tegra_channel_open(struct file *fp)
{
	int ret = 0;
	struct video_device *vdev = video_devdata(fp);
	struct tegra_channel *chan = video_get_drvdata(vdev);

	if (chan->subdev[0] == NULL)
		return -ENODEV;

	if (!chan->bypass) {
		tegra_vi_power_on(chan);
		tegra_csi_power(chan->vi->csi, chan->port, 1);
	}

	if (!chan->vi->pg_mode) {
		/* power on sensors connected in channel */
		ret = tegra_channel_set_power(chan, 1);
		if (ret < 0)
			return ret;
	}

	chan->saved_ctx_bypass = chan->bypass;
	chan->saved_ctx_pgmode = chan->vi->pg_mode;

	return v4l2_fh_open(fp);
}

static int tegra_channel_close(struct file *fp)
{
	int ret = 0;
	struct video_device *vdev = video_devdata(fp);
	struct tegra_channel *chan = video_get_drvdata(vdev);

	ret = vb2_fop_release(fp);

	if (!chan->saved_ctx_pgmode) {
		/* power off sensors connected in channel */
		ret = tegra_channel_set_power(chan, 0);
		if (ret < 0)
			dev_err(chan->vi->dev,
				"Failed to power off subdevices\n");
	}

	if (!chan->saved_ctx_bypass) {
		int pg_mode = chan->vi->csi->pg_mode;
		/* save TPG mode context for clean closure of CSI */
		chan->vi->csi->pg_mode = chan->saved_ctx_pgmode;
		tegra_csi_power(chan->vi->csi, chan->port, 0);
		chan->vi->csi->pg_mode = pg_mode;
		tegra_vi_power_off(chan->vi);
	}

	return ret;
}

/* -----------------------------------------------------------------------------
 * V4L2 file operations
 */
static const struct v4l2_file_operations tegra_channel_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= video_ioctl2,
	.open		= tegra_channel_open,
	.release	= tegra_channel_close,
	.read		= vb2_fop_read,
	.poll		= vb2_fop_poll,
	.mmap		= vb2_fop_mmap,
};

static void vi_channel_syncpt_init(struct tegra_channel *chan)
{
	chan->syncpt = nvhost_get_syncpt_client_managed(chan->vi->ndev, "vi");
}

static void vi_channel_syncpt_free(struct tegra_channel *chan)
{
	nvhost_syncpt_put_ref_ext(chan->vi->ndev, chan->syncpt);
}

static int tegra_channel_init(struct tegra_mc_vi *vi, unsigned int index)
{
	int ret;
	struct tegra_channel *chan  = &vi->chans[index];

	chan->vi = vi;
	tegra_vi_get_port_info(chan, vi->dev->of_node, index);
	if (csi_port_is_valid(chan->port)) {
		chan->csibase = vi->iomem + TEGRA_VI_CSI_BASE(chan->port);
		set_csi_portinfo(vi->csi, chan->port, chan->numlanes);
	}
	chan->align = 64;
	chan->num_subdevs = 0;
	mutex_init(&chan->video_lock);
	INIT_LIST_HEAD(&chan->capture);
	INIT_LIST_HEAD(&chan->done);
	init_waitqueue_head(&chan->start_wait);
	init_waitqueue_head(&chan->done_wait);
	spin_lock_init(&chan->start_lock);
	spin_lock_init(&chan->done_lock);

	/* Init video format */
	chan->fmtinfo = tegra_core_get_format_by_code(TEGRA_VF_DEF);
	chan->format.pixelformat = chan->fmtinfo->fourcc;
	chan->format.colorspace = V4L2_COLORSPACE_SRGB;
	chan->format.field = V4L2_FIELD_NONE;
	chan->format.width = TEGRA_DEF_WIDTH;
	chan->format.height = TEGRA_DEF_HEIGHT;
	chan->format.bytesperline = chan->format.width * chan->fmtinfo->bpp;
	chan->format.sizeimage = chan->format.bytesperline *
				    chan->format.height;

	/* Initialize the media entity... */
	chan->pad.flags = MEDIA_PAD_FL_SINK;

	ret = media_entity_init(&chan->video.entity, 1, &chan->pad, 0);
	if (ret < 0)
		return ret;

	/* init control handler */
	ret = v4l2_ctrl_handler_init(&chan->ctrl_handler, MAX_CID_CONTROLS);
	if (chan->ctrl_handler.error) {
		dev_err(&chan->video.dev, "failed to init control handler\n");
		goto video_register_error;
	}

	/* init video node... */
	chan->video.fops = &tegra_channel_fops;
	chan->video.v4l2_dev = &vi->v4l2_dev;
	chan->video.queue = &chan->queue;
	snprintf(chan->video.name, sizeof(chan->video.name), "%s-%s-%u",
		 dev_name(vi->dev), "output", chan->port);
	chan->video.vfl_type = VFL_TYPE_GRABBER;
	chan->video.vfl_dir = VFL_DIR_RX;
	chan->video.release = video_device_release_empty;
	chan->video.ioctl_ops = &tegra_channel_ioctl_ops;
	chan->video.ctrl_handler = &chan->ctrl_handler;
	chan->video.lock = &chan->video_lock;

	video_set_drvdata(&chan->video, chan);

	vi_channel_syncpt_init(chan);

	/* get the buffers queue... */
	chan->alloc_ctx = vb2_dma_contig_init_ctx(chan->vi->dev);
	if (IS_ERR(chan->alloc_ctx)) {
		dev_err(chan->vi->dev, "failed to init vb2 buffer\n");
		ret = -ENOMEM;
		goto vb2_init_error;
	}

	chan->queue.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	chan->queue.io_modes = VB2_MMAP | VB2_DMABUF | VB2_READ | VB2_USERPTR;
	chan->queue.lock = &chan->video_lock;
	chan->queue.drv_priv = chan;
	chan->queue.buf_struct_size = sizeof(struct tegra_channel_buffer);
	chan->queue.ops = &tegra_channel_queue_qops;
	chan->queue.mem_ops = &vb2_dma_contig_memops;
	chan->queue.timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC
				   | V4L2_BUF_FLAG_TSTAMP_SRC_EOF;
	ret = vb2_queue_init(&chan->queue);
	if (ret < 0) {
		dev_err(chan->vi->dev, "failed to initialize VB2 queue\n");
		goto vb2_queue_error;
	}

	ret = video_register_device(&chan->video, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		dev_err(&chan->video.dev, "failed to register video device\n");
		goto video_register_error;
	}

	return 0;

video_register_error:
	vb2_queue_release(&chan->queue);
vb2_queue_error:
	vb2_dma_contig_cleanup_ctx(chan->alloc_ctx);
vb2_init_error:
	media_entity_cleanup(&chan->video.entity);
	return ret;
}

static int tegra_channel_cleanup(struct tegra_channel *chan)
{
	video_unregister_device(&chan->video);

	v4l2_ctrl_handler_free(&chan->ctrl_handler);
	vb2_queue_release(&chan->queue);
	vb2_dma_contig_cleanup_ctx(chan->alloc_ctx);

	vi_channel_syncpt_free(chan);
	media_entity_cleanup(&chan->video.entity);

	return 0;
}

int tegra_vi_channels_init(struct tegra_mc_vi *vi)
{
	unsigned int i;
	int ret;

	for (i = 0; i < vi->num_channels; i++) {
		ret = tegra_channel_init(vi, i);
		if (ret < 0) {
			dev_err(vi->dev, "channel %d init failed\n", i);
			return ret;
		}
	}
	return 0;
}

int tegra_vi_channels_cleanup(struct tegra_mc_vi *vi)
{
	unsigned int i;
	int ret;

	for (i = 0; i < vi->num_channels; i++) {
		ret = tegra_channel_cleanup(&vi->chans[i]);
		if (ret < 0) {
			dev_err(vi->dev, "channel %d cleanup failed\n", i);
			return ret;
		}
	}
	return 0;
}
