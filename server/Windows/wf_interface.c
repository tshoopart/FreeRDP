/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Windows Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <winpr/tchar.h>
#include <winpr/windows.h>
#include <freerdp/utils/tcp.h>

#include "wf_peer.h"
#include "wf_settings.h"
#include "wf_info.h"

#include "wf_interface.h"

//todo: remove this
struct rdp_listener
{
	freerdp_listener* instance;

 	int sockfds[5];
	int num_sockfds;
};
//

DWORD WINAPI wf_server_main_loop(LPVOID lpParam)
{
	int i, fds;
	int rcount;
	int max_fds;
	void* rfds[32];
	fd_set rfds_set;
	freerdp_listener* instance;
	struct rdp_listener* listener;
	wfInfo* wfi;

	wfi = wf_info_get_instance();
	wfi->force_all_disconnect = FALSE;

	ZeroMemory(rfds, sizeof(rfds));
	instance = (freerdp_listener*) lpParam;
	listener = (struct rdp_listener*) instance->listener;

	while (listener->num_sockfds > 0)
	{
		rcount = 0;

		if (instance->GetFileDescriptor(instance, rfds, &rcount) != true)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}

		max_fds = 0;
		FD_ZERO(&rfds_set);

		for (i = 0; i < rcount; i++)
		{
			fds = (int)(long)(rfds[i]);

			if (fds > max_fds)
				max_fds = fds;

			FD_SET(fds, &rfds_set);
		}

		if (max_fds == 0)
			break;

		
		select(max_fds + 1, &rfds_set, NULL, NULL, NULL);
		
		if (instance->CheckFileDescriptor(instance) != true)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
	}

	printf("wf_server_main_loop terminating\n");

	instance->Close(instance);

	return 0;
}

BOOL wfreerdp_server_start(wfServer* server)
{
	freerdp_listener* instance;

	server->instance = freerdp_listener_new();
	server->instance->PeerAccepted = wf_peer_accepted;
	instance = server->instance;

	wf_settings_read_dword(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Server"), _T("DefaultPort"), &server->port);

	if (instance->Open(instance, NULL, (uint16) server->port))
	{
		server->thread = CreateThread(NULL, 0, wf_server_main_loop, (void*) instance, 0, NULL);
		return TRUE;
	}

	return FALSE;
}

BOOL wfreerdp_server_stop(wfServer* server)
{
	wfInfo* wfi;

	wfi = wf_info_get_instance();

	printf("Stopping server\n");
	wfi->force_all_disconnect = TRUE;
	server->instance->Close(server->instance);
	return TRUE;
}

wfServer* wfreerdp_server_new()
{
	wfServer* server;

	server = (wfServer*) malloc(sizeof(wfServer));
	ZeroMemory(server, sizeof(wfServer));

	freerdp_wsa_startup();

	if (server)
	{
		server->port = 3389;
	}

	return server;
}

void wfreerdp_server_free(wfServer* server)
{
	if (server)
	{
		free(server);
	}

	freerdp_wsa_cleanup();
}


FREERDP_API BOOL wfreerdp_server_is_running(wfServer* server)
{
	DWORD tStatus;
	BOOL bRet;

	bRet = GetExitCodeThread(server->thread, &tStatus);
	if (bRet == 0)
	{
		printf("Error in call to GetExitCodeThread\n");
		return FALSE;
	}

	if (tStatus == STILL_ACTIVE)
		return TRUE;
	return FALSE;
}

FREERDP_API UINT32 wfreerdp_server_num_peers()
{
	wfInfo* wfi;

	wfi = wf_info_get_instance();
	return wfi->peerCount;
}

FREERDP_API UINT32 wfreerdp_server_get_peer_hostname(int pId, wchar_t * dstStr)
{
	wfInfo* wfi;
	freerdp_peer* peer;

	wfi = wf_info_get_instance();
	peer = wfi->peers[pId];

	
	if (peer)
	{
		UINT32 sLen;

		sLen = strnlen_s(peer->hostname, 50);
		swprintf(dstStr, 50, L"%hs", peer->hostname);
		return sLen;
	}
	else
	{
		printf("nonexistent peer\n");
		return 0;
	}

}
