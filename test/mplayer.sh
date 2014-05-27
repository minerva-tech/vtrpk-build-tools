#!/bin/sh

mplayer -demuxer rawvideo -rawvideo w=384:h=144:format=nv12 captured.yuv -loop 0
