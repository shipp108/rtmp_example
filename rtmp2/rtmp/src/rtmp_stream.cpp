
#include "rtmp_stream.h"
#include "sps_decode.h"

#include <iostream>
#include <vector>
#include <dirent.h>

using namespace std;

#define RTMP_HEAD_SIZE   (sizeof(RTMPPacket) + RTMP_MAX_HEADER_SIZE)

#ifdef WIN32
#pragma comment(lib,"WS2_32.lib")
#pragma comment(lib,"winmm.lib")
#endif

enum {
	FLV_CODECID_H264 = 7,
};

int InitSockets() {
#ifdef WIN32
	WORD version;
	WSADATA wsaData;
	version = MAKEWORD(1, 1);
	return (WSAStartup(version, &wsaData) == 0);
#else
	return true;
#endif
}

inline void CleanupSockets() {
#ifdef WIN32
	WSACleanup();
#endif
}

char * put_byte(char *output, uint8_t nVal) {
	output[0] = nVal;
	return output + 1;
}
char * put_be16(char *output, uint16_t nVal) {
	output[1] = nVal & 0xff;
	output[0] = (nVal >> 8) & 0xff;
	return output + 2;
}
char * put_be24(char *output, uint32_t nVal) {
	output[2] = nVal & 0xff;
	output[1] = (nVal >> 8) & 0xff;
	output[0] = (nVal >> 16) & 0xff;
	return output + 3;
}
char * put_be32(char *output, uint32_t nVal) {
	output[3] = nVal & 0xff;
	output[2] = (nVal >> 8) & 0xff;
	output[1] = (nVal >> 16) & 0xff;
	output[0] = (nVal >> 24) & 0xff;
	return output + 4;
}
char * put_be64(char *output, uint64_t nVal) {
	output = put_be32(output, nVal >> 32);
	output = put_be32(output, nVal);
	return output;
}
char * put_amf_string(char *c, const char *str) {
	uint16_t len = strlen(str);
	c = put_be16(c, len);
	memcpy(c, str, len);
	return c + len;
}
char * put_amf_double(char *c, double d) {
	*c++ = AMF_NUMBER; /* type: Number */
	{
		unsigned char *ci, *co;
		ci = (unsigned char *) &d;
		co = (unsigned char *) c;
		co[0] = ci[7];
		co[1] = ci[6];
		co[2] = ci[5];
		co[3] = ci[4];
		co[4] = ci[3];
		co[5] = ci[2];
		co[6] = ci[1];
		co[7] = ci[0];
	}
	return c + 8;
}

CRTMPStream::CRTMPStream(void) :
		m_pRtmp(NULL), m_pMetaDataBuffer(NULL){
	InitSockets();
	m_pRtmp = RTMP_Alloc();
	RTMP_Init(m_pRtmp);
	gettimeofday(&tv_begin, NULL);
	tv_now = tv_begin;
	memset(&m_MetaData, 0, sizeof(RTMPMetadata));
	m_vPacketBuffer.resize(512 * 1024 + RTMP_HEAD_SIZE);
}

CRTMPStream::~CRTMPStream(void) {
	Close();
#ifdef WIN32
	WSACleanup();
#endif
	if (m_pMetaDataBuffer) {
		free(m_pMetaDataBuffer);
		m_pMetaDataBuffer = NULL;
	}
}

bool CRTMPStream::Connect(const char* url) {
	if (RTMP_SetupURL(m_pRtmp, (char*) url) == false) {
		RTMP_Free(m_pRtmp);
		return false;
	}
	RTMP_EnableWrite(m_pRtmp);
	if (RTMP_Connect(m_pRtmp, NULL) == false) {
		RTMP_Free(m_pRtmp);
		return false;
	}
	if (RTMP_ConnectStream(m_pRtmp, 0) < 0) {
		RTMP_Close(m_pRtmp);
		RTMP_Free(m_pRtmp);
		return false;
	}
	return true;
}

void CRTMPStream::Close() {
	if (m_pRtmp) {
		RTMP_Close(m_pRtmp);
		RTMP_Free(m_pRtmp);
		m_pRtmp = NULL;
	}
}

