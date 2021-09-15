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

#ifndef RGMS_VIDEO_HEADER
#define RGMS_VIDEO_HEADER

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

#include "opencv2/opencv.hpp"

#include "rgmutil/util.h"

namespace rgms::video {

enum class GetResult {
    AGAIN,
    SUCCESS,
    FAILURE
};
std::string ToString(const GetResult& res);
inline std::ostream& operator<<(std::ostream& os, const GetResult& res) {
    os << ToString(res);
    return os;
};

enum class ErrorState {
    NO_ERROR,
    CAN_NOT_OPEN_SOURCE,
    INPUT_EXHAUSTED,
    OTHER_ERROR
};

class IVideoSource {
public:
    IVideoSource();
    virtual ~IVideoSource();

    virtual int Width() const = 0;
    virtual int Height() const = 0;
    virtual int BufferSize() const final;

    // Supplied buffer must be BufferSize() 
    // [width() * height() * 3] in size. 
    // Must be filled in BGR order
    // ptsMilliseconds must be the ~timestamp of the frame in milliseconds
    virtual GetResult Get(uint8_t* buffer, int64_t* ptsMilliseconds) = 0;

    virtual void Reopen() = 0;
    virtual void ClearError() = 0;
    virtual ErrorState GetErrorState() = 0;
    virtual std::string GetLastError() = 0;
    virtual std::string GetInformation() = 0;
};
typedef std::unique_ptr<IVideoSource> IVideoSourcePtr;

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

// A class that has its own thread that continuously reads from the live video
// source. Maintains some relevant (health) information.
// Can reopen the source if necessary (I had a problem with twitch streams...)
class LiveVideoThread {
public:
    LiveVideoThread(IVideoSourcePtr input, 
            int queueSize,
            bool allowDiscard = true);
    ~LiveVideoThread();

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

    IVideoSourcePtr m_LiveInput;
    std::thread m_WatchingThread;
};

// A class that has its own thread that tries to have frames available as
// necessary
struct StaticVideoBufferConfig {
    int BufferSize; // in bytes, you should give quite a bit of space!!! (will round up!)
    float ForwardBias; // defaults to 0.5, meaning say you have room for 100 frames and you just read frame #324, we will try to have 274 to 374 in the buffer.

    static StaticVideoBufferConfig Defaults();
};

// This is a big pain because I don't want to rely on 'seeking' video files.
// Note that this is not adequate for actual video editing or anything. It reads
// from the beginning if you go too far back
class StaticVideoBuffer {
public:
    StaticVideoBuffer(IVideoSourcePtr source,
            StaticVideoBufferConfig config = StaticVideoBufferConfig::Defaults()); 
    ~StaticVideoBuffer();

    bool HasError() const;
    ErrorState GetErrorState() const;
    std::string GetError() const;
    std::string GetInputInformation() const;

    int Width() const;
    int Height() const;
    int ImageDataBufferSize() const;
    int64_t CurrentKnownNumFrames() const; // will change as the input is read.
    void UpdatePTS(std::vector<int64_t>* pts) const;

    bool HasFrame(int64_t frameIndex) const;

    // Needs to do things like reopen the input, decode frames, or read additional frames
    bool HasWork() const; 
    void DoWork(int64_t* frameIndex = nullptr, int64_t* pts = nullptr);

    // ImageData is only valid until the next call to DoWork!!!
    // If you want to keep it then copy it yourself. Watch out for
    // GetResult::AGAIN, and GetResult::FAILURE
    GetResult GetFrame(int64_t frameIndex, int64_t* pts, const uint8_t** imageData) const;
    
private:
    struct Record {
        int64_t FrameIndex;
        int64_t PTS;
        uint8_t* Data;
    };

    bool RecordIndex(int64_t frameIndex, const std::deque<Record>& records, 
            size_t* recordIndex = nullptr) const;
    bool BufferFull() const;
    bool MustRewind() const;
    bool MustAdvance() const;

private:
    StaticVideoBufferConfig m_Config;

    IVideoSourcePtr m_Source;
    int64_t m_SourceFrameIndex;

    mutable int64_t m_TargetFrameIndex;

    int64_t m_CurrentKnownNumFrames;
    bool m_InputWasExhausted;

    int m_MaxRecords;

    std::deque<Record> m_Records; // sequential by frame index
    std::vector<uint8_t> m_Data; // Big and preallocated :O
};

struct StaticVideoThreadConfig {
    StaticVideoBufferConfig StaticVideoBufferCfg;

    static StaticVideoThreadConfig Defaults();
};

class StaticVideoThread {
public:
    StaticVideoThread(IVideoSourcePtr source,
            StaticVideoThreadConfig config = StaticVideoThreadConfig::Defaults()); 
    ~StaticVideoThread();

    bool HasError() const;
    ErrorState GetErrorState() const;
    std::string GetError() const;
    std::string GetInputInformation() const;

    int64_t CurrentKnownNumFrames() const; // will change as the input is read.
    LiveInputFramePtr GetFrame(int frameIndex);

    void UpdatePTS(std::vector<int64_t>* pts);

private:
    void BufferThread();

private:
    StaticVideoThreadConfig m_Config;

    std::atomic<bool> m_ShouldStop;

    mutable std::mutex m_BufferMutex;

    std::thread m_BufferThread;
    StaticVideoBuffer m_Buffer;

    std::atomic<bool> m_HasError;
    std::atomic<ErrorState> m_ErrorState;

    std::atomic<int64_t> m_CurrentKnownNumFrames;

    std::string m_InformationString;
    std::string m_ErrorString;
    std::vector<int64_t> m_PTS;

};

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////

class CVVideoCaptureSource : public IVideoSource {
public:
    CVVideoCaptureSource(const std::string& input); 
    ~CVVideoCaptureSource();

    int Width() const override final;
    int Height() const override final;
    GetResult Get(uint8_t* buffer, int64_t* ptsMilliseconds) override final;
    void Reopen() override final;
    void ClearError() override final;
    ErrorState GetErrorState() override final;
    std::string GetLastError() override final;
    std::string GetInformation() override final;

private:
    bool ReadNextFrame();

private:
    bool m_OnFirstFrame;
    cv::Mat m_Frame;
    double m_PTS;
    int m_Width;
    int m_Height;
    int m_NumBytes;

    std::string m_Input;
    std::string m_Error;
    ErrorState m_ErrorState;
    std::unique_ptr<cv::VideoCapture> m_Capture;
};


}

#endif
