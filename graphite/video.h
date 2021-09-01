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

#ifndef GRAPHITE_VIDEO_HEADER
#define GRAPHITE_VIDEO_HEADER

#include <cstdint>
#include <string>
#include <iostream>
#include <memory>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <queue>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include "libavutil/opt.h"
}
#include "libv4l2.h"
#include "linux/videodev2.h"

#include "graphite/util.h"

namespace graphite::video {

std::string AVStringError(int retcode);

enum class LiveGetResult {
    AGAIN,
    SUCCESS,
    FAILURE
};
std::string ToString(const LiveGetResult& res);
inline std::ostream& operator<<(std::ostream& os, const LiveGetResult& res) {
    os << ToString(res);
    return os;
};

class ILiveInput {
public:
    ILiveInput();
    virtual ~ILiveInput();

    virtual int Width() const = 0;
    virtual int Height() const = 0;
    virtual int BufferSize() const;

    // Supplied buffer will be preallocated width() * height() * 3 in size. 
    // Will be filled in BGR order
    // ptsMilliseconds is the ~timestamp of the frame in milliseconds
    virtual LiveGetResult Get(uint8_t* buffer, int64_t* ptsMilliseconds) = 0;

    virtual void Reopen() = 0;
    virtual void ClearError() = 0;
    virtual std::string LastError() = 0;
    virtual std::string Information() = 0;
};
typedef std::unique_ptr<ILiveInput> ILiveInputPtr;

////////////////////////////////////////////////////////////////////////////////

struct LiveInputFrame {
    LiveInputFrame();
    LiveInputFrame(int width, int height);
    ~LiveInputFrame(); // frees buffer

    int64_t FrameNumber;
    int64_t PtsMilliseconds;

    int Width, Height;
    uint8_t* Buffer;    // CV_8UC3 BGR order
};
typedef std::shared_ptr<LiveInputFrame> LiveInputFramePtr;

// A class that has its own thread that continuously reads from the live input.
// Maintains some relevant (health) information.
// Can reopen the input if necessary (I had a problem with twitch streams...)
class LiveInputThread {
public:
    LiveInputThread(ILiveInputPtr input, 
            int queueSize,
            bool allowDiscard = true);
    ~LiveInputThread();

    // The latest frame is always available (for things like display)
    // (Except when it isn't, then you'll get nullptr)
    LiveInputFramePtr GetLatestFrame() const; 

    // A Queue of frames is maintained, get the next one. You have to read this
    // fast enough, or else some might get discarded. If nullptr is returned
    // check HasError, if false then just wait for more to be read from the input
    // To 'simulate' a backlog, set a large queue and use GetFrame(backlogsize)
    // && NextFrame(1)
    LiveInputFramePtr GetFrame(int index = 0, int advance = 0);
    LiveInputFramePtr GetNextFrame(); // Convenience method
    void Advance(int amt = 1);


    // Ask the reading thread to reopen the input at its convenience
    void RequestReset(); 

    bool GetPaused() const;
    void SetPaused(bool paused);

    bool HasError() const;
    std::string GetError() const;
    std::string InputInformation() const;

    // Stats
    int64_t NumReadFrames() const;
    int64_t NumReadFramesSinceLastReset() const;
    int64_t NumDiscardedFrames() const; // If you don't empty the queue fast enough then 
                                        // old frames are discarded
    int64_t NumDiscardedFramesSinceLastReset() const;
    int64_t OpenMilliseconds() const;
    int64_t OpenMillisecondsSinceLastReset() const;
    int64_t MillisecondsSinceLastFrame() const;
    int64_t QueueSize() const;

    // Performance
    int64_t GetResetThresholdMilliseconds() const;
    void SetResetThresholdMilliseconds(int64_t resetThreshold);
    int64_t GetQueueCapacity() const;
    void SetQueueCapacity(int64_t capacity);

private:
    void AdvanceAlreadyLocked(int amt);
    void WatchingThread();
    void ProcessReadFrame(LiveInputFramePtr p);
    bool ShouldReset();
    void Reset();


private:
    std::atomic<bool> m_ShouldStop;
    std::atomic<bool> m_ShouldReset;
    std::atomic<bool> m_HasError;
    std::atomic<bool> m_Paused;

    std::atomic<int64_t> m_NumReadFrames;
    std::atomic<int64_t> m_NumReadFramesSinceLastReset;
    std::atomic<int64_t> m_NumDiscardedFrames;
    std::atomic<int64_t> m_NumDiscardedFramesSinceLastReset;
    std::atomic<bool> m_AllowDiscard;

    std::atomic<int64_t> m_ResetThreshold;
    std::atomic<int64_t> m_QueueCapacity;

    const util::mclock::time_point m_StartTime;
    util::mclock::time_point m_ResetTime;
    util::mclock::time_point m_LastHit;

    mutable std::mutex m_SharedMutex;
    std::string m_InformationString;
    std::string m_ErrorString;
    std::deque<LiveInputFramePtr> m_Queue;
    LiveInputFramePtr m_LatestFrame;

    ILiveInputPtr m_LiveInput;
    std::thread m_WatchingThread;
};

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////

class LibAVLiveInput : public ILiveInput {
public:
    // As: "https://blahblahblah.m3u8"
    // or: "http://127.0.0.1:34613/" (remember:
    //  streamlink --twitch-disable-ads --twitch-low-latency twitch.tv/blah 720p60 
    //      --player-external-http)
    LibAVLiveInput(const std::string& input); 
    ~LibAVLiveInput();

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

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////

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