int CRTMPStream::SendPacket(unsigned int nPacketType, unsigned char *data,
		unsigned int size, unsigned int nTimestamp) {
	if (m_pRtmp == NULL) {
		return false;
	}

	RTMPPacket* packet = (RTMPPacket *)data;

	packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
	packet->m_nBodySize = size;
	packet->m_hasAbsTimestamp = 0;
	packet->m_packetType = nPacketType;
	packet->m_nInfoField2 = m_pRtmp->m_stream_id;
	packet->m_nChannel = 0x04;

	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
	if (RTMP_PACKET_TYPE_AUDIO == nPacketType && size != 4) {
		packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	}
	packet->m_nTimeStamp = nTimestamp;

	int nRet = 0;
	if (RTMP_IsConnected(m_pRtmp)) {
		nRet = RTMP_SendPacket(m_pRtmp, packet, 0);
	}

	return nRet;
}

bool CRTMPStream::SendMetadata(LPRTMPMetadata lpMetaData) 
{
	if (lpMetaData == NULL) {
		return false;
	}

	get_diff_time();

	RTMPPacket *packet = NULL;
	if (m_pMetaDataBuffer == NULL) {
		
		int i;
		unsigned char *body = NULL;
		int size = 1024 + RTMP_HEAD_SIZE;

		//printf("size: %d [%d %d %d]\n", size, lpMetaData->nSpsLen, lpMetaData->nPpsLen, RTMP_HEAD_SIZE);

		m_pMetaDataBuffer = (unsigned char *)malloc(size * sizeof(unsigned char));
		if (m_pMetaDataBuffer == NULL) {
			printf("m_pMetaDataBuffer is null\n");
			return false;
		}

		memset(m_pMetaDataBuffer, 0, size);

		packet = (RTMPPacket *)m_pMetaDataBuffer;
		packet->m_body = (char *)packet + RTMP_HEAD_SIZE;

		body = (unsigned char *)packet->m_body;
		
		i = 0;
		body[i++] = 0x17;
		body[i++] = 0x00;

		body[i++] = 0x00;
		body[i++] = 0x00;
		body[i++] = 0x00;

		/*AVCDecoderConfigurationRecord*/
		body[i++] = 0x01;
		body[i++] = lpMetaData->Sps[1];
		body[i++] = lpMetaData->Sps[2];
		body[i++] = lpMetaData->Sps[3];
		body[i++] = 0xff;

		/*sps*/
		body[i++]   = 0xe1;
		body[i++] = (lpMetaData->nSpsLen >> 8) & 0xff;
		body[i++] = lpMetaData->nSpsLen & 0xff;

		memcpy(&body[i], lpMetaData->Sps, lpMetaData->nSpsLen);
		i += lpMetaData->nSpsLen;

		/*pps*/
		body[i++]   = 0x01;
		body[i++] = (lpMetaData->nPpsLen >> 8) & 0xff;
		body[i++] = (lpMetaData->nPpsLen) & 0xff;

		memcpy(&body[i], lpMetaData->Pps, lpMetaData->nPpsLen);
		i += lpMetaData->nPpsLen;

		packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
		packet->m_nBodySize = i;
		packet->m_nChannel = 0x04;
		packet->m_nTimeStamp = 0;
		packet->m_hasAbsTimestamp = 0;
		packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
		packet->m_nInfoField2 = m_pRtmp->m_stream_id;

	} else {
		packet = (RTMPPacket *)m_pMetaDataBuffer;
	}
	int nRet = 0;
	nRet = RTMP_SendPacket(m_pRtmp, packet, 0);

	return nRet;

}

