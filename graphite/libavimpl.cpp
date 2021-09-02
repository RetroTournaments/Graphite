////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2021-2021 FlibidyDibidy
//
// This file is part of Graphite.
//
// Graphite is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Graphite is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Graphite; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#include <sstream>
#include <cassert>

#include "fmt/core.h"

#include "graphite/libavimpl.h"

using namespace graphite::video;

std::string graphite::video::AVStringError(int retcode) {
    char errbuf[1024] = { 0 };
    av_strerror(retcode, errbuf, 1024);
    std::string err(errbuf);
    return err;
}

LibAVLiveInput::LibAVLiveInput(const std::string& input)
    : m_Input(input) 
    , m_AVFormatContext(nullptr)
    , m_AVCodecContext(nullptr)
    , m_SwsContext(nullptr)
    , m_Picture(nullptr)
    , m_BGRPicture(nullptr)
    , m_Buffer(nullptr) 
    , m_InputIndex(-1) {
    av_log_set_level(AV_LOG_ERROR);
    Open();
}

LibAVLiveInput::~LibAVLiveInput() {
    Close();
}

static int InterruptCallback(void* opaque) {
    return 0;
}

void LibAVLiveInput::ReportError(const std::string& func, int retcode) {
    std::ostringstream os;
    os <<  "[ERROR] in " << func << std::endl
       << "  " << AVStringError(retcode) << std::endl;
    m_LastError = os.str();
}

void LibAVLiveInput::Open() {
    assert(m_AVFormatContext == nullptr);
    assert(m_AVCodecContext == nullptr);
    assert(m_SwsContext == nullptr);
    assert(m_InputIndex == -1);

    // Open the input and find stream info
    AVDictionary* dict = nullptr;

    av_dict_set(&dict, "rtsp_transport", "tcp", 0);
    int ret = avformat_open_input(&m_AVFormatContext, m_Input.c_str(), nullptr, &dict);
    av_dict_free(&dict);
    if (ret != 0) return ReportError("avformat_open_input", ret);

    ret = avformat_find_stream_info(m_AVFormatContext, nullptr);
    if (ret != 0) return ReportError("avformat_find_stream_info", ret);

    // Set up for interruptions
    assert(m_AVFormatContext);
    // TODO
    m_AVFormatContext->interrupt_callback.callback = InterruptCallback;
    m_AVFormatContext->interrupt_callback.opaque = this;

    // Find the 'video' stream and the appropriate codec
    assert(m_AVCodecContext == nullptr);
    assert(m_InputIndex == -1);
    for (int i = 0; i < m_AVFormatContext->nb_streams; i++) {
        AVCodecContext* avctx = avcodec_alloc_context3(nullptr);

        ret = avcodec_parameters_to_context(avctx, m_AVFormatContext->streams[i]->codecpar);
        if (ret != 0) return ReportError("avcodec_parameters_to_context", ret);

        avctx->pkt_timebase = m_AVFormatContext->streams[i]->time_base;

        AVCodec* codec = avcodec_find_decoder(avctx->codec_id);
        if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            ret = avcodec_open2(avctx, codec, nullptr);
            if (ret != 0) return ReportError("avcodec_open2", ret);

            m_InputIndex = i;
            m_AVCodecContext = avctx;

            uint32_t width = avctx->width;
            uint32_t height = avctx->height;

            m_SwsContext = sws_getContext(width, height, m_AVCodecContext->pix_fmt,
                    width, height, AV_PIX_FMT_BGR24,
                    SWS_BICUBIC, NULL, NULL, NULL);

            m_Picture = av_frame_alloc();
            m_BGRPicture = av_frame_alloc();
            m_NumBytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, width, height, 1);
            m_Buffer = (uint8_t *) av_malloc(m_NumBytes * sizeof(uint8_t));

            av_image_fill_arrays(
                    m_BGRPicture->data, m_BGRPicture->linesize,
                    m_Buffer, AV_PIX_FMT_BGR24, width, height, 1);
            break;
        } else {
            avcodec_free_context(&avctx);
        }
    }
    for (int i = 0; i < m_AVFormatContext->nb_streams; i++) {
        if (i != m_InputIndex) {
            m_AVFormatContext->streams[i]->discard = AVDISCARD_ALL;
        }
    }

    m_Information = fmt::format("LibAVInput: {}\n{}x{}", m_Input,
            Width(), Height());
}

