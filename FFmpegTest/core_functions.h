#ifndef core_functions_h
#define core_functions_h

#include <iostream>
#include "utilities.h"
using namespace std;

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/avutil.h>
}

#define INBUF_SIZE 4096


int h264_to_yuv(const char *inFilePath, const char *outFilePath) {
    // decoder setting
    const AVCodec *decoder = avcodec_find_decoder_by_name("h264");
    if (!decoder) {
        cout<<"avcodec_find_decoder_by_name error";
        return -1;
    }
    AVCodecContext *decoder_context = avcodec_alloc_context3(decoder);
    if (!decoder_context) {
        cout<<"avcodec_alloc_context3 error";
        return -1;
    }
    AVCodecParserContext *parser = av_parser_init(decoder->id); //initialize the AVCodecContext to use the given AVCodec
    if (avcodec_open2(decoder_context, decoder, nullptr) < 0){
        cout<<"avcodec_open2 error";
        return -1;
    }
    
    
    // packet and frame
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    
    
    // open files
    FILE *inFile = fopen(inFilePath, "rb");
    if (!inFile) {
        cout<<"Couldn't open input file"<<endl;
        return -1;
    }
    FILE *outFile = fopen(outFilePath, "wb");
    if (!outFile) {
        cout<<"Couldn't open output file"<<endl;
        return -1;
    }
    
    
    // loop input file to get packets and decode
    // 加上AV_INPUT_BUFFER_PADDING_SIZE是为了防止某些优化过的reader一次性读取过多导致越界（参考了FFmpeg示例代码）
    char inBuffer[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    char *inData = nullptr;
    while (!feof(inFile)) {
        // read data from input file
        size_t data_size = fread(inBuffer, 1, INBUF_SIZE, inFile);
        if (!data_size) break;
        inData = inBuffer;
        while (data_size > 0) {
            // parse a packet
            int ret = av_parser_parse2(parser, decoder_context, &packet->data, &packet->size, (const uint8_t *)inData, (int)data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (ret < 0) {
                cout<<"av_parser_parse2 error"<<endl;
                return -1;
            }
            
            inData += ret;
            data_size -= ret;
            
            if (packet->size)
                decode(decoder_context, frame, packet, outFile);
        }
    }
    
    decode(decoder_context, frame, nullptr, outFile);
    fclose(inFile);
    av_parser_close(parser);
    avcodec_free_context(&decoder_context);
    av_frame_free(&frame);
    av_packet_free(&packet);
    
    return 0;
}


int yuv_to_h264(const char *inFilePath, const char *outFilePath, int width, int height) {
    int ret;
    
    // encoder setting
    const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    AVCodecContext *encoder_context = avcodec_alloc_context3(encoder);
    encoder_context->codec_type = AVMEDIA_TYPE_VIDEO;
    encoder_context->codec_id = AV_CODEC_ID_H264;
    encoder_context->bit_rate = 800000;
    encoder_context->width = width;
    encoder_context->height = height;
    
    encoder_context->time_base = (AVRational) {1, 25}; // frames per second
    encoder_context->framerate = (AVRational) {25, 1};
    encoder_context->pix_fmt = AV_PIX_FMT_YUV420P;
    
    encoder_context->gop_size = 10; // number of pictures in a group of pictures
    encoder_context->max_b_frames = 1; // max number of b frames
    
    if (encoder_context->codec_id == AV_CODEC_ID_H264) {
        AVDictionary *param = 0;
        av_dict_set(&param, "preset", "slow", 0);   //压缩速度慢，保证视频质量
        // av_dict_set(&param, "tune", "zerolatency", 0);   //零延迟
    }
    avcodec_open2(encoder_context, encoder, nullptr);
    

    // packet and frame
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    frame->width = width;
    frame->height = height;
    frame->format = AV_PIX_FMT_YUV420P;
    av_frame_get_buffer(frame, 0); // 为frame分配缓冲区
    
    
    // input file
    FILE *inFile = fopen(inFilePath, "rb");
    if (!inFile) {
        cout<<"Couldn't open input file"<<endl;
        return -1;
    }
    
    // output stream
    AVFormatContext *output_context = NULL;
    ret = avformat_alloc_output_context2(&output_context, NULL, NULL, outFilePath);
    if (ret < 0) {
        cout<<"avformat_alloc_output_context2 error"<<endl;
        return -1;
    }
    const AVOutputFormat *pFmt = av_guess_format(NULL, outFilePath, NULL);
    output_context->oformat = pFmt;

    ret = avio_open(&output_context->pb, outFilePath, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        cout<<"avio_open error"<<endl;
        return ret;
    }
    AVStream *stream = avformat_new_stream(output_context, encoder);
    if (!stream) {
        cout<<"avformat_new_stream error";
        return ret;
    }
    
    // set AVFormatContext paras, or the avformat_write_header returns -22.
    output_context->streams[0]->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    output_context->streams[0]->codecpar->width = width;
    output_context->streams[0]->codecpar->height = height;
    output_context->streams[0]->codecpar->format = AV_PIX_FMT_YUV420P;
    
    ret = avformat_write_header(output_context, NULL);
    if (ret < 0) {
        cout<<"avformat_write_header error"<<endl;
        cout<<ret<<endl;
        return ret;
    }
    
    // av_dump_format(output_context, 0, outFile, 1);
    int pts_i = 0;
    int size = height * width * 3 / 2;
    char *yuv_buffer = new char [size];
    while (!feof(inFile)) {
        size_t data_size = fread(yuv_buffer, sizeof(char), size, inFile);
        if (data_size < size) break;
        read_frame_from_yuv(*frame, height, width, yuv_buffer);
        frame->pts = pts_i++;
        encode(output_context, encoder_context, frame, packet, stream);
    }
    
    encode(output_context, encoder_context, NULL, packet, stream);
    ret = av_write_trailer(output_context);
    if (ret < 0) {
        cout<<"av_write_trailer error"<<endl;
    }
    
    fclose(inFile);
    avformat_free_context(output_context);
    avcodec_free_context(&encoder_context);
    av_frame_free(&frame);
    av_packet_free(&packet);
    
    return 0;
}


int rescale_yuv(const char *inFilePath, const char *outFilePath, int width, int height, int out_width, int out_height) {
    // open files
    FILE *inFile = fopen(inFilePath, "rb");
    if (!inFile) {
        cout<<"Couldn't open input file"<<endl;
        return -1;
    }
    FILE *outFile = fopen(outFilePath, "wb");
    if (!outFile) {
        cout<<"Couldn't open output file"<<endl;
        return -1;
    }
    
    
    // frame setting
    AVFrame *inFrame = av_frame_alloc();
    inFrame->format = AV_PIX_FMT_YUV420P;
    inFrame->width = width;
    inFrame->height = height;
    av_frame_get_buffer(inFrame, 0);
    
    AVFrame *outFrame = av_frame_alloc();
    outFrame->format = inFrame->format;
    outFrame->width = out_width;
    outFrame->height = out_height;
    av_frame_get_buffer(outFrame, 0);
    
    
    // set up scaler
    struct SwsContext *sws_context;
    sws_context = sws_getContext(width, height, AV_PIX_FMT_YUV420P, out_width, out_height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    
    size_t inSize = width * height * 3 / 2;
    char *inBuffer = new char [inSize];
    
    while (!feof(inFile)) {
        size_t data_size = fread(inBuffer, 1, inSize, inFile);
        if (data_size < inSize) break;
        
        read_frame_from_yuv(*inFrame, height, width, inBuffer);
        
        // frame rescale
        sws_scale(sws_context, inFrame->data, inFrame->linesize, 0, height, outFrame->data, outFrame->linesize);

        write_frame_to_yuv(outFrame, out_height, out_width, outFile);
    }
    
    fclose(inFile);
    fclose(outFile);
    sws_freeContext(sws_context);
    return 0;
}


#endif /* core_functions_h */
