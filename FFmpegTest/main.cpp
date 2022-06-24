//
// using ffmpeg version 5.0.1
// the input .h264 file is extracted from .mp4 file using ffmpeg command line tool
// ffmpeg -i input.mp4 input_h264.h264
//


#include "core_functions.h"

int main(int argc, const char * argv[]) {
    // .h264 -> .yuv
    // parameters: inFilePath, outFilePath
    h264_to_yuv("input_h264.h264", "decode_from_h264.yuv");
    
    // .yuv -> .yuv
    // parameters: inFilePath, outFilePath, inWidth, inHeight, outWidth, outHeight
    rescale_yuv("decode_from_h264.yuv", "rescale_from_yuv.yuv", 1000, 418, 500, 250);
    
    // .yuv -> .h264
    // parameters: inFilePath, outFilePath, inWidth, inHeight
    yuv_to_h264("decode_from_h264.yuv", "encode_from_yuv.h264", 1000, 418);
    yuv_to_h264("rescale_from_yuv.yuv", "encode_from_rescaled_yuv.h264", 500, 250);
    
    return 0;
}

