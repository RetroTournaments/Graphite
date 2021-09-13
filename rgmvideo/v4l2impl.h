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

#ifndef RGMS_V4L2IMPL_HEADER
#define RGMS_V4L2IMPL_HEADER

#include "libv4l2.h"
#include "linux/videodev2.h"

#include "rgmvideo/video.h"

namespace rgms::video {

class V4L2LiveInput : public ILiveInput {
public:
    // As: "/dev/videoX"
    V4L2LiveInput(const std::string& input); 
    ~V4L2LiveInput();

    int Width() const override final;
    int Height() const override final;

    LiveGetResult Get(uint8_t* buffer, int64_t* ptsMilliseconds) override final;
    void Reopen() override final;
    void ClearError() override final;
    std::string LastError() override final;
    std::string Information() override final;

private:
    void Open();
    void Close();


private:
    std::string m_Input;
    std::string m_LastError, m_Information;
    int m_FileDescriptor;
    int m_Width, m_Height;
    std::vector<v4l2_buffer> m_BufferInfos;
    std::vector<void*> m_Buffers;
    v4l2_buffer m_GetBuffer;
    bool m_Streamon;
};

}

#endif
