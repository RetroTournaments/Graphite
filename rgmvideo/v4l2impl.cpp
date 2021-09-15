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

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>

#include "rgmvideo/v4l2impl.h"

using namespace rgms::video;

////////////////////////////////////////////////////////////////////////////////

V4L2VideoSource::V4L2VideoSource(const std::string& input)
    : m_Input(input) 
    , m_FileDescriptor(-1)
    , m_Streamon(false)
    , m_ErrorState(ErrorState::NO_ERROR)
{
    Open();
}

V4L2VideoSource::~V4L2VideoSource() {
    Close();
}

void V4L2VideoSource::Open() {
    m_FileDescriptor = open(m_Input.c_str(), O_RDWR | O_NONBLOCK);
    if (m_FileDescriptor < 0) {
        std::ostringstream os;
        os << "Failed opening video stream: " << strerror(errno);
        m_LastError = os.str();
        m_ErrorState = ErrorState::CAN_NOT_OPEN_SOURCE;
        return;
    }

    v4l2_capability cap;
    if (v4l2_ioctl(m_FileDescriptor, VIDIOC_QUERYCAP, &cap) < 0) {
        std::ostringstream os;
        os << "Failed querying video capabilities: " << strerror(errno);
        m_LastError = os.str();
        m_ErrorState = ErrorState::OTHER_ERROR;
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
        m_ErrorState = ErrorState::OTHER_ERROR;
        return;
    }

    v4l2_format format = {};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(m_FileDescriptor, VIDIOC_G_FMT, &format) < 0) {
        std::ostringstream os;
        os << "VIDIOC_G_FMT failed: " << strerror(errno);
        m_LastError = os.str();
        m_ErrorState = ErrorState::OTHER_ERROR;
        return;
    }

    m_Width = format.fmt.pix.width;
    m_Height = format.fmt.pix.height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
    if (ioctl(m_FileDescriptor, VIDIOC_S_FMT, &format) < 0) {
        std::ostringstream os;
        os << "VIDIOC_S_FMT failed: " << strerror(errno);
        m_LastError = os.str();
        m_ErrorState = ErrorState::OTHER_ERROR;
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
        m_ErrorState = ErrorState::OTHER_ERROR;
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
            m_ErrorState = ErrorState::OTHER_ERROR;
            return;
        }

        m_Buffers[i] = mmap(NULL, 
            buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, 
            m_FileDescriptor, buf.m.offset);
        if (m_Buffers[i] == MAP_FAILED) {
            std::ostringstream os;
            os << "map failed: " << strerror(errno);
            m_LastError = os.str();
            m_ErrorState = ErrorState::OTHER_ERROR;
            return;
        }

        memset(m_Buffers[i], 0, buf.length);
    }

    for (auto& buf : m_BufferInfos) {
        if (v4l2_ioctl(m_FileDescriptor, VIDIOC_QBUF, &buf) < 0) {
            std::ostringstream os;
            os << "VIDIOC_QBUF failed: " << strerror(errno);
            m_LastError = os.str();
            m_ErrorState = ErrorState::OTHER_ERROR;
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

void V4L2VideoSource::Close() {
    if (m_Streamon) {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (v4l2_ioctl(m_FileDescriptor, VIDIOC_STREAMOFF, &type) < 0) {
            std::ostringstream os;
            os << "VIDIOC_STREAMOFF failed: " << strerror(errno);
            m_LastError = os.str();
            m_ErrorState = ErrorState::OTHER_ERROR;
        }
        m_Streamon = false;
    }

    if (m_FileDescriptor >= 0) {
        close(m_FileDescriptor);
        m_FileDescriptor = -1;
    }
}

int V4L2VideoSource::Width() const {
    return m_Width;
}

int V4L2VideoSource::Height() const {
    return m_Height;
}

GetResult V4L2VideoSource::Get(uint8_t* buffer, int64_t* ptsMilliseconds) {
    if (m_LastError != "") {
        return GetResult::FAILURE;
    }
    if (!m_Streamon) {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (v4l2_ioctl(m_FileDescriptor, VIDIOC_STREAMON, &type) < 0) {
            std::ostringstream os;
            os << "VIDIOC_STREAMON failed: " << strerror(errno);
            m_LastError = os.str();
            m_ErrorState = ErrorState::OTHER_ERROR;
            return GetResult::FAILURE;
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
            m_ErrorState = ErrorState::OTHER_ERROR;
            return GetResult::FAILURE;
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
        return GetResult::SUCCESS;
    } else if (errno == EAGAIN) {
        return GetResult::AGAIN;
    } else {
        std::ostringstream os;
        os << "VIDIOC_DQBUF failed: " << strerror(errno);
        m_LastError = os.str();
        m_ErrorState = ErrorState::OTHER_ERROR;
        return GetResult::FAILURE;
    }

    return GetResult::FAILURE;
}

void V4L2VideoSource::Reopen() {
    Close();
    Open();
}

void V4L2VideoSource::ClearError() {
    m_LastError = "";
    m_ErrorState = ErrorState::NO_ERROR;
}

ErrorState V4L2VideoSource::GetErrorState() {
    return m_ErrorState;
}

std::string V4L2VideoSource::GetLastError() {
    return m_LastError;
}

std::string V4L2VideoSource::GetInformation() {
    return m_Information;
}
