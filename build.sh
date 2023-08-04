#!/bin/bash



gcc reminiscence.c -o reminiscence -g -I./headers $(pkg-config --libs libavformat) $(pkg-config --libs libavdevice) $(pkg-config --libs libavcodec) $(pkg-config --libs libavutil)