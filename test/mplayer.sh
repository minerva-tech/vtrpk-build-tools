#!/bin/sh

mplayer -demuxer rawvideo -rawvideo w=320:h=256:format=uyvy captured.yuv -loop 0
