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

TorcCameraThread::TorcCameraThread(TorcCameraOutput *Parent, const QString &Type, const TorcCameraParams &Params)
  : TorcQThread("Camera"),
    m_parent(Parent),
    m_type(Type),
    m_params(Params),
    m_camera(NULL),
    m_cameraLock(QReadWriteLock::Recursive),
    m_stop(false)
{
    if (m_parent)
    {
        connect(this, SIGNAL(WritingStarted()), m_parent, SLOT(WritingStarted()));
        connect(this, SIGNAL(WritingStopped()), m_parent, SLOT(WritingStopped()));
    }
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
        if (m_parent)
        {
            connect(m_camera, SIGNAL(InitSegmentReady()),  m_parent, SLOT(InitSegmentReady()));
            connect(m_camera, SIGNAL(SegmentReady(int)),   m_parent, SLOT(SegmentReady(int)));
            connect(m_camera, SIGNAL(SegmentRemoved(int)), m_parent, SLOT(SegmentRemoved(int)));
            connect(m_camera, SIGNAL(SetErrored(bool)),    m_parent, SLOT(CameraErrored(bool)));
        }

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
