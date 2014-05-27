/*
 * resizer_ss.c
 *
 * Example showing how to use Resizer in One shot mode to do Resize and
 * also format conversion from UYVY to YUV420 Semi planar format (aka
 * NV12 in v4l2 API)
 *
 * The following IP configuration used for this use case
 *
 * SDRAM -> IPIPEIF -> Resizer -> SDRAM
 *
 * Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of Texas Instruments Incorporated nor the names of
 * its contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <getopt.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <media/davinci/imp_previewer.h>
#include <media/davinci/imp_resizer.h>
#include <media/davinci/dm365_ipipe.h>
#include <linux/v4l2-subdev.h>
#include <linux/media.h>

#include "cmem.h"
#include "common.h"

#define ALIGN(x, y)	(((x + (y-1))/y)*y)
#define CLEAR(x) memset (&(x), 0, sizeof (x))

void usage()
{
	printf("Usage: do_resize [OPTIONS]\n");
	printf("Options:\n");
	printf("\t-i\t1280x720 UYVY input file\n");
	printf("\t-w\tWidth of the output image (stored in output1.yuv)\n");
	printf("\t-h\tHeight of the output image (stored in output1.yuv)\n");
	printf("\t-x\tCreate a second output image of 640x480 resolution\n");
	printf("\t\t\t0 => Do not create second image [DEFAULT without -x]\n");
	printf("\t\t\t1 => Create a second image\n");
	printf("\t-t\tFormat of the output file\n");
	printf("\t\t\t0 => UYVY format [DEFAULT without -t]\n");
	printf("\t\t\t1 => YUV 420 Semi-Planar format\n");

}

struct media_entity_desc	*entity[15];
struct media_links_enum		links;
int				entities_count;

char previewer_subdev[30];
char resizer_subdev[30];

#define MAX_WIDTH  1920
#define MAX_HEIGHT  1080
#define BYTESPERLINE 2
#define INPUT_HEIGHT 720
#define INPUT_WIDTH 1280

#define BUF_SIZE ((1280*720) + (1280*360) + 32)
char in_buf[MAX_WIDTH*MAX_HEIGHT*BYTESPERLINE];

#define OUTPUT1_FILE "./output1.yuv"
#define OUTPUT2_FILE "./output2.yuv"
#define ALIGN(x, y)	(((x + (y-1))/y)*y)

struct buf_info {
	void *user_addr;
	unsigned long phy_addr;
};

#define APP_NUM_BUFS	1

/* For input and output frame */
struct buf_info *input_buffers[APP_NUM_BUFS];
struct buf_info *output1_buffers[APP_NUM_BUFS];
struct buf_info *output2_buffers[APP_NUM_BUFS];

static unsigned long long prev_ts;
static unsigned long long curr_ts;
static unsigned long fp_period_average;
static unsigned long fp_period_max;
static unsigned long fp_period_min;
static unsigned long fp_period;

/*******************************************************************************
 * allocate_user_buffers() allocate buffer using CMEM
 ******************************************************************************/
