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
    , m_InformationString(source->GetInformation()) 
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
                m_ErrorString = m_LiveInput->GetLastError();
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
        m_ErrorString = m_LiveInput->GetLastError();
        m_InformationString = m_LiveInput->GetInformation();
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
    , m_ErrorState(ErrorState::NO_ERROR)
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
        m_ErrorState = ErrorState::INPUT_EXHAUSTED;
        m_Error = "ERROR input exhausted";
        return false;
    }
    return true;
}

void CVVideoCaptureSource::Reopen() {
    ClearError();
    m_Capture = std::make_unique<cv::VideoCapture>(m_Input);
    if (!m_Capture->isOpened()) {
        m_ErrorState = ErrorState::CAN_NOT_OPEN_SOURCE;
        m_Error = "ERROR unable to open camera";
    }

    m_OnFirstFrame = true;
    ReadNextFrame();
    m_Width = m_Frame.cols;
    m_Height = m_Frame.rows;
    m_NumBytes = m_Width * m_Height * 3;
}

void CVVideoCaptureSource::ClearError() {
    m_ErrorState = ErrorState::NO_ERROR;
    m_Error = "";
}

ErrorState CVVideoCaptureSource::GetErrorState() {
    return m_ErrorState;
}

std::string CVVideoCaptureSource::GetLastError() {
    return m_Error;
}

