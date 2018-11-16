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

TorcCameraThread::TorcCameraThread( const QString &Type, const TorcCameraParams &Params)
  : TorcQThread("Camera"),
    m_parent(NULL),
    m_type(Type),
    m_params(Params),
    m_camera(NULL),
    m_cameraLock(QReadWriteLock::Recursive),
    m_stop(false)
{
}

TorcCameraThread::~TorcCameraThread()
{
    QWriteLocker locker(&m_cameraLock);
    if (m_camera)
    {
        delete m_camera;
        m_camera = NULL;
    }
}

void TorcCameraThread::SetParent(TorcCameraOutput *Parent)
{
    if (!Parent)
        return;

    QWriteLocker locker(&m_cameraLock);
    if (m_parent)
        LOG(VB_GENERAL, LOG_WARNING, "Setting thread parent but already have one...");

    m_parent = Parent;
    connect(this, SIGNAL(WritingStarted()),    m_parent, SLOT(WritingStarted()));
    connect(this, SIGNAL(WritingStopped()),    m_parent, SLOT(WritingStopped()));
    connect(this, SIGNAL(InitSegmentReady()),  m_parent, SLOT(InitSegmentReady()));
    connect(this, SIGNAL(SegmentReady(int)),   m_parent, SLOT(SegmentReady()));
    connect(this, SIGNAL(SegmentRemoved(int)), m_parent, SLOT(SegmentRemoved(int)));
    connect(this, SIGNAL(CameraErrored(bool)), m_parent, SLOT(CameraErrored(bool)));
}

/*! \brief Decrement the reference count for this thread.
 *
 * We cannot use the default DownRef implementatin as we need to stop the thread
 * before deleting it.
*/
bool TorcCameraThread::DownRef(void)
{
    if (!m_refCount.deref())
    {
        StopWriting();
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

void TorcCameraThread::StopWriting(void)
{
    m_stop = true;
}

void TorcCameraThread::Start(void)
{
    if (m_stop)
        return;

    LOG(VB_GENERAL, LOG_INFO, "Camera thread starting");

    m_cameraLock.lockForWrite();
    m_camera = TorcCameraFactory::GetCamera(m_type, m_params);

    if (m_camera)
    {
        connect(m_camera, SIGNAL(InitSegmentReady()),  this, SIGNAL(InitSegmentReady()));
        connect(m_camera, SIGNAL(SegmentReady(int)),   this, SIGNAL(SegmentReady(int)));
        connect(m_camera, SIGNAL(SegmentRemoved(int)), this, SIGNAL(SegmentRemoved(int)));
        connect(m_camera, SIGNAL(SetErrored(bool)),    this, SIGNAL(CameraErrored(bool)));

        if (m_camera->Setup())
        {
            m_cameraLock.unlock();
            emit WritingStarted();
            m_cameraLock.lockForRead();
            while (!m_stop)
            {
                if (!m_camera->WriteFrame())
                {
                    LOG(VB_GENERAL, LOG_WARNING, "Camera returned error from WriteFrame()");
                    break;
                }
            }
            m_cameraLock.unlock();
            m_cameraLock.lockForWrite();
            m_camera->Stop();
        }

        m_camera->disconnect();
        delete m_camera;
        m_camera = NULL;
    }

    m_cameraLock.unlock();

    LOG(VB_GENERAL, LOG_INFO, "Camera thread stopping");
    emit WritingStopped();
}

void TorcCameraThread::Finish(void)
{
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

TorcCameraParams TorcCameraThread::GetParams(void)
{
    QReadLocker locker(&m_cameraLock);
    if (m_camera)
        return m_camera->GetParams();
    return TorcCameraParams();
}
