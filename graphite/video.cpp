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

#include <iostream>
#include <cassert>
#include <sstream>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>

#include "fmt/core.h"

#include "graphite/video.h"

using namespace graphite::video;

std::string graphite::video::AVStringError(int retcode) {
    char errbuf[1024] = { 0 };
    av_strerror(retcode, errbuf, 1024);
    std::string err(errbuf);
    return err;
}

ILiveInput::ILiveInput() {
}

ILiveInput::~ILiveInput() {
}

int ILiveInput::BufferSize() const {
    return Width() * Height() * 3;
}

std::string graphite::video::ToString(const LiveGetResult& res) {
    if (res == LiveGetResult::AGAIN) {
        return "LiveGetResult::AGAIN";
    } else if (res == LiveGetResult::SUCCESS) {
        return "LiveGetResult::SUCCESS";
    } else {
        return "LiveGetResult::FAILURE";
    }
    return "";
}

////////////////////////////////////////////////////////////////////////////////

LiveInputFrame::LiveInputFrame()
    : Buffer(nullptr) {
};

LiveInputFrame::LiveInputFrame(int width, int height)
    : Width(width)
    , Height(height)
    , Buffer(new uint8_t[width * height * 3]) {
}

LiveInputFrame::~LiveInputFrame() {
    if (Buffer) {
        delete[] Buffer;
    }
}

////////////////////////////////////////////////////////////////////////////////

LiveInputThread::LiveInputThread(ILiveInputPtr input, int queueSize, bool allowDiscard)
    : m_ShouldStop(false)
    , m_ShouldReset(false)
    , m_HasError(false)
    , m_Paused(false)
    , m_NumReadFrames(0)
    , m_NumReadFramesSinceLastReset(0)
    , m_NumDiscardedFrames(0)
    , m_NumDiscardedFramesSinceLastReset(0)
    , m_ResetThreshold(-1)
    , m_QueueCapacity(queueSize)
    , m_AllowDiscard(allowDiscard)
    , m_StartTime(util::mclock::now())
    , m_ResetTime(m_StartTime)
    , m_InformationString(input->Information()) 
    , m_LiveInput(std::move(input)) 
{
    if (!m_LiveInput) {
        throw std::runtime_error("no input");
    }
    m_WatchingThread = std::thread(
            &LiveInputThread::WatchingThread, this);
}

LiveInputThread::~LiveInputThread() {
    m_ShouldStop = true;
    m_WatchingThread.join();
}

LiveInputFramePtr LiveInputThread::GetLatestFrame() const {
    LiveInputFramePtr f;
    {
        std::lock_guard<std::mutex> lock(m_SharedMutex);
        f = m_LatestFrame;
    }
    return f;
}

LiveInputFramePtr LiveInputThread::GetFrame(int index, int advance) {
    if (index >= 0) {
        std::lock_guard<std::mutex> lock(m_SharedMutex);
        if (m_Queue.empty() || index >= m_Queue.size()) {
            return nullptr;
        }
        auto p = m_Queue[index];
        AdvanceAlreadyLocked(advance);
        return p;
    }
    return nullptr;
}


void LiveInputThread::Advance(int amt) {
    if (amt > 0) {
        std::lock_guard<std::mutex> lock(m_SharedMutex);
        AdvanceAlreadyLocked(amt);
    }
}

void LiveInputThread::AdvanceAlreadyLocked(int amt) {
    if (amt > 0) {
        if (amt >= m_Queue.size()) {
            m_Queue.clear();
        } else {
            m_Queue.erase(m_Queue.begin(), m_Queue.begin() + amt);
        }
    }
}



void LiveInputThread::ProcessReadFrame(LiveInputFramePtr p) {
    p->FrameNumber = m_NumReadFrames;
    m_NumReadFrames++;
    m_NumReadFramesSinceLastReset++;
    {
        std::lock_guard<std::mutex> lock(m_SharedMutex);
        if (m_Queue.size() >= m_QueueCapacity && m_AllowDiscard) {
            m_NumDiscardedFramesSinceLastReset++;
            m_NumDiscardedFrames++;
            m_Queue.pop_front();
        }
        m_Queue.push_back(p);
        m_LatestFrame = p;
        m_LastHit = util::mclock::now();
    }
}

int64_t LiveInputThread::MillisecondsSinceLastFrame() const {
    return util::ElapsedMillisFrom(m_LastHit);
}

bool LiveInputThread::ShouldReset() {
    // I honestly don't remember what I was doing here.
    if (m_ResetThreshold > 0 && MillisecondsSinceLastFrame() > m_ResetThreshold) {
        return util::ElapsedMillisFrom(m_ResetTime) > (m_ResetThreshold / 2);
    }
    return m_ShouldReset;
}