void LibAVLiveInput::Close() {
    if (m_AVFormatContext) {
        avformat_close_input(&m_AVFormatContext);
        m_AVFormatContext = nullptr;
    }
    if (m_AVCodecContext) {
        avcodec_free_context(&m_AVCodecContext);
        m_AVCodecContext = nullptr;
    }
    if (m_SwsContext) {
        sws_freeContext(m_SwsContext);
        m_SwsContext = nullptr;
    }
    if (m_Picture) {
        av_frame_unref(m_Picture);
        av_frame_free(&m_Picture);
        m_Picture = nullptr;
    }
    if (m_BGRPicture) {
        av_frame_unref(m_BGRPicture);
        av_frame_free(&m_BGRPicture);
        m_BGRPicture = nullptr;
    }
    if (m_Buffer) {
        av_free(m_Buffer);
        m_Buffer = nullptr;
    }
    m_InputIndex = -1;
}

int LibAVLiveInput::Width() const {
    if (m_AVCodecContext) {
        return m_AVCodecContext->width;
    }
    return 0;
}

int LibAVLiveInput::Height() const {
    if (m_AVCodecContext) {
        return m_AVCodecContext->height;
    }
    return 0;
}

LiveGetResult LibAVLiveInput::Get(uint8_t* buffer, int64_t* ptsMilliseconds) {
    if (m_AVFormatContext == nullptr ||
        m_AVCodecContext == nullptr ||
        m_SwsContext == nullptr ||
        m_InputIndex < 0) {
        if (m_LastError == "") {
            m_LastError = "[ERROR] in LibAVLiveInput::Get\n  Input is not open?";
        }
        return LiveGetResult::FAILURE;
    }
    if (m_LastError != "") {
        return LiveGetResult::FAILURE;
    }

    AVPacket* packet = av_packet_alloc();
    double pts = 0;

    int gotFrame = 0;
    int ret = av_read_frame(m_AVFormatContext, packet);
    if (ret == AVERROR(EAGAIN)) return LiveGetResult::AGAIN;
    if (ret != 0) {
        ReportError("av_read_frame", ret);
        return LiveGetResult::FAILURE;
    }

    if (packet->stream_index == m_InputIndex) {
        ret = avcodec_send_packet(m_AVCodecContext, packet);

        ret = avcodec_receive_frame(m_AVCodecContext, m_Picture);
        if (ret == 0) {
            pts = m_Picture->pts * av_q2d(m_AVFormatContext->streams[m_InputIndex]->time_base);
            *ptsMilliseconds = static_cast<int64_t>(std::round(pts * 1000));

            sws_scale(m_SwsContext, (const uint8_t* const *) m_Picture->data,
                    m_Picture->linesize, 0, Height(), m_BGRPicture->data,
                    m_BGRPicture->linesize);

            memcpy(buffer, m_Buffer, m_NumBytes);
            gotFrame = 1;
        }
    }
    av_packet_unref(packet);

    av_packet_free(&packet);
    if (gotFrame) return LiveGetResult::SUCCESS;
    return LiveGetResult::AGAIN;
}
void LibAVLiveInput::Reopen() {
    Close();
    Open();
}

void LibAVLiveInput::ClearError() {
    m_LastError = "";
}
std::string LibAVLiveInput::LastError() {
    return m_LastError;
}

std::string LibAVLiveInput::Information() {
    return m_Information;
}