std::string CVVideoCaptureSource::GetInformation() {
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
    , m_CurrentKnownNumFrames(0)
    , m_MaxRecords(0)
{
    if (!m_Source) {
        throw std::invalid_argument("must provide source");
    }

    int imSize = ImageDataBufferSize();
    if (imSize > 0) {
        m_MaxRecords = static_cast<int>(m_Config.BufferSize / static_cast<size_t>(imSize));
        m_MaxRecords += 1;
        m_Data.resize(m_MaxRecords * imSize);
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

bool StaticVideoBuffer::BufferFull() const {
    return m_Records.size() == m_MaxRecords;
}

bool StaticVideoBuffer::MustRewind() const {
    if (m_Records.empty()) {
        return false;
    }
    return m_TargetFrameIndex < m_Records.front().FrameIndex;
}
bool StaticVideoBuffer::MustAdvance() const {
    assert(BufferFull());
    return (m_TargetFrameIndex - m_Records.front().FrameIndex) > 
            ((m_Records.back().FrameIndex - m_Records.front().FrameIndex) * m_Config.ForwardBias);
}

bool StaticVideoBuffer::HasWork() const {
    if (MustRewind()) {
        return true;
    }
    if ((HasError() && GetErrorState() != ErrorState::INPUT_EXHAUSTED) || m_MaxRecords == 0) {
        return false;
    }
    if (!BufferFull()) {
        return true;
    }
    if (MustAdvance()) {
        return true;
    }

    return false;
}

void StaticVideoBuffer::DoWork(int64_t* frameIndex, int64_t* pts) {
    if (frameIndex) {
        *frameIndex = -1;
    }
    if (m_MaxRecords == 0) {
        return;
    }
    Record newRec;
    newRec.FrameIndex = m_SourceFrameIndex;
    newRec.Data = nullptr;

    if (MustRewind()) {
        newRec.Data = m_Data.data();
        m_Source->ClearError();
        m_Source->Reopen();
        m_SourceFrameIndex = 0;
        newRec.FrameIndex = 0;
        m_Records.clear();
    }

    if ((HasError() && GetErrorState() != ErrorState::INPUT_EXHAUSTED)) {
        return;
    }

    if (!BufferFull()) {
        newRec.Data = m_Data.data() + ImageDataBufferSize() * m_Records.size();
    } else if (MustAdvance()) {
        newRec.Data = m_Records.front().Data;
        m_Records.pop_front();
    } 

    if (!BufferFull() && newRec.Data) {
        if (m_Source->Get(newRec.Data, &newRec.PTS) == GetResult::SUCCESS) {
            m_SourceFrameIndex++;
            m_Records.push_back(newRec);
            if (m_SourceFrameIndex > m_CurrentKnownNumFrames) {
                m_CurrentKnownNumFrames = m_SourceFrameIndex;
            }

            if (frameIndex) {
                *frameIndex = newRec.FrameIndex;
            }
            if (pts) {
                *pts = newRec.PTS;
            }
        }
    }
}

bool StaticVideoBuffer::RecordIndex(int64_t frameIndex, 
        const std::deque<Record>& records, size_t* recordIndex) const {
    if (!records.empty() &&
        frameIndex >= records.front().FrameIndex && frameIndex <= records.back().FrameIndex) {

        if (recordIndex) {
            *recordIndex = static_cast<size_t>(frameIndex - records.front().FrameIndex);
        }
        return true;
    }
    return false;
}

bool StaticVideoBuffer::HasFrame(int64_t frameIndex) const {
    return RecordIndex(frameIndex, m_Records);
}

GetResult StaticVideoBuffer::GetFrame(int64_t frameIndex, int64_t* pts, const uint8_t** imageData) const {
    if (frameIndex < 0) {
        return GetResult::FAILURE;
    }
    m_TargetFrameIndex = frameIndex;
    if (HasError() && GetErrorState() != ErrorState::INPUT_EXHAUSTED) {
        return GetResult::FAILURE;
    }

    size_t recordIndex;
    const Record* r = nullptr;
    if (RecordIndex(frameIndex, m_Records, &recordIndex)) {
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

////////////////////////////////////////////////////////////////////////////////

StaticVideoThreadConfig StaticVideoThreadConfig::Defaults() {
    StaticVideoThreadConfig cfg;
    cfg.StaticVideoBufferCfg = StaticVideoBufferConfig::Defaults();
    return cfg;
}

StaticVideoThread::StaticVideoThread(IVideoSourcePtr source,
        StaticVideoThreadConfig config)
    : m_Config(config)
    , m_Buffer(std::move(source), config.StaticVideoBufferCfg)
    , m_ShouldStop(false)
    , m_HasError(false)
    , m_CurrentKnownNumFrames(0)
{
    m_BufferThread = std::thread(
            &StaticVideoThread::BufferThread, this);
}

StaticVideoThread::~StaticVideoThread() {
    m_ShouldStop = true;
    m_BufferThread.join();
}

bool StaticVideoThread::HasError() const {
    return m_HasError;
}

ErrorState StaticVideoThread::GetErrorState() const {
    return m_ErrorState;
}

std::string StaticVideoThread::GetError() const {
    std::lock_guard<std::mutex> lock(m_BufferMutex);
    return m_ErrorString;
}

std::string StaticVideoThread::GetInputInformation() const {
    std::lock_guard<std::mutex> lock(m_BufferMutex);
    return m_InformationString;
}

void StaticVideoThread::BufferThread() {
    while (!m_ShouldStop) {
        if (m_Buffer.HasWork()) {
            {
                std::lock_guard<std::mutex> lock(m_BufferMutex);
                int64_t frameIndex, pts;
                m_Buffer.DoWork(&frameIndex, &pts);
                if (frameIndex > static_cast<int>(m_PTS.size())) {
                    m_PTS.push_back(pts);
                    m_CurrentKnownNumFrames = m_PTS.size();
                }
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

int64_t StaticVideoThread::CurrentKnownNumFrames() const {
    return m_CurrentKnownNumFrames;
}

LiveInputFramePtr StaticVideoThread::GetFrame(int frameIndex) {
    std::lock_guard<std::mutex> lock(m_BufferMutex);

    int64_t pts;
    const uint8_t* data;
    if (m_Buffer.GetFrame(frameIndex, &pts, &data) == GetResult::SUCCESS) {
        LiveInputFramePtr f = std::make_shared<LiveInputFrame>(
                m_Buffer.Width(), m_Buffer.Height());
        f->FrameNumber = frameIndex;
        f->PtsMilliseconds = pts;
        std::memcpy(f->Buffer, data, m_Buffer.ImageDataBufferSize());
        return f;
    }
    return nullptr;
}

LiveInputFramePtr StaticVideoThread::GetFramePts(int64_t pts) {
    int targetFrame = 0;
    {
        std::lock_guard<std::mutex> lock(m_BufferMutex);
        auto it = std::lower_bound(m_PTS.begin(), m_PTS.end(), pts);
        if (it == m_PTS.end()) {
            it--;
        }
        if (it != m_PTS.begin()) {
            int64_t a = *it;
            int64_t b = *std::prev(it);
            if (std::abs(pts - b) < std::abs(pts - a)) {
                it--;
            }
        }
        targetFrame = std::distance(m_PTS.begin(), it);
    }

    return GetFrame(targetFrame);
}

void StaticVideoThread::UpdatePTS(std::vector<int64_t>* pts) {
    // TODO
    std::lock_guard<std::mutex> lock(m_BufferMutex);
    *pts = m_PTS;
}