bool CRTMPStream::SendH264Packet(unsigned char *data, unsigned int size,
		bool bIsKeyFrame, unsigned int nTimeStamp) {

	if (data == NULL && size < 11) {
		return false;
	}

	if(m_vPacketBuffer.size() < size + 9 + RTMP_HEAD_SIZE) {
		m_vPacketBuffer.resize(size + 10 + RTMP_HEAD_SIZE);
	}
	unsigned char *body = (unsigned char *)m_vPacketBuffer.data() + RTMP_HEAD_SIZE;

	int i = 0;
	if (bIsKeyFrame) {
		body[i++] = 0x17;	// 1:Iframe  7:AVC
	} else {
		body[i++] = 0x27;	// 2:Pframe  7:AVC
	}
	body[i++] = 0x01;	// AVC NALU
	body[i++] = 0x00;
	body[i++] = 0x00;
	body[i++] = 0x00;

	// NALU size
	body[i++] = (size >> 24) & 0xff;
	body[i++] = (size >> 16) & 0xff;
	body[i++] = (size >> 8) & 0xff;
	body[i++] = (size) & 0xff;

	// NALU data
	memcpy(&body[i], data, size);

	bool bRet = SendPacket(RTMP_PACKET_TYPE_VIDEO, m_vPacketBuffer.data(), i + size, nTimeStamp);

	return bRet;
}


bool CRTMPStream::processFirstFrame(unsigned char* data,
		const unsigned int length, RTMPMetadata &metaData) {
	int i = 0;
	// Record divides' start index in the frame buffer
	vector<unsigned int> divides;

	// Record the divides' length of the divide's start index,
	// such as length of 0x00 00 01 = 3,
	// length of 0x00 00 00 01 = 4
	vector<unsigned int> dividesLengths;

	while (i + 3 < length) {
		// start code is 0x00 00 01
		if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01) {
			divides.push_back(i);
			dividesLengths.push_back(3);
			i = i + 3;
			// start code is 0x00 00 00 01
		} else if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x00
				&& data[i + 3] == 0x01) {
			divides.push_back(i);
			dividesLengths.push_back(4);
			i = i + 4;
		} else {
			i++;
		}
	}
	divides.push_back(length);
	dividesLengths.push_back(0);

	for(int j = 0; j < divides.size() - 1; j++) {
		int nalIndex = divides[j] + dividesLengths[j];
		unsigned char *ptr = &data[nalIndex];
		unsigned char type = data[nalIndex] & 0x1f;
		switch(type) {
		case 0x07: {	// SPS
			metaData.nSpsLen = divides[j + 1] - nalIndex;
			memcpy(metaData.Sps, ptr, metaData.nSpsLen);
			int w = 0, h = 0, f = 0;
			h264_decode_sps(metaData.Sps, metaData.nSpsLen, w, h, f);
			metaData.nWidth = w;
			metaData.nHeight = h;
			metaData.nFrameRate = f;
			break;
		}
		case 0x08: {	// PPS
			metaData.nPpsLen = divides[j + 1] - nalIndex;
			memcpy(metaData.Pps, ptr, metaData.nPpsLen);
			break;
		}
		default: {
			//printf("error: unknow frame type: %x\n", results[j]->type);
		}
		}
	}

	if(metaData.nSpsLen > 0 && metaData.nPpsLen > 0) {
		return true;
	}
	return false;
}

bool CRTMPStream::processNormalFrame(unsigned char *data, const unsigned int length, NaluUnit &naluUnit) {
	int i = 0;
	int startLength = 0;

	if( i + 3 >= length) {
		return false;
	}

	if(data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x01) {
		startLength = 3;
	}

	if(data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x00 && data[i+3] == 0x01) {
		startLength = 4;
	}

	naluUnit.size = length - startLength;
	naluUnit.type = data[startLength] & 0x1f;
	naluUnit.data = &data[startLength];
	return true;
}

bool CRTMPStream::SendH264Frames(unsigned char *frameBuffer, unsigned int frameBufferSize, bool is_first)
{
	bool nRet = false;
	if (is_first) {
		// fetch SPS, PPS, SEI, IDR's imformation
		if (processFirstFrame(frameBuffer, frameBufferSize, m_MetaData)) {
			nRet = SendMetadata(&m_MetaData);
		}
	} else {
		// send normal frame
		NaluUnit naluUnit;
		if (processNormalFrame(frameBuffer, frameBufferSize, naluUnit)) {
			bool isKeyFrame = (naluUnit.type == 0x05) ? true : false;
			nRet = SendH264Packet(naluUnit.data, naluUnit.size, isKeyFrame, get_diff_time());
		}
	}
	return nRet;
}

