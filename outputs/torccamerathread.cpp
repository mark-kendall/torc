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
                LOG(VB_GENERAL, LOG_ERR, QString("Mismatch between thread and type for '%1'").arg(Type));

            thread->DownRef();
            // release our own ref
            if (!thread->IsShared())
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Removing camera thread - no longer in use '%1'").arg(Type));
                thread->DownRef();
                threads.remove(Type);
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Unknown camera thread type '%1'").arg(Type));
        }
    }
    else
    {
        TorcCameraThread* thread = NULL;
        if (threads.contains(Type))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Sharing existing camera thread '%1'").arg(Type));
            thread = threads.value(Type);
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Creating shared camera thread '%1'").arg(Type));
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
  : TorcQThread("Camera"),
    m_type(Type),
    m_params(Params),
    m_camera(NULL),
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
    connect(Parent, SIGNAL(StreamVideo(bool)), this, SIGNAL(StreamVideo(bool)));
    connect(this, SIGNAL(WritingStarted()),    Parent, SLOT(WritingStarted()));
    connect(this, SIGNAL(WritingStopped()),    Parent, SLOT(WritingStopped()));
    connect(this, SIGNAL(InitSegmentReady()),  Parent, SLOT(InitSegmentReady()));
    connect(this, SIGNAL(SegmentReady(int)),   Parent, SLOT(SegmentReady(int)));
    connect(this, SIGNAL(SegmentRemoved(int)), Parent, SLOT(SegmentRemoved(int)));
    connect(this, SIGNAL(CameraErrored(bool)), Parent, SLOT(CameraErrored(bool)));
    connect(this, SIGNAL(ParamsChanged(TorcCameraParams)), Parent, SLOT(ParamsChanged(TorcCameraParams)));
}

/*! \brief Connect camera signals for stills capture.
 */
void TorcCameraThread::SetStillsParent(TorcCameraStillsOutput *Parent)
{
    if (!Parent)
        return;

    QWriteLocker locker(&m_cameraLock);
    connect(this, SIGNAL(CameraErrored(bool)), Parent, SLOT(CameraErrored(bool)));
    connect(this, SIGNAL(StillReady(QString)), Parent, SLOT(StillReady(QString)));
    connect(this, SIGNAL(ParamsChanged(TorcCameraParams)), Parent, SLOT(ParamsChanged(TorcCameraParams)));
    connect(Parent, SIGNAL(TakeStills(uint)), this, SIGNAL(TakeStills(uint)));
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
            QObject* object = dynamic_cast<QObject*>(this);
            if (object)
            {
                object->deleteLater();
                return true;
            }
        }

        delete this;
        return true;
    }

    return false;
}

void TorcCameraThread::Start(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Camera thread starting");

    QWriteLocker locker(&m_cameraLock);
    m_camera = TorcCameraFactory::GetCamera(m_type, m_params);
    if (!m_camera)
    {
        quit();
        return;
    }

    // outbound streaming signals
    connect(m_camera, SIGNAL(WritingStarted()),    this, SIGNAL(WritingStarted()));
    connect(m_camera, SIGNAL(WritingStopped()),    this, SIGNAL(WritingStopped()));
    connect(m_camera, SIGNAL(InitSegmentReady()),  this, SIGNAL(InitSegmentReady()));
    connect(m_camera, SIGNAL(SegmentReady(int)),   this, SIGNAL(SegmentReady(int)));
    connect(m_camera, SIGNAL(SegmentRemoved(int)), this, SIGNAL(SegmentRemoved(int)));
    connect(m_camera, SIGNAL(SetErrored(bool)),    this, SIGNAL(CameraErrored(bool)));
    connect(m_camera, SIGNAL(ParametersChanged(TorcCameraParams)), this, SIGNAL(ParamsChanged(TorcCameraParams)));
    // inbound streaming signals
    connect(this, SIGNAL(StreamVideo(bool)), m_camera, SLOT(StreamVideo(bool)));
    // outbound stills signals
    connect(m_camera, SIGNAL(StillReady(QString)), this, SIGNAL(StillReady(QString)));
    // inbound stills signals
    connect(this, SIGNAL(TakeStills(uint)), m_camera, SLOT(TakeStills(uint)));

    if (!m_camera->Setup() || !m_camera->Start())
        quit();
}

void TorcCameraThread::Finish(void)
{
    if (m_camera)
    {
        m_camera->Stop();
        delete m_camera;
        m_camera = NULL;
    }

    LOG(VB_GENERAL, LOG_INFO, "Camera thread stopping");
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