int allocate_user_buffers(int in_buf_size, int out1_buf_size, int out2_buf_size)
{
	void *pool;
	int i;

	CMEM_AllocParams  alloc_params;
	printf("calling cmem utilities for allocating frame buffers\n");
	CMEM_init();

	alloc_params.type = CMEM_POOL;
	alloc_params.flags = CMEM_NONCACHED;
	alloc_params.alignment = 32;
	pool = CMEM_allocPool(0, &alloc_params);

	if (NULL == pool) {
		printf("Failed to allocate cmem pool\n");
		return -1;
	}
	printf("Allocating input buffers :buf size = %d \n", in_buf_size);

	for (i=0; i < APP_NUM_BUFS; i++) {
		input_buffers[i]->user_addr = CMEM_alloc(in_buf_size, &alloc_params);
		if (input_buffers[i]->user_addr) {
			input_buffers[i]->phy_addr = CMEM_getPhys(input_buffers[i]->user_addr);
			if (0 == input_buffers[i]->phy_addr) {
				printf("Failed to get phy cmem buffer address\n");
				return -1;
			}
		} else {
			printf("Failed to allocate cmem buffer\n");
			return -1;
		}
		printf("Got %p from CMEM, phy = %p\n", input_buffers[i]->user_addr,
			(void *)input_buffers[i]->phy_addr);
	}

	printf("**********************************************\n");

	printf("Allocating output1 buffers :buf size = %d \n", out1_buf_size);

	for (i=0; i < APP_NUM_BUFS; i++) {
		output1_buffers[i]->user_addr = CMEM_alloc(out1_buf_size, &alloc_params);
		if (output1_buffers[i]->user_addr) {
			output1_buffers[i]->phy_addr = CMEM_getPhys(output1_buffers[i]->user_addr);
			if (0 == output1_buffers[i]->phy_addr) {
				printf("Failed to get phy cmem buffer address\n");
				return -1;
			}
		} else {
			printf("Failed to allocate cmem buffer\n");
			return -1;
		}
		printf("Got %p from CMEM, phy = %p\n", output1_buffers[i]->user_addr,
			(void *)output1_buffers[i]->phy_addr);
	}
	printf("**********************************************\n");

	return 0;
}

