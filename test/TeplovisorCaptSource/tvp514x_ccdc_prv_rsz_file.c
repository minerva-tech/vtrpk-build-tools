
#include <stdio.h>
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
#include <linux/videodev.h>
#include <linux/videodev2.h>
#include <media/davinci/imp_previewer.h>
#include <media/davinci/imp_resizer.h>
#include <media/davinci/dm365_ipipe.h>
#include <media/davinci/vpfe_capture.h>
#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include "cmem.h"
#include "common.h"
#include <media/davinci/videohd.h> //NAG
/*******************************************************************************/
#define ALIGN(x, y) (((x + (y-1))/y)*y)
#define CLEAR(x) memset (&(x), 0, sizeof (x))

/* structure to hold address */
struct buf_info {
    void *user_addr;
    unsigned long phy_addr;
};

#define APP_NUM_BUFS    3
/* capture buffer addresses stored here */
struct buf_info     capture_buffers[APP_NUM_BUFS];

struct media_entity_desc    entity[15];
struct media_links_enum     links;
int entities_count;

#define CODE    V4L2_MBUS_FMT_YUYV8_2X8

#define WIDTH_IN   720
#define HEIGHT_IN  480
#define WIDTH_OUT  384
#define HEIGHT_OUT 144

#define BUFFER_SIZE (WIDTH_OUT * HEIGHT_OUT * 3 / 2)

int width, height;
int buf_size = ALIGN((360*240*2), 4096);

static unsigned long long prev_ts;
static unsigned long long curr_ts;
static unsigned long fp_period_average;
static unsigned long fp_period_max;
static unsigned long fp_period_min;
static unsigned long fp_period;

/*******************************************************************************
 * allocate_user_buffers() allocate buffer using CMEM
 ******************************************************************************/
int allocate_user_buffers(int buf_size)
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
	printf("Allocating capture buffers :buf size = %d \n", buf_size);

	for (i=0; i < APP_NUM_BUFS; i++) {
		capture_buffers[i].user_addr = CMEM_alloc(buf_size, &alloc_params);
		if (capture_buffers[i].user_addr) {
			capture_buffers[i].phy_addr = CMEM_getPhys(capture_buffers[i].user_addr);
			if (0 == capture_buffers[i].phy_addr) {
				printf("Failed to get phy cmem buffer address\n");
				return -1;
			}
		} else {
			printf("Failed to allocate cmem buffer\n");
			return -1;
		}
		printf("Got %p from CMEM, phy = %p\n", capture_buffers[i].user_addr,
			(void *)capture_buffers[i].phy_addr);
	}

	printf("**********************************************\n");

	return 0;
}

