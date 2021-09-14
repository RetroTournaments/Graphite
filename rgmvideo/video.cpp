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


#include "fmt/core.h"

#include "rgmvideo/video.h"

using namespace rgms::video;


IVideoSource::IVideoSource() {
}

IVideoSource::~IVideoSource() {
}

int IVideoSource::BufferSize() const {
    return Width() * Height() * 3;
}

std::string rgms::video::ToString(const GetResult& res) {
    if (res == GetResult::AGAIN) {
        return "GetResult::AGAIN";
    } else if (res == GetResult::SUCCESS) {
        return "GetResult::SUCCESS";
    } else {
        return "GetResult::FAILURE";
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

LiveVideoThread::LiveVideoThread(IVideoSourcePtr source, int queueSize, bool allowDiscard)
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
    , m_InformationString(source->Information()) 
    , m_LiveInput(std::move(source)) 
{
    if (!m_LiveInput) {
        throw std::runtime_error("no source");
    }
    m_WatchingThread = std::thread(
            &LiveVideoThread::WatchingThread, this);
}

LiveVideoThread::~LiveVideoThread() {
    m_ShouldStop = true;
    m_WatchingThread.join();
}

LiveInputFramePtr LiveVideoThread::GetLatestFrame() const {
    LiveInputFramePtr f;
    {
        std::lock_guard<std::mutex> lock(m_SharedMutex);
        f = m_LatestFrame;
    }
    return f;
}

LiveInputFramePtr LiveVideoThread::GetFrame(int index, int advance) {
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


void LiveVideoThread::Advance(int amt) {
    if (amt > 0) {
        std::lock_guard<std::mutex> lock(m_SharedMutex);
        AdvanceAlreadyLocked(amt);
    }
}

void LiveVideoThread::AdvanceAlreadyLocked(int amt) {
    if (amt > 0) {
        if (amt >= m_Queue.size()) {
            m_Queue.clear();
        } else {
            m_Queue.erase(m_Queue.begin(), m_Queue.begin() + amt);
        }
    }
}



void LiveVideoThread::ProcessReadFrame(LiveInputFramePtr p) {
    p->FrameNumber = m_NumReadFrames;
    m_NumReadFrames++;
    m_NumReadFramesSinceLastReset++;
    {
        std::lock_guard<std::mutex> lock(m_SharedMutex);
        if (static_cast<int>(m_Queue.size()) >= m_QueueCapacity && m_AllowDiscard) {
            m_NumDiscardedFramesSinceLastReset++;
            m_NumDiscardedFrames++;
            m_Queue.pop_front();
        }
        m_Queue.push_back(p);
        m_LatestFrame = p;
        m_LastHit = util::mclock::now();
    }
}

int64_t LiveVideoThread::MillisecondsSinceLastFrame() const {
    return util::ElapsedMillisFrom(m_LastHit);
}

bool LiveVideoThread::ShouldReset() {
    // I honestly don't remember what I was doing here.
    if (m_ResetThreshold > 0 && MillisecondsSinceLastFrame() > m_ResetThreshold) {
        return util::ElapsedMillisFrom(m_ResetTime) > (m_ResetThreshold / 2);
    }
    return m_ShouldReset;
}

void LiveVideoThread::WatchingThread() {
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
                queueFull = static_cast<int>(m_Queue.size()) >= m_QueueCapacity;
            }

            if (queueFull) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
        }

        GetResult res = m_LiveInput->Get(p->Buffer, &p->PtsMilliseconds);
        if (res == GetResult::AGAIN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        } else if (res == GetResult::SUCCESS) {
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

void LiveVideoThread::RequestReset() {
    m_ShouldReset = true;
}

bool LiveVideoThread::GetPaused() const {
    return m_Paused;
}

void LiveVideoThread::SetPaused(bool paused) {
    m_Paused = paused;
}

void LiveVideoThread::Reset() {
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

bool LiveVideoThread::HasError() const {
    return m_HasError;
}

std::string LiveVideoThread::GetError() const {
    return m_ErrorString;
}

std::string LiveVideoThread::InputInformation() const {
    return m_InformationString;
}

int64_t LiveVideoThread::NumReadFrames() const {
    return m_NumReadFrames;
}

int64_t LiveVideoThread::NumReadFramesSinceLastReset() const {
    return m_NumReadFramesSinceLastReset;
}

int64_t LiveVideoThread::NumDiscardedFrames() const {
    return m_NumDiscardedFrames;
}

int64_t LiveVideoThread::NumDiscardedFramesSinceLastReset() const {
    return m_NumDiscardedFramesSinceLastReset;
}


int64_t LiveVideoThread::OpenMilliseconds() const {
    return util::ElapsedMillisFrom(m_StartTime);
}

int64_t LiveVideoThread::OpenMillisecondsSinceLastReset() const {
    return util::ElapsedMillisFrom(m_ResetTime);
}

int64_t LiveVideoThread::QueueSize() const {
    std::lock_guard<std::mutex> lock(m_SharedMutex);
    return static_cast<int64_t>(m_Queue.size());
}

int64_t LiveVideoThread::GetResetThresholdMilliseconds() const {
    return m_ResetThreshold;
}

void LiveVideoThread::SetResetThresholdMilliseconds(int64_t resetThreshold) {
    m_ResetThreshold = resetThreshold;
}

int64_t LiveVideoThread::GetQueueCapacity() const {
    return m_QueueCapacity;
}

void LiveVideoThread::SetQueueCapacity(int64_t capacity) {
    m_QueueCapacity = capacity;
}

////////////////////////////////////////////////////////////////////////////////

CVVideoCaptureSource::CVVideoCaptureSource(const std::string& input)
    : m_Input(input)
{
    Reopen();
}

CVVideoCaptureSource::~CVVideoCaptureSource()
{
}

int CVVideoCaptureSource::Width() const {
    return m_Width;
}

int CVVideoCaptureSource::Height() const {
    return m_Height;
}


GetResult CVVideoCaptureSource::Get(uint8_t* buffer, int64_t* ptsMilliseconds) {
    if (!m_Error.empty()) {
        return GetResult::FAILURE;
    }

    if (!m_OnFirstFrame) {
        if (!ReadNextFrame()) {
            return GetResult::FAILURE;
        }
    }

    m_OnFirstFrame = false;
    std::memcpy(buffer, m_Frame.data, m_NumBytes);
    *ptsMilliseconds = static_cast<int64_t>(std::round(m_PTS));
    return GetResult::SUCCESS;
}

bool CVVideoCaptureSource::ReadNextFrame() {
    m_Capture->read(m_Frame);
    m_PTS = m_Capture->get(cv::CAP_PROP_POS_MSEC);
    if (m_Frame.empty()) {
        m_Error = "ERROR empty frame";
        return false;
    }
    return true;
}

void CVVideoCaptureSource::Reopen() {
    ClearError();
    m_Capture = std::make_unique<cv::VideoCapture>(m_Input);
    if (!m_Capture->isOpened()) {
        m_Error = "ERROR unable to open camera";
    }

    m_OnFirstFrame = true;
    ReadNextFrame();
    m_Width = m_Frame.cols;
    m_Height = m_Frame.rows;
    m_NumBytes = m_Width * m_Height * 3;
}

void CVVideoCaptureSource::ClearError() {
    m_Error = "";
}

std::string CVVideoCaptureSource::LastError() {
    return m_Error;
}

std::string CVVideoCaptureSource::Information() {
    return m_Input;
}

////////////////////////////////////////////////////////////////////////////////

StaticVideoBufferConfig StaticVideoBufferConfig::Defaults() {
    StaticVideoBufferConfig cfg;
    cfg.BufferSize = 1024 * 1024 * 1024;
    cfg.ForwardBias = 0.5;
    return cfg;
}

StaticVideoBuffer::StaticVideoBuffer(IVideoSourcePtr source,
        StaticVideoBufferConfig config)
    : m_Config(config)
    , m_Source(std::move(source))
    , m_SourceFrameIndex(0)
    , m_TargetFrameIndex(0)
    , m_InputWasExhausted(false)
{
    if (!m_Source) {
        throw std::invalid_argument("must provide source");
    }

    int imSize = ImageDataBufferSize();
    if (imSize > 0) {
        int n = m_Config.BufferSize / imSize;
        n += 1;
        m_Data.resize(n * imSize);
    }
}

StaticVideoBuffer::~StaticVideoBuffer() {
}

bool StaticVideoBuffer::HasError() const {
    return GetErrorState() != ErrorState::NO_ERROR;
}

ErrorState StaticVideoBuffer::GetErrorState() const {
    return m_Source->GetErrorState();
}

std::string StaticVideoBuffer::GetError() const {
    return m_Source->GetLastError();
}

std::string StaticVideoBuffer::GetInputInformation() const {
    return m_Source->GetInformation();
}

int StaticVideoBuffer::Width() const {
    return m_Source->Width();
}

int StaticVideoBuffer::Height() const {
    return m_Source->Height();
}

int StaticVideoBuffer::ImageDataBufferSize() const {
    return m_Source->BufferSize();
}

int64_t StaticVideoBuffer::CurrentKnownNumFrames() const {
    return m_CurrentKnownNumFrames;
}

bool StaticVideoBuffer::HasWork() const {
    // TODO can we read more?
    return false;
}

void StaticVideoBuffer::DoWork() {
    // TODO read stuff etc
}

bool StaticVideoBuffer::RecordIndex(int64_t frameIndex, 
        const std::deque<Record>& records, size_t* recordIndex) {

    if (!records.empty() &&
        frameIndex >= records.front().frameIndex && frameIndex <= records.back().frameIndex) {

        if (recordIndex) {
            *recordIndex = static_cast<size_t>(frameIndex - records.front().frameIndex);
        }
        return true;
    }
    return false;
}

bool StaticVideoBuffer::HasFrame(int64_t frameIndex) {
    return RecordIndex(frameIndex, m_Records) || RecordIndex(frameIndex, m_RewindRecords);
}

GetResult StaticVideoBuffer::GetFrame(int64_t frameIndex, int64_t* pts, uint8_t** imageData) {
    m_TargetFrameIndex = frameIndex;
    if (HasError()) {
        return GetResult::FAILURE;
    }

    size_t recordIndex;
    Record* r = nullptr;
    if (RecordIndex(frameIndex, m_Records, &recordIndex)) {
        r = &m_Records[recordIndex];
    } else if (RecordIndex(frameIndex, m_RewindRecords, &recordIndex)) {
        r = &m_Records[recordIndex];
    }

    if (r) {
        if (pts) {
            *pts = r->PTS;
        }
        if (imageData) {
            *imageData = r->Data;
        }
        return GetResult::SUCCESS;
    }
    return GetResult::AGAIN;
}