int main(int argc, char *argp[])
{
	char shortoptions[] = "i:w:h:t:x:";
	int mode = O_RDWR,c,ret = 0,index;
	int preview_fd, resizer_fd = 0, dev_idx;
	unsigned long /*oper_mode, user_mode,*/ driver_mode;
	struct prev_channel_config prev_chan_config;
	struct prev_single_shot_config prev_ss_config; // single shot mode configuration
	struct rsz_channel_config rsz_chan_config; // resizer channel config
	struct rsz_single_shot_config rsz_ss_config; // single shot mode configuration
	struct prev_cap cap;
	struct prev_module_param mod_param;
	struct prev_wb wb_params;
	struct imp_reqbufs req_buf;
	struct imp_buffer buf_in[3];
	struct imp_buffer buf_out1[3];
	struct imp_buffer buf_out2[3];
	struct imp_convert convert;
	int i,j,h, num_bytes;
	char in_file[100] = "";
	FILE *inp_f = NULL, *outp1_f = NULL, *outp2_f = NULL;
	int width = INPUT_WIDTH, height = INPUT_HEIGHT, size;
	int out_format=0;
	unsigned long level;
	short byte=0;
	char out420_buf[BUF_SIZE];
	char *src, *dest;
	int second_output = 0;
	int output1_size, output2_size;

	/* mc additions */
	int media_fd =0, vidout_fd = 0, vidin_fd = 0, frame_count = 0, capture_pitch = 0;
	struct media_link_desc link;
	struct v4l2_format v4l2_fmt;
	enum v4l2_buf_type type;
	char *source;
	unsigned long temp;
	struct v4l2_buffer cap_buf, input_buf;
	struct v4l2_requestbuffers req;
	struct v4l2_subdev_format fmt;
	__u32 code = 0;

	__u32 id;
	struct stat devstat;
	char devname[32];
	char sysname[32];
	char target[1024];
	char *p;

	int E_RSZ_VID_OUT;
	int E_RSZ_VID_IN;
	int E_RSZ;

	for(;;) {
		c = getopt_long(argc, argp, shortoptions, NULL, (void *)&index);
		if(-1 == c)
			break;
		switch(c) {
			case 'i':
				strcpy(in_file,optarg);
				break;
			case 'w':
				width = atoi(optarg);
				break;
			case 'h':
				height = atoi(optarg);
				break;
			case 'x':
				second_output = atoi(optarg);
				break;
			case 't':
				out_format = atoi(optarg);
				break;
			default:
				usage();
				exit(1);
		}
	}

	if (!strcmp(in_file, "")) {
		printf("Use -i option to provide an 1280x720 UYVY input file\n"
				"An example input is located with the sources: "
				"video_720p\n");
		usage();
		exit(1);
	}

	printf("doing resize on in_file = %s and writing out to out_file1 = %s out_file2 = %s\n",
		in_file,OUTPUT1_FILE, OUTPUT2_FILE);

	printf("starting\n");
	inp_f = fopen(in_file, "rb");
	if (inp_f == NULL) {
		perror("Error in opening input file \n");
		ret = -1;
		goto out;
	}
	outp1_f = fopen(OUTPUT1_FILE, "wb");
	if (outp1_f == NULL) {
		perror("Error in opening output file \n");
		ret = -1;
		goto out;
	}

	if (second_output) {
		outp2_f = fopen(OUTPUT2_FILE, "wb");
		if (outp2_f == NULL) {
			perror("Error in opening output file \n");
			ret = -1;
			goto out;
		}
	}

	size = fread(in_buf,1, (INPUT_WIDTH * INPUT_HEIGHT * BYTESPERLINE), inp_f);

	output1_size = width * height * 2;
	output2_size = 640 * 480 * 2;
	code = V4L2_MBUS_FMT_YUYV8_2X8;

	if (out_format) {
		int pitch = ALIGN(width, 32);

		output1_size =  pitch * height * 1.5;
		output2_size = 640 * 480 * 1.5;
		code = V4L2_MBUS_FMT_NV12_1X20;
	}

	if (size != (INPUT_WIDTH * INPUT_HEIGHT * BYTESPERLINE)) {
		perror("mismatch between file size and  INPUT_WIDTH * INPUT_HEIGHT * 2\n");
		ret = -1;
		goto out;
	}

	for (i=0;i< APP_NUM_BUFS;i++){
		input_buffers[i] = (struct buf_info *)malloc(sizeof(struct buf_info));
		output1_buffers[i] = (struct buf_info *)malloc(sizeof(struct buf_info));
		output2_buffers[i] = (struct buf_info *)malloc(sizeof(struct buf_info));
	}

	for (i=0;i< 16;i++){
		entity[i] = (struct media_entity_desc *)malloc(sizeof(struct media_entity_desc));
	}
	
	media_fd = open("/dev/media0", O_RDWR);
	if (media_fd < 0) {
		printf("%s: Can't open media device %s\n", __func__, "/dev/media0");
		goto out;
	}

	/* 4.enumerate media-entities */
	printf("4.enumerating media entities\n");
	index = 0;
	do {
		memset(entity[index], 0, sizeof(*entity[0]));
		entity[index]->id = index | MEDIA_ENT_ID_FLAG_NEXT;

		ret = ioctl(media_fd, MEDIA_IOC_ENUM_ENTITIES, entity[index]);
		if (ret < 0) {
			if (errno == EINVAL)
				break;
		} else{
			printf("[%x]:%s\n", entity[index]->id, entity[index]->name);

			if (!strcmp(entity[index]->name, E_VIDEO_RSZ_OUT_NAME)) {
				E_RSZ_VID_OUT =  entity[index]->id;
			}
			else if (!strcmp(entity[index]->name, E_VIDEO_RSZ_IN_NAME)) {
				E_RSZ_VID_IN =  entity[index]->id;
			}
			else if (!strcmp(entity[index]->name, E_RSZ_NAME)) {
				E_RSZ =  entity[index]->id;
			}

			/* Find the corresponding device name. */
			sprintf(sysname, "/sys/dev/char/%u:%u", entity[index]->v4l.major,
				entity[index]->v4l.minor);

			ret = readlink(sysname, target, sizeof(target));
			if (ret < 0)
				continue;

			target[ret] = '\0';
			p = strrchr(target, '/');
			if (p == NULL)
				continue;

			sprintf(devname, "/dev/%s", p + 1);
			ret = stat(devname, &devstat);
			if (ret < 0)
				continue;

			/* Sanity check: udev might have reordered the device nodes.
			* Make sure the major/minor match. We should really use
			* libudev.
			*/
			if (major(devstat.st_rdev) == entity[index]->v4l.major &&
				minor(devstat.st_rdev) == entity[index]->v4l.minor) {
				if (!strcmp(entity[index]->name , E_PRV_NAME)) {
					strcpy(previewer_subdev, devname);
				} else if (!strcmp(entity[index]->name , E_RSZ_NAME)) {
					strcpy(resizer_subdev, devname);
				}
			}
		}

		index++;
	}while(ret == 0);
	entities_count = index;
	printf("total number of entities: %x\n", entities_count);
	printf("**********************************************\n");

	/* 5.enumerate links for each entity */
	printf("5.enumerating links/pads for entities\n");
	for(index = 0; index < entities_count; index++) {

		links.entity = entity[index]->id;

		links.pads = malloc(sizeof( struct media_pad_desc) * entity[index]->pads);
		links.links = malloc(sizeof(struct media_link_desc) * entity[index]->links);

		ret = ioctl(media_fd, MEDIA_IOC_ENUM_LINKS, &links);
		if (ret < 0) {
			if (errno == EINVAL)
				break;
		}else{
			/* display pads info first */
			if(entity[index]->pads)
				printf("pads for entity %x=", entity[index]->id);

			for(i = 0;i< entity[index]->pads; i++)
			{
				printf("(%x, %s) ", links.pads->index,(links.pads->flags & MEDIA_PAD_FL_INPUT)?"INPUT":"OUTPUT");
				links.pads++;
			}

			printf("\n");

			/* display links now */
			for(i = 0;i< entity[index]->links; i++)
			{
				printf("[%x:%x]-------------->[%x:%x]",links.links->source.entity,
				       links.links->source.index,links.links->sink.entity,links.links->sink.index);
				       if(links.links->flags & MEDIA_LNK_FL_ENABLED)
						printf("\tACTIVE\n");
				       else
						printf("\tINACTIVE \n");

				links.links++;
			}

			printf("\n");
		}
	}

	printf("**********************************************\n");
	/* 8.enable 'rsz-vid-in-->rsz' link */
	printf("7. ENABLEing link [rsz-vid-in]----------->[rsz]\n");
	memset(&link, 0, sizeof(link));

	link.flags |=  MEDIA_LNK_FL_ENABLED;
	link.source.entity = E_RSZ_VID_IN;
	link.source.index = P_RSZ_VID_IN;
	link.source.flags = MEDIA_PAD_FL_OUTPUT;

	link.sink.entity = E_RSZ;
	link.sink.index = P_RSZ_SINK;
	link.sink.flags = MEDIA_PAD_FL_INPUT;

	ret = ioctl(media_fd, MEDIA_IOC_SETUP_LINK, &link);
	if(ret) {
		printf("failed to enable link between rsz-vid-in and rsz\n");
		goto out;
	} else
		printf("[rsz-vid-in]----------->[rsz]\tENABLED\n");

	/* 9. enable 'rsz->rsz-vid-out' link */
	printf("8. ENABLEing link [rsz]----------->[rsz-vid-out]\n");
	memset(&link, 0, sizeof(link));

	link.flags |=  MEDIA_LNK_FL_ENABLED;
	link.source.entity = E_RSZ;
	link.source.index = P_RSZ_SOURCE;
	link.source.flags = MEDIA_PAD_FL_OUTPUT;

	link.sink.entity = E_RSZ_VID_OUT;
	link.sink.index = P_RSZ_VID_OUT;
	link.sink.flags = MEDIA_PAD_FL_INPUT;

	ret = ioctl(media_fd, MEDIA_IOC_SETUP_LINK, &link);
	if(ret) {
		printf("failed to enable link between rsz and rsz-vid-out\n");
		goto out;
	} else
		printf("[rsz]----------->[rsz-vid-out]\t ENABLED\n");


	/* Open the resizer */
	printf("Configuring resizer in the chain mode\n");
	printf("Opening resizer device\n");
	//resizer_fd = open((const char *)dev_name_rsz[0], mode);
	resizer_fd = open(resizer_subdev, O_RDWR);
	if(resizer_fd <= 0) {
		printf("Cannot open resizer device\n");
		goto out;
	}

	/* set format on pad of rsz */
	/* 18. set format on sink-pad of rsz */
	printf("10. setting format on sink-pad of rsz entity. . . \n");
	memset(&fmt, 0, sizeof(fmt));

	fmt.pad = P_RSZ_SINK;
	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.format.code = code;
	fmt.format.width = INPUT_WIDTH;
	fmt.format.height = INPUT_HEIGHT;
	fmt.format.field = V4L2_FIELD_NONE;

	ret = ioctl(resizer_fd, VIDIOC_SUBDEV_S_FMT, &fmt);
	if(ret) {
		printf("1: failed to set format on pad %x\n", fmt.pad);
		goto out;
	}
	else
		printf("successfully format is set on pad %x\n", fmt.pad);

	
	/* 19. set format on source-pad of rsz */
	printf("10. setting format on source-pad of rsz entity. . . \n");
	memset(&fmt, 0, sizeof(fmt));

	fmt.pad = P_RSZ_SOURCE;
	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.format.code = code;
	fmt.format.width = width;
	fmt.format.height = height;
	fmt.format.field = V4L2_FIELD_NONE;

	ret = ioctl(resizer_fd, VIDIOC_SUBDEV_S_FMT, &fmt);
	if(ret) {
		printf("2: failed to set format on pad %x\n", fmt.pad);
		goto out;
	}
	else
		printf("successfully format is set on pad %x\n", fmt.pad);


	printf("Setting default configuration in Resizer\n");
	bzero(&rsz_ss_config, sizeof(struct rsz_single_shot_config));
	//rsz_chan_config.oper_mode = IMP_MODE_SINGLE_SHOT;
	rsz_chan_config.chain = 0;
	rsz_chan_config.len = 0;
	rsz_chan_config.config = NULL; /* to set defaults in driver */
	if (ioctl(resizer_fd, RSZ_S_CONFIG, &rsz_chan_config) < 0) {
		perror("Error in setting default configuration for single shot mode\n");
		ret = -1;
		goto out;
	}

	printf("default configuration setting in Resizer successfull\n");
	bzero(&rsz_ss_config, sizeof(struct rsz_single_shot_config));
	//rsz_chan_config.oper_mode = IMP_MODE_SINGLE_SHOT;
	rsz_chan_config.chain = 0;
	rsz_chan_config.len = sizeof(struct rsz_single_shot_config);
	rsz_chan_config.config = &rsz_ss_config;

	if (ioctl(resizer_fd, RSZ_G_CONFIG, &rsz_chan_config) < 0) {
		perror("Error in getting resizer channel configuration from driver\n");
		ret = -1;
		goto out;
	}

	// in the chain mode only output configurations are valid
	// input params are set at the previewer
	rsz_ss_config.input.image_width = INPUT_WIDTH;
	rsz_ss_config.input.image_height = INPUT_HEIGHT;
	rsz_ss_config.input.ppln = rsz_ss_config.input.image_width + 8;
	rsz_ss_config.input.lpfr = rsz_ss_config.input.image_height + 10;
	rsz_ss_config.input.pix_fmt = IPIPE_UYVY;
	if (out_format)
		rsz_ss_config.output1.pix_fmt = IPIPE_YUV420SP;
	else
		rsz_ss_config.output1.pix_fmt = IPIPE_UYVY;
	rsz_ss_config.output1.enable = 1;
	rsz_ss_config.output1.width = width;
	rsz_ss_config.output1.height = height;
	rsz_ss_config.output2.enable = 0;
	if (second_output) {
		rsz_ss_config.output2.enable = 1;
		rsz_ss_config.output2.width = 640;
		rsz_ss_config.output2.height = 480;
		if (out_format)
			rsz_ss_config.output2.pix_fmt = IPIPE_YUV420SP;
		else
			rsz_ss_config.output2.pix_fmt = IPIPE_UYVY;
	}

	//rsz_chan_config.oper_mode = IMP_MODE_SINGLE_SHOT;
	rsz_chan_config.chain = 0;
	rsz_chan_config.len = sizeof(struct rsz_single_shot_config);
	if (ioctl(resizer_fd, RSZ_S_CONFIG, &rsz_chan_config) < 0) {
		perror("Error in setting default configuration for single shot mode\n");
		ret = -1;
		goto out;
	}
	//rsz_chan_config.oper_mode = IMP_MODE_SINGLE_SHOT;
	rsz_chan_config.chain = 0;
	rsz_chan_config.len = sizeof(struct rsz_single_shot_config);

	// read again and verify
	if (ioctl(resizer_fd, RSZ_G_CONFIG, &rsz_chan_config) < 0) {
		perror("Error in getting configuration from driver\n");
		ret = -1;
		goto out;
	}

	if (allocate_user_buffers(size, output1_size, output2_size) < 0) {
		perror("Error in allocating user buffers\n");
		ret = -1;
		goto out;
	}

	/* copy contents of in_buf into input_buffers */
	for(i = 0; i < APP_NUM_BUFS; i++)
		memcpy(input_buffers[i]->user_addr, in_buf, size);


	if ((vidin_fd = open("/dev/video3", O_RDWR | O_NONBLOCK, 0)) <= -1) {
		printf("failed to open %s \n", "/dev/video3");
		goto out;
	} else
		printf("successfully opened /dev/video3\n");

	if ((vidout_fd = open("/dev/video4", O_RDWR | O_NONBLOCK, 0)) <= -1) {
		printf("failed to open %s \n", "/dev/video4");
		goto out;
	} else
		printf("successfully opened /dev/video4\n");

	printf("15.setting format V4L2_PIX_FMT_UYVY\n");
	CLEAR(v4l2_fmt);
	v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	v4l2_fmt.fmt.pix.width = INPUT_WIDTH;
	v4l2_fmt.fmt.pix.height = INPUT_HEIGHT;
	v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
	v4l2_fmt.fmt.pix.field = V4L2_FIELD_NONE;

	if (-1 == ioctl(vidin_fd, VIDIOC_S_FMT, &v4l2_fmt)) {
		printf("3: failed to set format on video-in device \n");
		goto out;
	} else
		printf("successfully set the format\n");

	/* 31.setting format */
	printf("15.setting format V4L2_PIX_FMT_UYVY\n");
	CLEAR(v4l2_fmt);
	v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	v4l2_fmt.fmt.pix.width = width;
	v4l2_fmt.fmt.pix.height = height;
	if (out_format)
		v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
	else
		v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
	v4l2_fmt.fmt.pix.field = V4L2_FIELD_NONE;

	if (-1 == ioctl(vidout_fd, VIDIOC_S_FMT, &v4l2_fmt)) {
		printf("4 :failed to set format on captute device \n");
		goto out;
	} else
		printf("successfully set the format\n");

	/* 32.call G_FMT for knowing picth */
	if (-1 == ioctl(vidout_fd, VIDIOC_G_FMT, &v4l2_fmt)) {
		printf("failed to get format from captute device \n");
		goto out;
	} else {
		printf("capture_pitch: %d\n", v4l2_fmt.fmt.pix.bytesperline);
		capture_pitch = v4l2_fmt.fmt.pix.bytesperline;
	}
	printf("**********************************************\n");

	/* 33.make sure 3 buffers are supported for streaming on vid-in*/
	CLEAR(req);
	req.count = APP_NUM_BUFS;
	req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	req.memory = V4L2_MEMORY_USERPTR;

	ret = ioctl(vidin_fd, VIDIOC_REQBUFS, &req);
	if(ret) {
		printf("call to VIDIOC_REQBUFS on vid-in failed %x\n", ret);
		goto out;
	}

	if (req.count != APP_NUM_BUFS) {
		printf("%d buffers not supported by capture device", APP_NUM_BUFS);
		goto out;
	} else
		printf("%d buffers are supported for streaming\n", APP_NUM_BUFS);

	printf("**********************************************\n");

	/* 34.queue the buffers */
	for (i = 0; i < APP_NUM_BUFS; i++) {
		struct v4l2_buffer buf;
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buf.memory = V4L2_MEMORY_USERPTR;
		buf.index = i;
		buf.length = size;
		buf.m.userptr = (unsigned long)input_buffers[i]->user_addr;

		ret = ioctl(vidin_fd, VIDIOC_QBUF, &buf);
		if(ret) {
			printf("call to VIDIOC_QBUF on vid-in failed %x\n", ret);
			goto out;
		}
	}

	/* 33.make sure 3 buffers are supported for streaming on vid-out*/
	CLEAR(req);
	req.count = APP_NUM_BUFS;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	if (-1 == ioctl(vidout_fd, VIDIOC_REQBUFS, &req)) {
		printf("call to VIDIOC_REQBUFS on vid-out failed %x\n", ret);
		goto out;
	}

	if (req.count != APP_NUM_BUFS) {
		printf("%d buffers not supported by capture device", APP_NUM_BUFS);
		goto out;
	} else
		printf("%d buffers are supported for streaming\n", APP_NUM_BUFS);

	printf("**********************************************\n");

	/* 34.queue the buffers */
	for (i = 0; i < APP_NUM_BUFS; i++) {
		struct v4l2_buffer buf;
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_USERPTR;
		buf.index = i;
		buf.length = output1_size;
		buf.m.userptr = (unsigned long)output1_buffers[i]->user_addr;

		ret == ioctl(vidout_fd, VIDIOC_QBUF, &buf);
		if (ret) {
			printf("call to VIDIOC_QBUF on vid-out failed %x\n", ret);
			goto out;
		}
	}

	/* 35.start streaming */
	CLEAR(type);
	type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	if (-1 == ioctl(vidin_fd, VIDIOC_STREAMON, &type)) {
		printf("failed to start streaming on vidin device");
		goto out;
	} else
		printf("streaming started successfully\n");

	CLEAR(type);
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == ioctl(vidout_fd, VIDIOC_STREAMON, &type)) {
		printf("failed to start streaming on vidout device");
		goto out;
	} else
		printf("streaming started successfully\n");


	/* 36.get 5 frames from capture device and store in a file */
	frame_count = 0;

	while(frame_count != 5) {

		CLEAR(cap_buf);
		CLEAR(input_buf);

		cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cap_buf.memory = V4L2_MEMORY_USERPTR;
try_again:
		ret = ioctl(vidout_fd, VIDIOC_DQBUF, &cap_buf);
		if (ret < 0) {
			if (errno == EAGAIN) {
				goto try_again;
			}
			printf("failed to DQ buffer from vid-out device\n");
			goto out;
		}
try2_again:
		input_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		input_buf.memory = V4L2_MEMORY_USERPTR;
		ret = ioctl(vidin_fd, VIDIOC_DQBUF, &input_buf);
		if (ret < 0) {
			if (errno == EAGAIN) {
				goto try2_again;
			}
			printf("failed to DQ buffer from vid-in device\n");
			goto out;
		}

		temp = cap_buf.m.userptr;
		source = (char *)temp;

		if (out_format) {
			h = height + height/2;
			num_bytes = width;
		}
		else {
			h = height;
			num_bytes = width*2;
		}

		/* copy frame to a file */
		for(i=0 ; i < h; i++) {
			fwrite(source, 1 , num_bytes, outp1_f);
			source += capture_pitch;
		}


		ret = ioctl(vidin_fd, VIDIOC_QBUF, &input_buf);
		if (ret < 0) {
			printf("failed to Q buffer onto vid-in device, ret = %x\n", ret);
			goto out;
		}

		/* Q the buffer for capture, again */
		ret = ioctl(vidout_fd, VIDIOC_QBUF, &cap_buf);
		if (ret < 0) {
			printf("failed to Q buffer onto vid-out device, ret = %x\n", ret);
			goto out;
		}

		if (frame_count == 0)
			prev_ts = (cap_buf.timestamp.tv_sec*1000000) + cap_buf.timestamp.tv_usec;
		else {
			curr_ts = (cap_buf.timestamp.tv_sec*1000000) + cap_buf.timestamp.tv_usec;
			fp_period = curr_ts - prev_ts;
			if (frame_count == 1) {
				fp_period_max = fp_period_min = fp_period_average = fp_period;
			}
			else {
				/* calculate jitters and average */
				if (fp_period > fp_period_max)
					fp_period_max = fp_period;
				if (fp_period < fp_period_min)
					fp_period_min = fp_period;

				fp_period_average =
					((fp_period_average * frame_count) +
					fp_period)/(frame_count + 1);
			}
			prev_ts = curr_ts;
		}

		frame_count++;

		printf("frame:%5u, ", frame_count);
		printf("buf.timestamp:%lu:%lu\n",
			cap_buf.timestamp.tv_sec, cap_buf.timestamp.tv_usec);
	}

	printf("**********************************************\n");