int main(int argc, char *argp[])
{
    int i, index, media_fd = 0, teplovisor_fd = 0, ccdc_fd = 0, prv_fd = 0, rsz_fd = 0, capt_fd = 0,
        ret, capture_pitch, frame_count,
        output_format;
	struct media_link_desc link;
	struct v4l2_subdev_format fmt;
	struct v4l2_input input;
	struct v4l2_dv_preset preset;
	struct v4l2_format v4l2_fmt;
	struct v4l2_requestbuffers req;
	enum v4l2_buf_type type;
	char *source;
	struct v4l2_buffer cap_buf;
	unsigned long temp;
	void * dmptr;
	struct prev_channel_config prev_chan_config;
	struct prev_continuous_config prev_cont_config;
	struct prev_cap cap;
	struct prev_module_param mod_param;
	struct rsz_channel_config rsz_chan_config;
	struct rsz_continuous_config rsz_cont_config;
	v4l2_std_id cur_std;
	int E_VIDEO;
	int E_TEPLOVISOR;
	int E_CCDC;
	int E_PRV;
	int E_RSZ;
	FILE* file = NULL;

    /* 2.allocate buffers using cmem */
    if (allocate_user_buffers(ALIGN((BUFFER_SIZE), 4096)) < 0) {
        printf("Unable to Allocate user buffewidth, height,rs\n");
        goto cleanup;
    }

    /* 3.open media device */
    media_fd = open("/dev/media0", O_RDWR);
    if (media_fd < 0) {
        printf("%s: Can't open media device %s\n", __func__, "/dev/media0");
        goto cleanup;
    }

    /* 4.enumerate media-entities */
    index = 0;
    do {
        memset(&entity[index], 0, sizeof(entity));
        entity[index].id = index | MEDIA_ENT_ID_FLAG_NEXT;

        ret = ioctl(media_fd, MEDIA_IOC_ENUM_ENTITIES, &entity[index]);

        if (ret < 0) {
            if (errno == EINVAL) break;
        }
        else {
            if (!strcmp(entity[index].name, E_VIDEO_RSZ_OUT_NAME))
                E_VIDEO =  entity[index].id;
            else if (!strcmp(entity[index].name, "teplovisor"))
                E_TEPLOVISOR =  entity[index].id;
            else if (!strcmp(entity[index].name, E_CCDC_NAME))
                E_CCDC =  entity[index].id;
            else if (!strcmp(entity[index].name, E_PRV_NAME))
                E_PRV =  entity[index].id;
            else if (!strcmp(entity[index].name, E_RSZ_NAME))
                E_RSZ =  entity[index].id;
            printf("[%x]:%s\n", entity[index].id, entity[index].name);
        }
        index++;
    } while (ret == 0);
    entities_count = index;
    printf("total number of entities: %x\n", entities_count);

    /* 5.enumerate links for each entity */
    for(index = 0; index < entities_count; index++) {

        links.entity = entity[index].id;
        links.pads = malloc(sizeof( struct media_pad_desc) * entity[index].pads);
        links.links = malloc(sizeof(struct media_link_desc) * entity[index].links);

        ret = ioctl(media_fd, MEDIA_IOC_ENUM_LINKS, &links);
        if (ret < 0) {
            if (errno == EINVAL) break;
        }
        else {
            /* display pads info first */
            if(entity[index].pads) printf("pads for entity %x=", entity[index].id);
            for(i = 0;i< entity[index].pads; i++) {
                printf("(%x, %s) ", links.pads->index,(links.pads->flags & MEDIA_PAD_FL_INPUT)?"INPUT":"OUTPUT");
                links.pads++;
            }
            printf("\n");
            /* display links now */
            for(i = 0;i< entity[index].links; i++) {
                printf("[%x:%x]-------------->[%x:%x]",links.links->source.entity,
                        links.links->source.index,links.links->sink.entity,links.links->sink.index);
                if(links.links->flags & MEDIA_LNK_FL_ENABLED) printf("\tACTIVE\n");
                else                                          printf("\tINACTIVE \n");
                links.links++;
            }
            printf("\n");
        }
    }

    /* 6. enable 'teplovisor-->ccdc' link */
    memset(&link, 0, sizeof(link));

    link.flags |=  MEDIA_LNK_FL_ENABLED;
    link.source.entity = E_TEPLOVISOR;
    link.source.index = P_TEPLOVISOR;
    link.source.flags = MEDIA_PAD_FL_OUTPUT;

    link.sink.entity = E_CCDC;
    link.sink.index = P_CCDC_SINK;
    link.sink.flags = MEDIA_PAD_FL_INPUT;

    ret = ioctl(media_fd, MEDIA_IOC_SETUP_LINK, &link);
    if (ret) {
        printf("failed to enable link between teplovisor and ccdc\n");
        goto cleanup;
    }

    /* 7.enable 'ccdc-->prv' link */
    printf("7. ENABLEing link [CCDC]----------->[PRV]\n");
    memset(&link, 0, sizeof(link));

    link.flags |=  MEDIA_LNK_FL_ENABLED;
    link.source.entity = E_CCDC;
    link.source.index = P_CCDC_SOURCE;
    link.source.flags = MEDIA_PAD_FL_OUTPUT;

    link.sink.entity = E_PRV;
    link.sink.index = P_PRV_SINK;
    link.sink.flags = MEDIA_PAD_FL_INPUT;

    ret = ioctl(media_fd, MEDIA_IOC_SETUP_LINK, &link);
    if(ret) {
        printf("failed to enable link between ccdc and previewer\n");
        goto cleanup;
    }
    else printf("[ccdc]----------->[prv]\tENABLED\n");

    /* 8.enable 'prv-->rsz' link */
    printf("7. ENABLEing link [prv]----------->[rsz]\n");
    memset(&link, 0, sizeof(link));

    link.flags |=  MEDIA_LNK_FL_ENABLED;
    link.source.entity = E_PRV;
    link.source.index = P_PRV_SOURCE;
    link.source.flags = MEDIA_PAD_FL_OUTPUT;

    link.sink.entity = E_RSZ;
    link.sink.index = P_RSZ_SINK;
    link.sink.flags = MEDIA_PAD_FL_INPUT;

    ret = ioctl(media_fd, MEDIA_IOC_SETUP_LINK, &link);
    if(ret) {
        printf("failed to enable link between prv and rsz\n");
        goto cleanup;
    }
    else printf("[prv]----------->[rsz]\tENABLED\n");

    /* 9. enable 'rsz->memory' link */
    printf("8. ENABLEing link [rsz]----------->[video_node]\n");
    memset(&link, 0, sizeof(link));

    link.flags |=  MEDIA_LNK_FL_ENABLED;
    link.source.entity = E_RSZ;
    link.source.index = P_RSZ_SOURCE;
    link.source.flags = MEDIA_PAD_FL_OUTPUT;

    link.sink.entity = E_VIDEO;
    link.sink.index = P_VIDEO;
    link.sink.flags = MEDIA_PAD_FL_INPUT;

    ret = ioctl(media_fd, MEDIA_IOC_SETUP_LINK, &link);
    if(ret) {
        printf("failed to enable link between rsz and video node\n");
        goto cleanup;
    }
    else printf("[rsz]----------->[video_node]\t ENABLED\n");

    /* 13.open rsz device */
    rsz_fd = open("/dev/v4l-subdev3", O_RDWR);
    if(prv_fd == -1) {
        printf("failed to open %s\n", "/dev/v4l-subdev3");
        goto cleanup;
    }

    /* 14. set default configuration in rsz */
    //rsz_chan_config.oper_mode = IMP_MODE_CONTINUOUS;
    rsz_chan_config.chain  = 1;
    rsz_chan_config.len = 0;
    rsz_chan_config.config = NULL;
    if (ioctl(rsz_fd, RSZ_S_CONFIG, &rsz_chan_config) < 0) {
        perror("failed to set default configuration in resizer\n");
        goto cleanup;
    }

    /* 15. get configuration from rsz */
    bzero(&rsz_cont_config, sizeof(struct rsz_continuous_config));
    //rsz_chan_config.oper_mode = IMP_MODE_CONTINUOUS;
    rsz_chan_config.chain = 1;
    rsz_chan_config.len = sizeof(struct rsz_continuous_config);
    rsz_chan_config.config = &rsz_cont_config;

    if (ioctl(rsz_fd, RSZ_G_CONFIG, &rsz_chan_config) < 0) {
        perror("failed to get resizer channel configuration\n");
        goto cleanup;
    }

    /* 16. set configuration in rsz */
    rsz_cont_config.output1.enable = 1;
    rsz_cont_config.output2.enable = 0;
    //rsz_chan_config.oper_mode = IMP_MODE_CONTINUOUS;
    rsz_chan_config.chain = 1;
    rsz_chan_config.len = sizeof(struct rsz_continuous_config);
    rsz_chan_config.config = &rsz_cont_config;
    if (ioctl(rsz_fd, RSZ_S_CONFIG, &rsz_chan_config) < 0) {
        perror("failed to set configuration in resizer\n");
        goto cleanup;
    }

    /* 20.open prv device */
    prv_fd = open("/dev/v4l-subdev2", O_RDWR);
    if(prv_fd == -1) {
        printf("failed to open %s\n", "/dev/v4l-subdev2");
        goto cleanup;
    }

    /* 21. set default configuration in PRV */
    //prev_chan_config.oper_mode = IMP_MODE_CONTINUOUS;
    prev_chan_config.len = 0;
    prev_chan_config.config = NULL; /* to set defaults in driver */
    if (ioctl(prv_fd, PREV_S_CONFIG, &prev_chan_config) < 0) {
        perror("failed to set default configuration on prv\n");
        goto cleanup;
    }

    /* 22. get configuration from prv */
    //prev_chan_config.oper_mode = IMP_MODE_CONTINUOUS;
    prev_chan_config.len = sizeof(struct prev_continuous_config);
    prev_chan_config.config = &prev_cont_config;
    if (ioctl(prv_fd, PREV_G_CONFIG, &prev_chan_config) < 0) {
        perror("failed to get default configuration from prv\n");
        goto cleanup;
    }

    /* 23. set configuration in prv */
    //prev_chan_config.oper_mode = IMP_MODE_CONTINUOUS;
    prev_chan_config.len = sizeof(struct prev_continuous_config);
    prev_chan_config.config = &prev_cont_config;

    if (ioctl(prv_fd, PREV_S_CONFIG, &prev_chan_config) < 0) {
        perror("failed to set configuration on prv\n");
        goto cleanup;
    }

    /* 24. setting default prv-params */
    cap.index=0;
    for (;;) {
        ret = ioctl(prv_fd , PREV_ENUM_CAP, &cap);
        if (ret < 0) break;
        strcpy(mod_param.version, cap.version);
        mod_param.module_id = cap.module_id;
        printf("setting default for prv-module %s\n", cap.module_name);
        mod_param.param = NULL;
        if (ioctl(prv_fd, PREV_S_PARAM, &mod_param) < 0) {
            printf("error in Setting %s prv-params from driver\n", cap.module_name);
            goto cleanup;
        }
        cap.index++;
    }

    /* 27.open capture device */
    if ((capt_fd = open("/dev/video4", O_RDWR | O_NONBLOCK, 0)) <= -1) {
        printf("failed to open %s \n", "/dev/video4");
        goto cleanup;
    }

    /* 28.enumerate inputs supported by capture via teplovisor[wh s active through mc] */
    printf("12.enumerating INPUTS\n");
    bzero(&input, sizeof(struct v4l2_input));
    input.type = V4L2_INPUT_TYPE_CAMERA;
    input.index = 0;
    index = 0;
    for (;;) {
        ret = ioctl(capt_fd, VIDIOC_ENUMINPUT, &input);
        if(ret != 0) break;
        printf("[%x].%s\n", index, input.name);
        sleep(1);
        bzero(&input, sizeof(struct v4l2_input));
        index++;
        input.index = index;
    }

    /* 29.setting COMPOSITE input */
    printf("13.setting FPGA input. . .\n");
    bzero(&input, sizeof(struct v4l2_input));
    input.type = V4L2_INPUT_TYPE_CAMERA;
    input.index = 0;
    if (ioctl (capt_fd, VIDIOC_S_INPUT, &input.index) < 0) {
        printf("failed to set FPGA with capture device\n");
        goto cleanup;
    }

    /* 14.setting NTSC std */
    bzero(&cur_std, sizeof(cur_std));
    cur_std = V4L2_STD_NTSC;
    if (ioctl(capt_fd, VIDIOC_S_STD, &cur_std) < 0) {
        printf("failed to set NTSC std on capture device\n");
        goto cleanup;
    }

    /* 10. set format on pad of teplovisor */
    teplovisor_fd = open("/dev/v4l-subdev0", O_RDWR);
    if(teplovisor_fd < 0) {
        perror("failed to open /dev/v4l-subdev0");
        goto cleanup;
    }

    printf("8. setting format on pad of teplovisor entity. . .\n");
    memset(&fmt, 0, sizeof(fmt));

    fmt.pad = P_TEPLOVISOR;
    fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    fmt.format.code = CODE;
    fmt.format.width = WIDTH_IN;
    fmt.format.height = HEIGHT_IN;
    fmt.format.colorspace = V4L2_COLORSPACE_SMPTE170M;
    fmt.format.field = V4L2_FIELD_INTERLACED;

    ret = ioctl(teplovisor_fd, VIDIOC_SUBDEV_S_FMT, &fmt);
    if (ret) {
        printf("failed to set format on pad %x\n", fmt.pad);
        goto cleanup;
    }

    /* 11. set format on sink-pad of ccdc */
    ccdc_fd = open("/dev/v4l-subdev1", O_RDWR);
    if(ccdc_fd < 0) {
        printf("failed to open %s\n", "/dev/v4l-subdev1");
        goto cleanup;
    }

    printf("9. setting format on sink-pad of ccdc entity. . .\n");
    memset(&fmt, 0, sizeof(fmt));

    fmt.pad = P_CCDC_SINK;
    fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    fmt.format.code = CODE;
    fmt.format.width = WIDTH_IN;
    fmt.format.height = HEIGHT_IN;
    fmt.format.colorspace = V4L2_COLORSPACE_SMPTE170M;
    fmt.format.field = V4L2_FIELD_INTERLACED;

    ret = ioctl(ccdc_fd, VIDIOC_SUBDEV_S_FMT, &fmt);
    if (ret) {
        printf("failed to set format on pad %x\n", fmt.pad);
        goto cleanup;
    }

    /* 12. set format on OF-pad of ccdc */
    memset(&fmt, 0, sizeof(fmt));

    fmt.pad = P_CCDC_SOURCE;
    fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    fmt.format.code = CODE;
    fmt.format.width = WIDTH_IN;
    fmt.format.height = HEIGHT_IN;
    fmt.format.colorspace = V4L2_COLORSPACE_SMPTE170M;
    fmt.format.field = V4L2_FIELD_INTERLACED;

    ret = ioctl(ccdc_fd, VIDIOC_SUBDEV_S_FMT, &fmt);
    if (ret) {
        printf("failed to set format on pad %x\n", fmt.pad);
        goto cleanup;
    }

    /* 25. set format on sink-pad of prv */
    memset(&fmt, 0, sizeof(fmt));

    fmt.pad = P_PRV_SINK;
    fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    fmt.format.code = CODE;
    fmt.format.width = WIDTH_IN;
    fmt.format.height = HEIGHT_IN;
    fmt.format.colorspace = V4L2_COLORSPACE_SMPTE170M;
    fmt.format.field = V4L2_FIELD_INTERLACED;

    ret = ioctl(prv_fd, VIDIOC_SUBDEV_S_FMT, &fmt);
    if (ret) {
        printf("failed to set format on pad %x\n", fmt.pad);
        goto cleanup;
    }

    /* 26. set format on source-pad of prv */
    printf("10. setting format on source-pad of prv entity. . . \n");
    memset(&fmt, 0, sizeof(fmt));

    fmt.pad = P_PRV_SOURCE;
    fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    fmt.format.code = CODE;
    fmt.format.width = WIDTH_IN;
    fmt.format.height = HEIGHT_IN;
    fmt.format.colorspace = V4L2_COLORSPACE_SMPTE170M;
    fmt.format.field = V4L2_FIELD_INTERLACED;

    ret = ioctl(prv_fd, VIDIOC_SUBDEV_S_FMT, &fmt);
    if (ret) {
        printf("failed to set format on pad %x\n", fmt.pad);
        goto cleanup;
    }

    /* 18. set format on sink-pad of rsz */
    memset(&fmt, 0, sizeof(fmt));

    fmt.pad = P_RSZ_SINK;
    fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    fmt.format.code = CODE;
    fmt.format.width = WIDTH_IN;
    fmt.format.height = HEIGHT_IN;
    fmt.format.colorspace = V4L2_COLORSPACE_SMPTE170M;
    fmt.format.field = V4L2_FIELD_INTERLACED;

    ret = ioctl(rsz_fd, VIDIOC_SUBDEV_S_FMT, &fmt);
    if (ret) {
        printf("failed to set format on pad %x\n", fmt.pad);
        goto cleanup;
    }

    /* 19. set format on source-pad of rsz */
    memset(&fmt, 0, sizeof(fmt));

    fmt.pad = P_RSZ_SOURCE;
    fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    fmt.format.code = V4L2_MBUS_FMT_NV12_1X20;
    fmt.format.width = WIDTH_OUT;
    fmt.format.height = HEIGHT_OUT;
    fmt.format.colorspace = V4L2_COLORSPACE_SMPTE170M;
    fmt.format.field = V4L2_FIELD_INTERLACED;

    ret = ioctl(rsz_fd, VIDIOC_SUBDEV_S_FMT, &fmt);
    if (ret) {
        printf("failed to set format on pad %x\n", fmt.pad);
        goto cleanup;
    }

    /* 31.setting format */
    CLEAR(v4l2_fmt);
    v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_fmt.fmt.pix.width = WIDTH_OUT;
    v4l2_fmt.fmt.pix.height = HEIGHT_OUT;
    v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
    v4l2_fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (ioctl(capt_fd, VIDIOC_S_FMT, &v4l2_fmt) < 0) {
        printf("failed to set format on captute device \n");
        goto cleanup;
    }

    /* 32. call G_FMT for knowing picth */
    if (ioctl(capt_fd, VIDIOC_G_FMT, &v4l2_fmt) < 0) {
        printf("failed to get format from captute device \n");
        goto cleanup;
    } else {
        capture_pitch = v4l2_fmt.fmt.pix.bytesperline;
        printf("capture_pitch: %d, width: %d, height: %d\n", capture_pitch, v4l2_fmt.fmt.pix.height, v4l2_fmt.fmt.pix.height);
    }

    /* 33. make sure 3 buffers are supported for streaming */
    CLEAR(req);
    req.count = 3;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;

    if (ioctl(capt_fd, VIDIOC_REQBUFS, &req) < 0) {
        printf("call to VIDIOC_REQBUFS failed\n");
        goto cleanup;
    }

    if (req.count != 3) {
        printf("3 buffers not supported by capture device");
        goto cleanup;
    }

    /* 34. queue the buffers */
    for (i = 0; i < 3; i++) {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_USERPTR;
        buf.index = i;
        buf.length = buf_size;
        buf.m.userptr = (unsigned long)capture_buffers[i].user_addr;
        if (ioctl(capt_fd, VIDIOC_QBUF, &buf) < 0) {
            printf("call to VIDIOC_QBUF failed\n");
            goto cleanup;
        }
    }

    /* 35.start streaming */
    CLEAR(type);
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(capt_fd, VIDIOC_STREAMON, &type) < 0) {
        printf("failed to start streaming on capture device\n");
        goto cleanup;
    }

    /* 35.open file in which frames should be saved */
    file = fopen("teplovisor_ccdc_prv_rsz.yuv", "wa+");
        if (file == NULL) {
        perror("could not open the file to write");
        goto cleanup;
    }

    /* 36.get 20 frames from capture device and store in a file */
    frame_count = 0;
    while(frame_count != 5) {
        CLEAR(cap_buf);
        cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cap_buf.memory = V4L2_MEMORY_USERPTR;
try_again:
        ret = ioctl(capt_fd, VIDIOC_DQBUF, &cap_buf);
        if (ret < 0) {
            if (errno == EAGAIN) goto try_again;
            printf("failed to DQ buffer from capture device\n");
            goto cleanup;
        }
        temp = cap_buf.m.userptr;
        source = (char *)temp;

        /* copy frame to a file */
        for (i = 0; i < (HEIGHT_OUT + HEIGHT_OUT/2); i++) {
            fwrite(source, 1 , capture_pitch, file);
            source += capture_pitch;
        }

        /* Q the buffer for capture, again */
        ret = ioctl(capt_fd, VIDIOC_QBUF, &cap_buf);
        if (ret < 0) {
            printf("failed to Q buffer onto capture device\n");
            goto streamoff;
        }

        if (frame_count == 0)
            prev_ts = (cap_buf.timestamp.tv_sec*1000000) + cap_buf.timestamp.tv_usec;
        else {
            curr_ts = (cap_buf.timestamp.tv_sec*1000000) + cap_buf.timestamp.tv_usec;
            fp_period = curr_ts - prev_ts;
            if (frame_count == 1)
                fp_period_max = fp_period_min = fp_period_average = fp_period;
            else {
                /* calculate jitters and average */
                if (fp_period > fp_period_max) fp_period_max = fp_period;
                if (fp_period < fp_period_min) fp_period_min = fp_period;

                fp_period_average = ((fp_period_average * frame_count) + fp_period)/(frame_count + 1);
            }
            prev_ts = curr_ts;
        }
        frame_count++;
        printf("frame:%5u, ", frame_count);
        printf("buf.timestamp:%lu:%lu\n", cap_buf.timestamp.tv_sec, cap_buf.timestamp.tv_usec);
    }

streamoff:
    /* 37. do stream off */
    CLEAR(type);
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl(capt_fd, VIDIOC_STREAMOFF, &type)) {
        printf("failed to stop streaming on capture device\n");
        goto cleanup;
    }

cleanup:
    /* 24. de-enable all the links which are active right now */
    for (index = 0; index < entities_count; index++) {
        links.entity = entity[index].id;
        links.pads = malloc(sizeof( struct media_pad_desc) * entity[index].pads);
        links.links = malloc(sizeof(struct media_link_desc) * entity[index].links);
        ret = ioctl(media_fd, MEDIA_IOC_ENUM_LINKS, &links);
        if (ret < 0) {
            if (errno == EINVAL) break;
        }
        else {
            for(i = 0;i< entity[index].links; i++) {
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
                    if (ret) printf("failed to de-enable link \n");
                }
                links.links++;
            }
        }
    }

    /* 42.close all the file descriptors */
    if (rsz_fd) close(rsz_fd);
    if (prv_fd) close(prv_fd);
    if (ccdc_fd) close(ccdc_fd);
    if (teplovisor_fd) close(teplovisor_fd);
    if (capt_fd) close(capt_fd);
    if (media_fd) close(media_fd);
    if (file) fclose(file);

    return ret;
}
