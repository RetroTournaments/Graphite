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

#ifndef RGMS_LIBAVIMPL_HEADER
#define RGMS_LIBAVIMPL_HEADER

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include "libavutil/opt.h"
}

#include "rgmvideo/video.h"

namespace rgms::video {

std::string AVStringError(int retcode);


class LibAVVideoSource : public IVideoSource {
public:
    // As: "https://blahblahblah.m3u8"
    // or: "http://127.0.0.1:34613/" (remember:
    //  streamlink --twitch-disable-ads --twitch-low-latency twitch.tv/blah 720p60 
    //      --player-external-http)
    LibAVVideoSource(const std::string& input); 
    ~LibAVVideoSource();

    int Width() const override final;
    int Height() const override final;

    GetResult Get(uint8_t* buffer, int64_t* ptsMilliseconds) override final;
    void Reopen() override final;
    void ClearError() override final;
    std::string LastError() override final;
    std::string Information() override final;

private:
    void Open();
    void Close();
    void ReportError(const std::string& func, int retcode);

private:
    std::string m_Input;
    std::string m_LastError, m_Information;

    AVFormatContext* m_AVFormatContext;
    AVCodecContext* m_AVCodecContext;
    SwsContext* m_SwsContext;
    int m_InputIndex;

    AVFrame* m_Picture;
    AVFrame* m_BGRPicture;
    int m_NumBytes;
    uint8_t* m_Buffer;
};

}

#endif
