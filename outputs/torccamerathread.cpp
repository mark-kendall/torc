/* Class TorcCameraThread
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2018
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torclogging.h"
#include "torccameraoutput.h"
#include "torccamerathread.h"

/*! \brief Create and release shared camera threads/devices.
 *
 * A TorcCamera device can be configured to stream video and/or capture still images. Each role requires different for control
 * and to avoid breaking the 'single input to an output' rule, we create different output devices for stills and video. These can then
 * present their own methods externally and have custom controls/logic to handle their state. These output devices then share
 * the camera thread by way of custom referencing counting.
 *
 * This static method encapsulates all camera thread creation, sharing and destruction without needing any external state.
*/
void TorcCameraThread::CreateOrDestroy(TorcCameraThread *&Thread, const QString &Type, const TorcCameraParams &Params)
{
    static QMutex lock(QMutex::NonRecursive);
    static QMutexLocker locker(&lock);
    static QMap<QString,TorcCameraThread*> threads;

    if (Thread)
    {
        if (threads.contains(Type))
        {
            TorcCameraThread* thread = threads.value(Type);
            if (thread != Thread)
                LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Mismatch between thread and type for '%1'").arg(Type));

            thread->DownRef();
            // release our own ref
            if (!thread->IsShared())
            {
                LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Removing camera thread - no longer in use '%1'").arg(Type));
                thread->DownRef();
                threads.remove(Type);
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Unknown camera thread type '%1'").arg(Type));
        }
    }
    else
    {
        TorcCameraThread* thread = nullptr;
        if (threads.contains(Type))
        {
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Sharing existing camera thread '%1'").arg(Type));
            thread = threads.value(Type);
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Creating shared camera thread '%1'").arg(Type));
            thread = new TorcCameraThread(Type, Params);
            threads.insert(Type, thread);
        }
        // as creator, we hold one reference. Add another for the requestor.
        thread->UpRef();
        Thread = thread;
    }
}

/*! \brief A custom thread to run TorcCameraDevice's
 */
TorcCameraThread::TorcCameraThread( const QString &Type, const TorcCameraParams &Params)
  : TorcQThread(QStringLiteral("Camera")),
    m_type(Type),
    m_params(Params),
    m_camera(nullptr),
    m_cameraLock(QReadWriteLock::Recursive)
{
}

TorcCameraThread::~TorcCameraThread()
{
    delete m_camera;
}

/*! \brief Connect camera signals for video streaming to the stream output device.
 */
void TorcCameraThread::SetVideoParent(TorcCameraVideoOutput *Parent)
{
    if (!Parent)
        return;

    QWriteLocker locker(&m_cameraLock);
    connect(Parent, &TorcCameraVideoOutput::StreamVideo, this, &TorcCameraThread::StreamVideo);
    connect(this, &TorcCameraThread::WritingStarted,     Parent, &TorcCameraVideoOutput::WritingStarted);
    connect(this, &TorcCameraThread::WritingStopped,     Parent, &TorcCameraVideoOutput::WritingStopped);
    connect(this, &TorcCameraThread::InitSegmentReady,   Parent, &TorcCameraVideoOutput::InitSegmentReady);
    connect(this, &TorcCameraThread::SegmentReady,       Parent, &TorcCameraVideoOutput::SegmentReady);
    connect(this, &TorcCameraThread::SegmentRemoved,     Parent, &TorcCameraVideoOutput::SegmentRemoved);
    connect(this, &TorcCameraThread::CameraErrored,      Parent, &TorcCameraVideoOutput::CameraErrored);
    connect(this, &TorcCameraThread::ParamsChanged,      Parent, &TorcCameraVideoOutput::ParamsChanged);
}

/*! \brief Connect camera signals for stills capture.
 */
void TorcCameraThread::SetStillsParent(TorcCameraStillsOutput *Parent)
{
    if (!Parent)
        return;

    QWriteLocker locker(&m_cameraLock);
    connect(this, &TorcCameraThread::CameraErrored,      Parent, &TorcCameraStillsOutput::CameraErrored);
    connect(this, &TorcCameraThread::StillReady,         Parent, &TorcCameraStillsOutput::StillReady);
    connect(this, &TorcCameraThread::ParamsChanged,      Parent, &TorcCameraStillsOutput::ParamsChanged);
    connect(Parent, &TorcCameraStillsOutput::TakeStills, this,   &TorcCameraThread::TakeStills);
}

/*! \brief Decrement the reference count for this thread.
 *
 * We cannot use the default DownRef implementation as we need to stop the thread
 * before deleting it.
*/
bool TorcCameraThread::DownRef(void)
{
    if (!m_refCount.deref())
    {
        quit();
        wait();
        if (!m_eventLoopEnding)
        {
            deleteLater();
            return true;
        }

        delete this;
        return true;
    }

    return false;
}

void TorcCameraThread::Start(void)
{
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Camera thread starting"));

    QWriteLocker locker(&m_cameraLock);
    m_camera = TorcCameraFactory::GetCamera(m_type, m_params);
    if (!m_camera)
    {
        quit();
        return;
    }

    // outbound streaming signals
    connect(m_camera, &TorcCameraDevice::WritingStarted,    this, &TorcCameraThread::WritingStarted);
    connect(m_camera, &TorcCameraDevice::WritingStopped,    this, &TorcCameraThread::WritingStopped);
    connect(m_camera, &TorcCameraDevice::InitSegmentReady,  this, &TorcCameraThread::InitSegmentReady);
    connect(m_camera, &TorcCameraDevice::SegmentReady,      this, &TorcCameraThread::SegmentReady);
    connect(m_camera, &TorcCameraDevice::SegmentRemoved,    this, &TorcCameraThread::SegmentRemoved);
    connect(m_camera, &TorcCameraDevice::SetErrored,        this, &TorcCameraThread::CameraErrored);
    connect(m_camera, &TorcCameraDevice::ParametersChanged, this, &TorcCameraThread::ParamsChanged);
    // inbound streaming signals
    connect(this, &TorcCameraThread::StreamVideo, m_camera, &TorcCameraDevice::StreamVideo);
    // outbound stills signals
    connect(m_camera, &TorcCameraDevice::StillReady, this,  &TorcCameraThread::StillReady);
    // inbound stills signals
    connect(this, &TorcCameraThread::TakeStills, m_camera,  &TorcCameraDevice::TakeStills);

    if (!m_camera->Setup() || !m_camera->Start())
        quit();
}

void TorcCameraThread::Finish(void)
{
    if (m_camera)
    {
        m_camera->Stop();
        delete m_camera;
        m_camera = nullptr;
    }

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Camera thread stopping"));
}

QByteArray TorcCameraThread::GetSegment(int Segment)
{
    QReadLocker locker(&m_cameraLock);
    if (m_camera)
        return m_camera->GetSegment(Segment);
    return QByteArray();
}

QByteArray TorcCameraThread::GetInitSegment(void)
{
    QReadLocker locker(&m_cameraLock);
    if (m_camera)
        return m_camera->GetInitSegment();
    return QByteArray();
}

