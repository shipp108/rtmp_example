/******************************************************************** 
filename:   RTMPStream.h
created:    2013-04-3
author:     firehood 
purpose:    发送H264视频到RTMP Server，使用libRtmp库
*********************************************************************/ 
#pragma once
#include "rtmp.h"
#include "rtmp_sys.h"
#include "amf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include <vector>

using namespace std;

#define FILEBUFSIZE (1024 * 1024 * 100)       //  10M

// NALU单元
typedef struct _NaluUnit
{
	int type;
	int size;
	unsigned char *data;
}NaluUnit;

typedef struct _RTMPMetadata
{
	// video, must be h264 type
	unsigned int	nWidth;
	unsigned int	nHeight;
	unsigned int	nFrameRate;		// fps
	unsigned int	nVideoDataRate;	// bps
	unsigned int	nSpsLen;
	unsigned char	Sps[1024];
	unsigned int	nPpsLen;
	unsigned char	Pps[1024];

	// audio, must be aac type
	bool	        bHasAudio;
	unsigned int	nAudioSampleRate;
	unsigned int	nAudioSampleSize;
	unsigned int	nAudioChannels;
	char		    pAudioSpecCfg;
	unsigned int	nAudioSpecCfgLen;

} RTMPMetadata,*LPRTMPMetadata;


class CRTMPStream
{
public:
	CRTMPStream(void);
	~CRTMPStream(void);
public:
	// 连接到RTMP Server
	bool Connect(const char* url);
    // 断开连接
	void Close();
    // 发送MetaData
	bool SendMetadata(LPRTMPMetadata lpMetaData);
    // 发送H264数据帧
	bool SendH264Packet(unsigned char *data,unsigned int size,bool bIsKeyFrame,unsigned int nTimeStamp);

	// send H264 data frames
	bool SendH264Frames(unsigned char *frameBuffer, unsigned int frameBufferSize, bool is_first);

private:

	// 发送数据
	int SendPacket(unsigned int nPacketType,unsigned char *data,unsigned int size,unsigned int nTimestamp);

	// process first frame, get sps, psp, sei, idr
	bool processFirstFrame(unsigned char* data, const unsigned int length, RTMPMetadata &metaData);

	// process frames excluding first frame
	bool processNormalFrame(unsigned char *data, const unsigned int length, NaluUnit &naluUnit);

	unsigned int get_diff_time() {
		if (tv_begin.tv_sec == tv_now.tv_sec && tv_begin.tv_usec == tv_now.tv_usec) {
			gettimeofday(&tv_now, NULL);
			return 0;
		}
		gettimeofday(&tv_now, NULL);
		return (unsigned int)((tv_now.tv_sec - tv_begin.tv_sec) * 1e3 + (tv_now.tv_usec - tv_begin.tv_usec) * 1e-3);
	}
private:
	RTMP* m_pRtmp;
	RTMPMetadata m_MetaData;
	unsigned char *m_pMetaDataBuffer;
	std::vector<unsigned char> m_vPacketBuffer;
	struct timeval tv_begin, tv_now;
};