void LiveInputThread::WatchingThread() {
    auto AllocFrame = [&](){
        LiveInputFramePtr p = std::make_shared<LiveInputFrame>(
                m_LiveInput->Width(), m_LiveInput->Height());
        return p;
    };
    LiveInputFramePtr p = AllocFrame();


    while (!m_ShouldStop) {
        if (ShouldReset()) {
            Reset();
            p = AllocFrame();
        }
        if (m_Paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        if (!m_AllowDiscard) {
            bool queueFull = false;
            {
                std::lock_guard<std::mutex> lock(m_SharedMutex);
                queueFull = m_Queue.size() >= m_QueueCapacity;
            }

            if (queueFull) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
        }

        LiveGetResult res = m_LiveInput->Get(p->Buffer, &p->PtsMilliseconds);
        if (res == LiveGetResult::AGAIN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        } else if (res == LiveGetResult::SUCCESS) {
            ProcessReadFrame(p);
            p = AllocFrame();
        } else {
            {
                std::lock_guard<std::mutex> lock(m_SharedMutex);
                m_ErrorString = m_LiveInput->LastError();
                m_HasError = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}

void LiveInputThread::RequestReset() {
    m_ShouldReset = true;
}

bool LiveInputThread::GetPaused() const {
    return m_Paused;
}

void LiveInputThread::SetPaused(bool paused) {
    m_Paused = paused;
}

void LiveInputThread::Reset() {
    m_LiveInput->ClearError();
    m_LiveInput->Reopen();
    {
        std::lock_guard<std::mutex> lock(m_SharedMutex);
        m_ErrorString = m_LiveInput->LastError();
        m_InformationString = m_LiveInput->Information();
        m_HasError = m_ErrorString != "";
        m_ResetTime = util::mclock::now();
        m_Queue.clear();
    }
    m_ShouldReset = false;
    m_NumReadFramesSinceLastReset = 0;
    m_NumDiscardedFramesSinceLastReset = 0;
}

bool LiveInputThread::HasError() const {
    return m_HasError;
}

std::string LiveInputThread::GetError() const {
    return m_ErrorString;
}

std::string LiveInputThread::InputInformation() const {
    return m_InformationString;
}

int64_t LiveInputThread::NumReadFrames() const {
    return m_NumReadFrames;
}

int64_t LiveInputThread::NumReadFramesSinceLastReset() const {
    return m_NumReadFramesSinceLastReset;
}

int64_t LiveInputThread::NumDiscardedFrames() const {
    return m_NumDiscardedFrames;
}

int64_t LiveInputThread::NumDiscardedFramesSinceLastReset() const {
    return m_NumDiscardedFramesSinceLastReset;
}


int64_t LiveInputThread::OpenMilliseconds() const {
    return util::ElapsedMillisFrom(m_StartTime);
}

int64_t LiveInputThread::OpenMillisecondsSinceLastReset() const {
    return util::ElapsedMillisFrom(m_ResetTime);
}

int64_t LiveInputThread::QueueSize() const {
    std::lock_guard<std::mutex> lock(m_SharedMutex);
    return static_cast<int64_t>(m_Queue.size());
}

int64_t LiveInputThread::GetResetThresholdMilliseconds() const {
    return m_ResetThreshold;
}

void LiveInputThread::SetResetThresholdMilliseconds(int64_t resetThreshold) {
    m_ResetThreshold = resetThreshold;
}

int64_t LiveInputThread::GetQueueCapacity() const {
    return m_QueueCapacity;
}

void LiveInputThread::SetQueueCapacity(int64_t capacity) {
    m_QueueCapacity = capacity;
}


////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////

V4L2LiveInput::V4L2LiveInput(const std::string& input)
    : m_Input(input) 
    , m_FileDescriptor(-1)
    , m_Streamon(false)
{
    Open();
}

V4L2LiveInput::~V4L2LiveInput() {
    Close();
}

void V4L2LiveInput::Open() {
    m_FileDescriptor = open(m_Input.c_str(), O_RDWR | O_NONBLOCK);
    if (m_FileDescriptor < 0) {
        std::ostringstream os;
        os << "Failed opening video stream: " << strerror(errno);
        m_LastError = os.str();
        return;
    }

    v4l2_capability cap;
    if (v4l2_ioctl(m_FileDescriptor, VIDIOC_QUERYCAP, &cap) < 0) {
        std::ostringstream os;
        os << "Failed querying video capabilities: " << strerror(errno);
        m_LastError = os.str();
        return;
    }

    std::ostringstream info;
    info << "V4L2 Stream - (" << cap.driver << ") (" << cap.card << ") "
         << " version: "
         << (cap.version >> 16) << "." 
         << ((cap.version >> 8 & 0xFF)) << "." 
         << ((cap.version & 0xFF)) << std::endl;
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        m_LastError = "Single-planar video capture not supported";
        return;
    }

    v4l2_format format = {};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(m_FileDescriptor, VIDIOC_G_FMT, &format) < 0) {
        std::ostringstream os;
        os << "VIDIOC_G_FMT failed: " << strerror(errno);
        m_LastError = os.str();
        return;
    }

    m_Width = format.fmt.pix.width;
    m_Height = format.fmt.pix.height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
    if (ioctl(m_FileDescriptor, VIDIOC_S_FMT, &format) < 0) {
        std::ostringstream os;
        os << "VIDIOC_S_FMT failed: " << strerror(errno);
        m_LastError = os.str();
        return;
    }

    info << "[" << m_Width << "x" << m_Height << "] : BGR24" << std::endl;

    int nbufs = 4;

    v4l2_requestbuffers req = {0};
    req.count = nbufs;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (v4l2_ioctl(m_FileDescriptor, VIDIOC_REQBUFS, &req) < 0) {
        std::ostringstream os;
        os << "VIDIOC_REQBUFS failed: " << strerror(errno);
        m_LastError = os.str();
        return;
    }

    m_BufferInfos.resize(nbufs);
    m_Buffers.resize(nbufs);
    for (int i = 0; i < nbufs; i++) {
        v4l2_buffer& buf = m_BufferInfos[i];
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (v4l2_ioctl(m_FileDescriptor, VIDIOC_QUERYBUF, &buf)) {
            std::ostringstream os;
            os << "VIDIOC_REQBUFS failed: " << strerror(errno);
            m_LastError = os.str();
            return;
        }

        m_Buffers[i] = mmap(NULL, 
            buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, 
            m_FileDescriptor, buf.m.offset);
        if (m_Buffers[i] == MAP_FAILED) {
            std::ostringstream os;
            os << "map failed: " << strerror(errno);
            m_LastError = os.str();
            return;
        }

        memset(m_Buffers[i], 0, buf.length);
    }

    for (auto& buf : m_BufferInfos) {
        if (v4l2_ioctl(m_FileDescriptor, VIDIOC_QBUF, &buf) < 0) {
            std::ostringstream os;
            os << "VIDIOC_QBUF failed: " << strerror(errno);
            m_LastError = os.str();
            return;
        }
    }
    m_Streamon = false;

    //uint32_t fcc = format.fmt.pix.pixelformat;
    //std::cout << static_cast<char>((fcc      ) & 0x7f);
    //std::cout << static_cast<char>((fcc >>  8) & 0x7f);
    //std::cout << static_cast<char>((fcc >> 16) & 0x7f);
    //std::cout << static_cast<char>((fcc >> 24) & 0x7f) << std::endl;


    m_Information = info.str();

}

void V4L2LiveInput::Close() {
    if (m_Streamon) {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (v4l2_ioctl(m_FileDescriptor, VIDIOC_STREAMOFF, &type) < 0) {
            std::ostringstream os;
            os << "VIDIOC_STREAMOFF failed: " << strerror(errno);
            m_LastError = os.str();
        }
        m_Streamon = false;
    }

    if (m_FileDescriptor >= 0) {
        close(m_FileDescriptor);
        m_FileDescriptor = -1;
    }
}

int V4L2LiveInput::Width() const {
    return m_Width;
}

int V4L2LiveInput::Height() const {
    return m_Height;
}

LiveGetResult V4L2LiveInput::Get(uint8_t* buffer, int64_t* ptsMilliseconds) {
    if (m_LastError != "") {
        return LiveGetResult::FAILURE;
    }
    if (!m_Streamon) {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (v4l2_ioctl(m_FileDescriptor, VIDIOC_STREAMON, &type) < 0) {
            std::ostringstream os;
            os << "VIDIOC_STREAMON failed: " << strerror(errno);
            m_LastError = os.str();
            return LiveGetResult::FAILURE;
        }
        m_Streamon = true;
    }
    memset(&m_GetBuffer, 0, sizeof(m_GetBuffer));
    m_GetBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    m_GetBuffer.memory = V4L2_MEMORY_MMAP;

    int ret = v4l2_ioctl(m_FileDescriptor, VIDIOC_DQBUF, &m_GetBuffer);
    if (ret == 0) {
        int index = m_GetBuffer.index;
        if (v4l2_ioctl(m_FileDescriptor, VIDIOC_QBUF, &m_BufferInfos[index]) < 0) {
            std::ostringstream os;
            os << "VIDIOC_QBUF failed: " << strerror(errno);
            m_LastError = os.str();
            return LiveGetResult::FAILURE;
        }

        // Got a frame!
        if (buffer) {
            std::copy(reinterpret_cast<uint8_t*>(m_Buffers[index]), 
                    reinterpret_cast<uint8_t*>(m_Buffers[index]) + m_GetBuffer.length, 
                    buffer);
        }
        if (ptsMilliseconds) {
            *ptsMilliseconds = m_GetBuffer.timestamp.tv_sec * 1000 + m_GetBuffer.timestamp.tv_usec / 1000;
        }
        return LiveGetResult::SUCCESS;
    } else if (errno == EAGAIN) {
        return LiveGetResult::AGAIN;
    } else {
        std::ostringstream os;
        os << "VIDIOC_DQBUF failed: " << strerror(errno);
        m_LastError = os.str();
        return LiveGetResult::FAILURE;
    }

    return LiveGetResult::FAILURE;
}

void V4L2LiveInput::Reopen() {
    Close();
    Open();
}

void V4L2LiveInput::ClearError() {
    m_LastError = "";
}
std::string V4L2LiveInput::LastError() {
    return m_LastError;
}

std::string V4L2LiveInput::Information() {
    return m_Information;
}
