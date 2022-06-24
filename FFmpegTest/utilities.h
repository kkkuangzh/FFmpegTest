#ifndef utilities_h
#define utilities_h

#include <iostream>
using namespace std;

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/avutil.h>
}

#define INBUF_SIZE 4096


int write_frame_to_yuv(AVFrame *frame, int height, int width, FILE *outFile) {
    size_t size = height * width * 3 / 2;
    char* buf = new char [size];
    memset(buf, 0, size);
    int a = 0, i;
    // 写入Y平面数据
    for (i = 0; i < height; i++) {
        memcpy(buf + a, frame->data[0] + i * frame->linesize[0], width);
        a += width;
    }
    // 写入U平面数据
    for (i = 0; i < height / 2; i++) {
        memcpy(buf + a, frame->data[1] + i * frame->linesize[1], width / 2);
        a += width / 2;
    }
    // 写入V平面数据
    for (i = 0; i < height / 2; i++) {
        memcpy(buf + a, frame->data[2] + i * frame->linesize[2], width / 2);
        a += width / 2;
    }
    fwrite(buf, 1, size, outFile);
    delete[] buf;
    
    return 0;
}


int read_frame_from_yuv(AVFrame &frame, int height, int width, char *buffer) {
    int i = 0;
    for (i = 0; i < height; i++) {
        memcpy(frame.data[0] + i * frame.linesize[0], buffer + width*i, width);
    }
    for (i = 0; i < height / 2; i++) {
        memcpy(frame.data[1] + i * frame.linesize[1], buffer + height*width + width/2*i, width/2);
    }
    for (i = 0; i < height / 2; i++) {
        memcpy(frame.data[2] + i * frame.linesize[2], buffer + height*width*5/4 + width/2*i, width/2);
    }
    return 0;
}


static int decode(AVCodecContext *decoder_context, AVFrame *frame, AVPacket *packet, FILE *outFile) {
    int ret = avcodec_send_packet(decoder_context, packet); //supply raw packet data as input to a decoder
    if (ret < 0) {
        cout<<"avcodec_send_packet error"<<endl;
        return ret;
    }
    while (ret >= 0) {
        ret = avcodec_receive_frame(decoder_context, frame); //return decoded output data from a decoder
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        } else if (ret < 0) {
            cout<<"avcodec_receive_frame error";
            return ret;
        }
        write_frame_to_yuv(frame, decoder_context->height, decoder_context->width, outFile);
    }
    return 0;
}


static int encode(AVFormatContext *format_context, AVCodecContext *encoder_context, AVFrame *frame, AVPacket *packet, AVStream *stream) {
    int ret = avcodec_send_frame(encoder_context, frame); // supply raw frame to the encoder
    if (ret < 0) {
        cout<<"avcodec_send_frame error"<<endl;
        return -1;
    }
    while (ret >= 0) {
        ret = avcodec_receive_packet(encoder_context, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return -1;
        } else if (ret < 0) {
            cout<<"avcodec_receive_packet error";
            return ret;
        }
        packet->stream_index = stream->index;
        packet->pos = -1;
        ret = av_interleaved_write_frame(format_context, packet); // write a packet to an output media
        if (ret < 0) {
            cout<<"av_interleaved_write_frame error"<<endl;
            return ret;
        }
        av_packet_unref(packet); // wipe a packet
    }
    return 0;
}


#endif /* utilities_h */

