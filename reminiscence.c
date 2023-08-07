#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>


const int FPS = 60;
const char *output_name = "realout.mp4";

int main(void)
{

    avdevice_register_all();

    AVFormatContext *pFormatContextIn = avformat_alloc_context();

    const AVInputFormat *pAVInputFormat = av_find_input_format("x11grab");
    assert(pFormatContextIn != NULL);
    assert(avformat_open_input(&pFormatContextIn, ":0.0+0,0", pAVInputFormat, NULL) == 0 && "Could not open input stream");


    int video_index = -1;
    for (int i = 0; i < pFormatContextIn->nb_streams; i++)
    {
        if (pFormatContextIn->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_index = i;
            break;
        }
    }

    assert(video_index >= 0 && "video_index was less than 0");

    const AVCodec *pAVCodec = avcodec_find_decoder(pFormatContextIn->streams[video_index]->codecpar->codec_id);
    assert(pAVCodec != NULL && "pAVCodec was NULL");

    AVCodecContext *pCodecContextIn = avcodec_alloc_context3(pAVCodec);
    assert(avcodec_parameters_to_context(pCodecContextIn, pFormatContextIn->streams[video_index]->codecpar) >= 0 && "Failed to create AVCodecContext");  


    assert(avcodec_open2(pCodecContextIn, pAVCodec, NULL) == 0);
    av_dump_format(pFormatContextIn, video_index, ":0.0", 0);


    // allocate video frame
    AVFrame *pFrame = av_frame_alloc();
    assert(pFrame != NULL);

    // int err = avformat_write_header(pFormatContextOut, NULL);
    // if (err == AVSTREAM_INIT_IN_WRITE_HEADER || AVSTREAM_INIT_IN_INIT_OUTPUT)
    // {
    //     printf("Wrote header\n");
    // }
    // else
    // {
    //     printf("Failed to write header\n");
    //     return 1;
    // }

    // printf("Intentional exit");
    // exit(1);

    //Output stuff
    const AVOutputFormat *pAVOutputFormat = av_guess_format(NULL, output_name, NULL);
    assert(pAVOutputFormat != NULL);

    AVIOContext *pAVIOOutputContext;
    /** Open the output file to write to it. */
    int ret = avio_open(&pAVIOOutputContext, output_name, AVIO_FLAG_WRITE);
    assert(ret >= 0);

    AVFormatContext *pAVOutputFormatContext = avformat_alloc_context();
    assert(pAVOutputFormatContext != NULL);

    pAVOutputFormatContext->pb = pAVIOOutputContext;
    pAVOutputFormatContext->oformat = av_guess_format(NULL, output_name, NULL);
    assert(pAVOutputFormatContext->oformat != NULL);

    AVStream *pAVOutputStream = avformat_new_stream(pAVOutputFormatContext, NULL);
    assert(pAVOutputStream != NULL);

    const AVCodec *pCodecOutput = avcodec_find_encoder(AV_CODEC_ID_H264);
    assert(pCodecOutput != NULL);

    AVCodecContext *pCodecContextOut = avcodec_alloc_context3(pCodecOutput);
    assert(pCodecOutput != NULL);

    pCodecContextOut->bit_rate = pCodecContextIn->bit_rate;
    pCodecContextOut->width = pCodecContextIn->width;
    pCodecContextOut->height = pCodecContextIn->height;
    pCodecContextOut->time_base = (AVRational){1, 25};
    pCodecContextOut->framerate = (AVRational){25, 1};
    pCodecContextOut->gop_size = 10;
    pCodecContextOut->max_b_frames = 1;
    pCodecContextOut->pix_fmt = AV_PIX_FMT_YUV420P;
    pCodecContextOut->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    ret = avcodec_parameters_from_context(pAVOutputStream->codecpar, pCodecContextOut);
    assert(ret >= 0);

    ret = avcodec_open2(pCodecContextOut, pCodecOutput, NULL);
    assert(ret == 0);

    ret = avformat_write_header(pAVOutputFormatContext, NULL);
    if (ret == AVSTREAM_INIT_IN_WRITE_HEADER || ret == AVSTREAM_INIT_IN_INIT_OUTPUT)
    {
        printf("Wrote header\n");
    }
    else
    {
        printf("Failed to write header\n");
        return 1;
    }

    size_t i = 0;
    const size_t frame_limit = 100;
    AVPacket *pAVPacketIn = av_packet_alloc();
    AVPacket *pAVPacketOut = av_packet_alloc();
    

    while(av_read_frame(pFormatContextIn, pAVPacketIn) >= 0 && i++ < frame_limit)
    {
        if (pAVPacketIn->stream_index == video_index) 
        {
            // Send the packet to the input decoder
            int ret = avcodec_send_packet(pCodecContextIn, pAVPacketIn);
            if (ret < 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
            {
                // We now have no frames left in the packet
                printf("avcodec_send_packet: %d\n", ret);
                break;
            }

            while (ret  >= 0) 
            {
                // Receive the now raw data from the decoder 
                ret = avcodec_receive_frame(pCodecContextIn, pFrame);
                if (ret == AVERROR(EAGAIN))
                    break;
                else if (ret < 0)
                {
                    printf("avcodec_receive_frame: %d\n", ret);
                    return 1;
                }

                av_frame_make_writable(pFrame);

                // Send raw data to the output encoder
                ret = avcodec_send_frame(pCodecContextOut, pFrame);
                if (ret <= 0)
                {
                    printf("avcodec_send_frame: %d = %s\n", ret, av_err2str(ret));
                    return 1;
                }

                // Receive the encoded data from the encoder
                // ret = avcodec_receive_packet(pCodecContextOut, pAVPacketOut);
                // assert(ret == 0);

                // Write the data to output file
                
                
                printf("frame: %ld\n", pCodecContextIn->frame_num);
            }
        }
    
        av_packet_unref(pAVPacketIn);
        av_packet_unref(pAVPacketOut);
    }
    ret = av_write_trailer(pAVOutputFormatContext);
    assert(ret == 0);

    
    // while (av_read_frame(pFormatContextIn, pAVPacket) >= 0 && i++ < frame_limit)
    // {
    //     if (pAVPacket->stream_index != video_index)
    //     {
    //         av_packet_unref(pAVPacket);
    //         continue;
    //     }
    //         assert(avcodec_send_packet(pCodecContextIn, pAVPacket) == 0);
                
    //         printf("Packet Counter: %zu/%zu\n", i, frame_limit); fflush(stdout);

    //         size_t ii = 0;
    //         while (avcodec_receive_frame(pCodecContextIn, pFrame) == 0)
    //         {
    //             // Optional scaling here

    //             avcodec_send_frame(pCodecContextOut, pFrame);

    //             int err = avcodec_receive_packet(pCodecContextOut, pAVPacket);
    //             printf("err=%d: %s\n", err, av_err2str(err));

    //             // while (avcodec_receive_packet(pCodecContextOut, pAVPacket) == 0)
    //             // {
    //             //     printf("Wrote frame\n");
    //             //     assert(av_interleaved_write_frame(pFormatContextOut, pAVPacket) == 0);
    //             //     av_packet_unref(pAVPacket);
    //             // }


    //             printf("Frame Counter: %zu\n", ++ii); fflush(stdout);
    //         }

    //     av_packet_unref(pAVPacket);
    // }

    // err = av_write_trailer(pFormatContextOut);
    // if (err != 0)
    // {
    //     printf("Failed to write trailer: %s\n", av_err2str(err));
    // }


    avformat_free_context(pFormatContextIn);
    avformat_free_context(pAVOutputFormatContext);
    // avcodec_free_context(&pCodecContextOut);
    avcodec_free_context(&pCodecContextIn);
    av_frame_free(&pFrame);
    
    
    return 0;
}

// int main(void)
// {

//     // https://github.com/leandromoreira/ffmpeg-libav-tutorial#video---what-you-see
//     // https://github.com/abdullahfarwees/screen-recorder-ffmpeg-cpp/blob/master/src/ScreenRecorder.cpp

//     AVFormatContext *pFormatContext = avformat_alloc_context();
//     avformat_open_input(&pFormatContext, "luddegragas.mp4", NULL, NULL);


    
//     AVPacket *pkt = av_packet_alloc();
//     av_new_packet(pkt, 200000);

//     size_t vframe = 0;
//     size_t aframe = 0;
//     while (av_read_frame(pFormatContext, pkt) == 0)
//     {
//         AVStream *stream = pFormatContext->streams[pkt->stream_index];
//         switch (stream->codecpar->codec_type)
//         {
//             case AVMEDIA_TYPE_VIDEO:
//                 printf("video fsize: %zu\n", pkt->buf->size);
//                 printf("video dur: %ld\n", pkt->duration);
//                 vframe++;
//                 break;
//             case AVMEDIA_TYPE_AUDIO:
//                 printf("audio fsize: %zu\n", pkt->buf->size);
//                 aframe++;
//                 break;
                
//         }
//         av_packet_unref(pkt);
//     }

//     printf("vframe=%zu\n", vframe);
//     printf("aframe=%zu\n", aframe);


//     av_packet_free(&pkt);


    

//     return 0;
// }

