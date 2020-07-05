/*************************************************************************
*
* Copyright (C) 2016-2020 Mediator Software and/or its subsidiary(-ies).
* All rights reserved.
* Contact: Mediator Software (info@mediator-software.com)
*
* NOTICE:  All information contained herein is, and remains the property of
* Mediator Software and its suppliers, if any.
* The intellectual and technical concepts contained herein are proprietary
* to Mediator Software and its suppliers and may be covered by U.S. and
* Foreign Patents, patents in process, and are protected by trade secret or
* copyright law. Dissemination of this information or reproduction of this
* material is strictly forbidden unless prior written permission is obtained
* from Mediator Software.
*
* If you have questions regarding the use of this file, please contact
* Mediator Software (info@mediator-software.com).
*
***************************************************************************/

#include "IvryCustomTrackingApp.h"

IvryCustomTrackingApp::IvryCustomTrackingApp()
	: m_hQuitEvent(INVALID_HANDLE_VALUE)
	, m_bUseDeviceOrientation(true)
{
	// Start position 1m off the ground at origin
	m_afPosition[0] = m_afPosition[2] = 0;
	m_afPosition[1] = 1;
}

IvryCustomTrackingApp::~IvryCustomTrackingApp()
{
	if (m_hQuitEvent != INVALID_HANDLE_VALUE)
	{
		// Make sure event is signalled before deleting
		::SetEvent(m_hQuitEvent);
		::CloseHandle(m_hQuitEvent);
	}
}

/** Run tracker **/
DWORD IvryCustomTrackingApp::Run()
{
	DWORD result = ERROR_SUCCESS;

	// Open connection to driver
	if (Open())
	{
		// Create 'exiting' event
		m_hQuitEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		if (m_hQuitEvent != INVALID_HANDLE_VALUE)
		{
			// NOTE: in an external tracking process disabling device orientation and
			// enabling external tracking would normally be done once the external 
			// tracking had actually begun, to avoid no tracking being active

			for (int i = 0; i < retry; i++) {
				if (!success) {
					InitPSMoveService();
					Sleep(1000);
				}
				else {
					break;
				}
			}

			// Disable device orientation
			EnableDeviceOrientation(false);

			// Enable external tracking
			TrackingEnabled(true);

			// Wait for quit event
			::WaitForSingleObject(m_hQuitEvent, INFINITE);

			// Disable external tracking
			TrackingEnabled(false);

			// Enable device orientation
			EnableDeviceOrientation(true);
		}
		else
		{
			// Get last error code from Windows
			result = ::GetLastError();
		}

		// Close connection to driver
		Close();
	}
	else
	{
		// Get last error code from library
		result = this->GetLastError();
	}

	return result;
}

/** Pose has been recevied from driver **/
void IvryCustomTrackingApp::OnDevicePoseUpdated(const vr::DriverPose_t &pose)
{
	vr::DriverPose_t updatedPose = pose;

	// Not using device orientation?
	if (!m_bUseDeviceOrientation)
	{
		// Use tracker orientation
		updatedPose.qRotation = { 1, 0, 0, 0 };
	}

	if (success) {
		PSM_Update();
		if (hmdList.count > 0) {
			PSM_GetHmdPosition(hmdList.hmd_id[0], &hmdPose);
			m_afPosition[0] = (double)hmdPose.x * 0.01;
			m_afPosition[1] = (double)hmdPose.y * 0.01;
			m_afPosition[2] = (double)hmdPose.z * 0.01;
		}
	}

	// Use tracker position
	updatedPose.vecPosition[0] = m_afPosition[0];
	updatedPose.vecPosition[1] = m_afPosition[1];
	updatedPose.vecPosition[2] = m_afPosition[2];

	// Send tracker pose to driver
	PoseUpdated(updatedPose);
}

/** Device orientation has been enabled/disabled by user **/
void IvryCustomTrackingApp::OnDeviceOrientationEnabled(bool enable)
{
	m_bUseDeviceOrientation = enable;
}

/** Driver is requesting tracking process quit **/
void IvryCustomTrackingApp::OnQuit()
{
	if (m_hQuitEvent != INVALID_HANDLE_VALUE)
	{
		if (hmdList.count > 0)
		{
			PSM_StopHmdDataStream(hmdList.hmd_id[0], PSM_DEFAULT_TIMEOUT);
			PSM_FreeHmdListener(hmdList.hmd_id[0]);
		}
		PSM_Shutdown();

		// Signal that Run() can exit
		::SetEvent(m_hQuitEvent);
	}

	LogMessage("Shutting down\n");
}

void IvryCustomTrackingApp::InitPSMoveService() {
	success = true;
	if (PSM_Initialize(PSMOVESERVICE_DEFAULT_ADDRESS, PSMOVESERVICE_DEFAULT_PORT, PSM_DEFAULT_TIMEOUT) != PSMResult_Success)
	{
		LogMessage("Failed to initialize the client network manager");
		success = false;
	}
	if (success)
	{
		memset(&hmdList, 0, sizeof(PSMHmdList));
		PSM_GetHmdList(&hmdList, PSM_DEFAULT_TIMEOUT);

		// Register as listener and start streams
		unsigned int data_stream_flags = PSMControllerDataStreamFlags::PSMStreamFlags_includePositionData;

		//HMD
		if (hmdList.count > 0) {
			if (PSM_AllocateHmdListener(hmdList.hmd_id[0]) != PSMResult_Success) {
				LogMessage("Failed to AllocateHmdListener");
				success = false;
			}
				
			if (PSM_StartHmdDataStream(hmdList.hmd_id[0], data_stream_flags, PSM_DEFAULT_TIMEOUT) != PSMResult_Success) {
				LogMessage("Failed to StartHmdDataStream");
				success = false;
			}
		}
	}
}