out:

	/* 35.start streaming */
	CLEAR(type);
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == ioctl(vidout_fd, VIDIOC_STREAMOFF, &type)) {
		printf("failed to stop streaming on vidout device");
	} else
		printf("streaming stopped successfully\n");

	CLEAR(type);
	type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	if (-1 == ioctl(vidin_fd, VIDIOC_STREAMOFF, &type)) {
		printf("failed to stop streaming on vidin device");
	} else
		printf("streaming stopped successfully\n");


	for(index = 0; index < entities_count; index++) {

		links.entity = entity[index]->id;

		links.pads = malloc(sizeof( struct media_pad_desc) * entity[index]->pads);
		links.links = malloc(sizeof(struct media_link_desc) * entity[index]->links);

		ret = ioctl(media_fd, MEDIA_IOC_ENUM_LINKS, &links);
		if (ret < 0) {
			if (errno == EINVAL)
				break;
		}else{

			for(i = 0;i< entity[index]->links; i++)
			{		/* go through each active link */
				       if(links.links->flags & MEDIA_LNK_FL_ENABLED) {
					        /* de-enable the link */
					        memset(&link, 0, sizeof(link));

						link.flags |=  ~MEDIA_LNK_FL_ENABLED;
						link.source.entity = links.links->source.entity;
						link.source.index = links.links->source.index;
						link.source.flags = MEDIA_PAD_FL_OUTPUT;

						link.sink.entity = links.links->sink.entity;
						link.sink.index = links.links->sink.index;
						link.sink.flags = MEDIA_PAD_FL_INPUT;

						ret = ioctl(media_fd, MEDIA_IOC_SETUP_LINK, &link);
						if(ret) {
							printf("failed to de-enable link \n");
						}

				       }

				links.links++;
			}
		}
	}
	
	if (resizer_fd) {
		if (!close(resizer_fd))
			printf("resizer closed successfully\n");
		else
			printf("Error in closing resizer\n");
	}
	if(vidin_fd) {
		close(vidin_fd);
		printf("video-in closed successfully\n");
	}
	if(vidout_fd) {
		close(vidout_fd);
		printf("video-out closed successfully\n");
	}
	if(media_fd) {
		close(media_fd);
		printf("media device closed successfully\n");
	}
	if (inp_f)
		fclose(inp_f);
	if (outp1_f)
		fclose(outp1_f);
	if (outp2_f)
		fclose(outp2_f);

	return ret;
}

