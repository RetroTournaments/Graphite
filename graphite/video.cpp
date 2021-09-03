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

#include "graphite/video.h"

using namespace graphite::video;


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
                queueFull = static_cast<int>(m_Queue.size()) >= m_QueueCapacity;
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

OpenCVInput::OpenCVInput(const std::string& input)
    : m_Input(input)
{
    Reopen();
}

OpenCVInput::~OpenCVInput()
{
}

int OpenCVInput::Width() const {
    return m_Width;
}

int OpenCVInput::Height() const {
    return m_Height;
}


LiveGetResult OpenCVInput::Get(uint8_t* buffer, int64_t* ptsMilliseconds) {
    if (!m_Error.empty()) {
        return LiveGetResult::FAILURE;
    }

    if (!m_OnFirstFrame) {
        if (!ReadNextFrame()) {
            return LiveGetResult::FAILURE;
        }
    }

    m_OnFirstFrame = false;
    std::memcpy(buffer, m_Frame.data, m_NumBytes);
    *ptsMilliseconds = static_cast<int64_t>(std::round(m_PTS));
    return LiveGetResult::SUCCESS;
}

bool OpenCVInput::ReadNextFrame() {
    m_Capture->read(m_Frame);
    m_PTS = m_Capture->get(cv::CAP_PROP_POS_MSEC);
    if (m_Frame.empty()) {
        m_Error = "ERROR empty frame";
        return false;
    }
    return true;
}

void OpenCVInput::Reopen() {
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

void OpenCVInput::ClearError() {
    m_Error = "";
}

std::string OpenCVInput::LastError() {
    return m_Error;
}

std::string OpenCVInput::Information() {
    return m_Input;
}
