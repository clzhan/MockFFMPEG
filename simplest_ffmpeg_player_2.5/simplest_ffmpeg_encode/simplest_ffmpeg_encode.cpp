// simplest_ffmpeg_encode.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "windows.h"
#include "time.h"

EXTERN_C
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
};

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")

#define MAX_BUFFER_SIZE ( 256 * 1024 )

void SaveBmp(AVCodecContext *CodecContex, AVFrame *Picture, int width, int height);

int _tmain(int argc, _TCHAR* argv[])
{
	int vIndex = -1;
	int aIndex = -1;

	char * sourceFile = "d:\\media\\3d\\Avatar1024.mkv";
	char * destFile	  = "c:\\enc_out.mp4";
	int width, height;
	
	unsigned char * pEncodeBuf = new unsigned char[MAX_BUFFER_SIZE];

	av_register_all();

	AVFormatContext * pInputFormatContext	= NULL;
	AVCodec			* pInputCodec			= NULL;
	AVCodecContext	* pInputCodecContext	= NULL;
	AVPacket		* pInPack				= NULL;

	AVOutputFormat  * pOutputFormat			= NULL;
	AVFormatContext * pOutputFormatContext  = NULL;
	AVCodec			* pOutputCodec			= NULL;
	AVCodecContext  * pOutputCodecContext	= NULL;
	AVStream		* pOutStream			= NULL;
	AVPacket		* pOutPack				= NULL;	
	AVFrame			* pOutFrame				= NULL;

	pOutFrame = av_frame_alloc();
	pInPack = (AVPacket*)av_malloc(sizeof(AVPacket));
	pOutPack = (AVPacket*)av_malloc(sizeof(AVPacket));

	//打开输入文件
	if(avformat_open_input(&pInputFormatContext, sourceFile, NULL, NULL) != 0)
	{
		printf("cannot open the file %s \n", sourceFile);
		goto EXIT;
	}

	//查找媒体流信息
	if(avformat_find_stream_info(pInputFormatContext, NULL) < 0)
	{
		printf("cannot find media stream info.\n");
		goto EXIT;
	}

	//查找视频流
	for( int i = 0; i < pInputFormatContext->nb_streams; i++)
	{
		if(pInputFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			vIndex = i;
			break;
		}
		//else if(pInputFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		//{
		//	aIndex = i;
		//}
	}

	if(vIndex == -1)
	{
		printf("cannot find video stream. \n");
		goto EXIT;
	}

	pInputCodecContext = pInputFormatContext->streams[vIndex]->codec;
	pInputCodec = avcodec_find_decoder(pInputCodecContext->codec_id);
	if(pInputCodec == NULL)
	{
		goto LINK_ERROR;
	}

	if(avcodec_open2(pInputCodecContext, pInputCodec, NULL) != 0)
	{
		goto LINK_ERROR;
	}

	width = pInputCodecContext->width;
	height = pInputCodecContext->height;

	pOutputFormat = av_guess_format(NULL, destFile, NULL);
	if(pOutputFormat == NULL)
	{
		goto LINK_ERROR;
	}

	pOutputFormatContext = avformat_alloc_context();
	if(pOutputFormatContext == NULL)
	{
		goto LINK_ERROR;
	}

	pOutputFormatContext->oformat = pOutputFormat;
	pOutStream = avformat_new_stream(pOutputFormatContext, NULL);
	if(pOutStream == NULL)
	{
		goto LINK_ERROR;
	}
	
	//设定输出流编码参数
	pOutputCodecContext = pOutStream->codec;
	pOutputCodecContext->codec_id = pOutputFormat->video_codec;//AV_CODEC_ID_MPEG4;
	pOutputCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
	pOutputCodecContext->bit_rate	= 128*1024;//98000;
	pOutputCodecContext->width = width;
	pOutputCodecContext->height = height;
	pOutputCodecContext->time_base = pInputCodecContext->time_base;
	pOutputCodecContext->gop_size = pInputCodecContext->gop_size;
	pOutputCodecContext->pix_fmt = pInputCodecContext->pix_fmt;
	pOutputCodecContext->max_b_frames = pInputCodecContext->max_b_frames;	
	pOutputCodecContext->time_base.den = pInputCodecContext->time_base.den;
	pOutputCodecContext->time_base.num = pInputCodecContext->time_base.num;
	pOutStream->r_frame_rate = pInputFormatContext->streams[vIndex]->r_frame_rate;
	
	pOutputCodec = avcodec_find_encoder(pOutputCodecContext->codec_id);
	if(pOutputCodec ==  NULL)
	{
		printf("Cannot find encoder which codec_id = %d.\n", pOutputCodecContext->codec_id);
		goto LINK_ERROR;
	}

	if(avcodec_open2(pOutputCodecContext, pOutputCodec, NULL) < 0)
	{
		printf("avcodec_open2 found error .\n");
		goto LINK_ERROR;
	}

	if(!(pOutputCodecContext->flags & AVFMT_NOFILE))
	{
		if(avio_open(&pOutputFormatContext->pb, destFile, AVIO_FLAG_READ_WRITE) != 0)
		{
			printf("avio_open found error .\n");
			goto LINK_ERROR;
		}
	}

	if(avformat_write_header(pOutputFormatContext, NULL) < 0)
	{
		printf("avformat_write_header found error .\n");
		goto LINK_ERROR;
	}

	int length = 0;
	int nComplete = -1;
	int frame_index = 0;
	int ret = -1;
	int got_packet_ptr = -1;
	while(av_read_frame(pInputFormatContext, pInPack) >= 0)
	{
		if(pInPack->stream_index != vIndex) continue;
		length = avcodec_decode_video2(pInputCodecContext, pOutFrame, &nComplete, pInPack);
		if(nComplete > 0)
		{
			//SaveBmp(pInputCodecContext, pOutFrame, width, height);  

			//成功解码一帧图像
			pOutPack->data = NULL;
			pOutPack->size = 0;
			av_init_packet(pOutPack);
			ret = avcodec_encode_video2(pOutputCodecContext, pOutPack, pOutFrame, &got_packet_ptr);
			if(ret >= 0 && got_packet_ptr)
			{
				pOutPack->flags = pInPack->flags;
				pOutPack->stream_index = pInPack->stream_index;					
				pOutPack->dts = av_rescale_q_rnd(pOutPack->dts,
					pOutputFormatContext->streams[vIndex]->codec->time_base, pOutputFormatContext->streams[vIndex]->time_base,
					(AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
				pOutPack->pts = av_rescale_q_rnd(pOutPack->pts,
					pOutputFormatContext->streams[vIndex]->codec->time_base, pOutputFormatContext->streams[vIndex]->time_base,
					(AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
				//pOutPack->dts = av_rescale_q(pOutPack->dts,
				//	pOutputFormatContext->streams[vIndex]->codec->time_base, pOutputFormatContext->streams[vIndex]->time_base);
				//pOutPack->pts = av_rescale_q(pOutPack->pts,
				//	pOutputFormatContext->streams[vIndex]->codec->time_base, pOutputFormatContext->streams[vIndex]->time_base);
				//pOutPack->duration = av_rescale_q(pOutPack->duration,
				//	pOutputFormatContext->streams[vIndex]->codec->time_base, pOutputFormatContext->streams[vIndex]->time_base
				//	);

				ret = av_write_frame(pOutputFormatContext, pOutPack);
			}

			frame_index++;
			av_free_packet(pOutPack);  
		}
		av_free_packet(pInPack);
	}

	av_write_trailer(pOutputFormatContext);

	for( int i = 0; i < pOutputFormatContext->nb_streams; i++)
	{
		av_free(&pOutputFormatContext->streams[i]->codec);
		av_free(&pOutputFormatContext->streams[i]);
	}

LINK_ERROR:
	av_free(pInputCodec);		
	av_free(pOutputCodec);
	av_free(pOutputFormat);
	avcodec_close(pInputCodecContext);
	avformat_close_input(&pInputFormatContext);
	av_free(pInputFormatContext);
	av_free(pOutputFormatContext);

EXIT:
	printf("press enter key to exit!\n");
	getchar();
	exit(1);
	return 0;
}

void SaveBmp(AVCodecContext *CodecContex, AVFrame *Picture, int width, int height)  
{  
	AVPicture pPictureRGB;//RGB图片  

	static struct SwsContext *img_convert_ctx;  
	img_convert_ctx = sws_getContext(width, height, CodecContex->pix_fmt, width, height,AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);  
	avpicture_alloc(&pPictureRGB, AV_PIX_FMT_RGB24, width, height);  
	sws_scale(img_convert_ctx, Picture->data, Picture->linesize,0, height, pPictureRGB.data, pPictureRGB.linesize);  

	int lineBytes = pPictureRGB.linesize[0], i=0;  

	char fileName[1024]={0};  
	time_t ltime;  
	time(&ltime);  
	sprintf_s(fileName, "%d.bmp", ltime);  

	//FILE *pDestFile = fopen(fileName, "wb");
	FILE *pDestFile = NULL;
	fopen_s(&pDestFile, fileName, "wb");  
	BITMAPFILEHEADER btfileHeader;  
	btfileHeader.bfType = MAKEWORD(66, 77);   
	btfileHeader.bfSize = lineBytes*height;   
	btfileHeader.bfReserved1 = 0;   
	btfileHeader.bfReserved2 = 0;   
	btfileHeader.bfOffBits = 54;  

	BITMAPINFOHEADER bitmapinfoheader;  
	bitmapinfoheader.biSize = 40;   
	bitmapinfoheader.biWidth = width;   
	bitmapinfoheader.biHeight = height;   
	bitmapinfoheader.biPlanes = 1;   
	bitmapinfoheader.biBitCount = 24;  
	bitmapinfoheader.biCompression = BI_RGB;   
	bitmapinfoheader.biSizeImage = lineBytes*height;   
	bitmapinfoheader.biXPelsPerMeter = 0;   
	bitmapinfoheader.biYPelsPerMeter = 0;   
	bitmapinfoheader.biClrUsed = 0;   
	bitmapinfoheader.biClrImportant = 0;  

	fwrite(&btfileHeader, 14, 1, pDestFile);  
	fwrite(&bitmapinfoheader, 40, 1, pDestFile);  
	for(i=height-1; i>=0; i--)  
	{  
		fwrite(pPictureRGB.data[0]+i*lineBytes, lineBytes, 1, pDestFile);  
	}  

	fclose(pDestFile);  
	avpicture_free(&pPictureRGB);  
}  
