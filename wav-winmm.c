/*
 * Copyright (c) 2012 Toni Spets <toni.spets@iki.fi>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <windows.h>
#include <stdio.h>
#include <dirent.h>
#include "player.h"

#define MAGIC_DEVICEID 0xBEEF
#define MAX_TRACKS 32

int playing = 0;
HANDLE player = NULL;

int player_main()
{
    while (playing)
    {
        while (playing)
        {
            int ret = plr_pump();
            if (!ret)
            {
                playing = 0;
            }
        }
    }

    player = NULL;

    return 0;
}

MCIERROR WINAPI (*real_mciSendCommandA)(MCIDEVICEID, UINT, DWORD_PTR, DWORD_PTR);

int lastTrack = 0;

#ifdef _DEBUG
    #define dprintf(...) if (fh) { fprintf(fh, __VA_ARGS__); fflush(NULL); }
    FILE *fh = NULL;
#else
    #define dprintf(...)
#endif

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
#ifdef _DEBUG
        fh = fopen("winmm.txt", "w");
#endif

        dprintf("ogg-winmm searching tracks...\r\n");

        HMODULE hMod = GetModuleHandle("system32\\winmm.dll");
        real_mciSendCommandA = (void *)GetProcAddress(hMod, "mciSendCommandA");

        DIR *dp = opendir("./MUSIC");
        struct dirent *ep;
        if (dp)
        {
            while (ep = readdir(dp))
            {
                int track;
                if (sscanf(ep->d_name, "Track%02d.ogg", &track) == 1 && strstr(ep->d_name, ".ogg"))
                {
                    if (track > lastTrack)
                        lastTrack = track;

                    dprintf("Track %02d: %s\r\n", track, ep->d_name);
                }
            }
            closedir(dp);
        }

        dprintf("Emulating total of %d CD tracks.\r\n\r\n", lastTrack);
    }

#ifdef _DEBUG
    if (fdwReason == DLL_PROCESS_DETACH)
    {
        if (fh)
        {
            fclose(fh);
            fh = NULL;
        }
    }
#endif

    return TRUE;
}

MCIERROR WINAPI fake_mciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
    char cmdbuf[1024];

    dprintf("mciSendCommandA(IDDevice=%p, uMsg=%p, fdwCommand=%p, dwParam=%p)\r\n", IDDevice, uMsg, fdwCommand, dwParam);

    if (fdwCommand & MCI_NOTIFY)
    {
        dprintf("  MCI_NOTIFY\r\n");
    }

    if (fdwCommand & MCI_WAIT)
    {
        dprintf("  MCI_WAIT\r\n");
    }

    if (uMsg == MCI_OPEN)
    {
        LPMCI_OPEN_PARMS parms = (LPVOID)dwParam;

        dprintf("  MCI_OPEN\r\n");

        if (fdwCommand & MCI_OPEN_ALIAS)
        {
            dprintf("    MCI_OPEN_ALIAS\r\n");
        }

        if (fdwCommand & MCI_OPEN_SHAREABLE)
        {
            dprintf("    MCI_OPEN_SHAREABLE\r\n");
        }

        if (fdwCommand & MCI_OPEN_TYPE_ID)
        {
            dprintf("    MCI_OPEN_TYPE_ID\r\n");

            if (LOWORD(parms->lpstrDeviceType) == MCI_DEVTYPE_CD_AUDIO)
            {
                dprintf("  Returning magic device id for MCI_DEVTYPE_CD_AUDIO\r\n");
                parms->wDeviceID = MAGIC_DEVICEID;
                return 0;
            }
        }

        if (fdwCommand & MCI_OPEN_TYPE && !(fdwCommand & MCI_OPEN_TYPE_ID))
        {
            dprintf("    MCI_OPEN_TYPE\r\n");
            dprintf("        -> %s\r\n", parms->lpstrDeviceType);

            if (strcmp(parms->lpstrDeviceType, "cdaudio") == 0)
            {
                dprintf("  Returning magic device id for MCI_DEVTYPE_CD_AUDIO\r\n");
                parms->wDeviceID = MAGIC_DEVICEID;
                return 0;
            }
        }

    }

    if (IDDevice == MAGIC_DEVICEID || IDDevice == 0)
    {
        if (uMsg == MCI_SET)
        {
            dprintf("  MCI_SET\r\n");

            return 0;
        }

        if (uMsg == MCI_CLOSE)
        {
            dprintf("  MCI_CLOSE\r\n");

            playing = 0;
            if (player)
            {
                WaitForSingleObject(player, INFINITE);
            }

            return 0;
        }

        if (uMsg == MCI_PLAY)
        {
            LPMCI_PLAY_PARMS parms = (LPVOID)dwParam;

            dprintf("  MCI_PLAY\r\n");

            if (fdwCommand & MCI_FROM)
            {
                dprintf("    dwFrom: %d\r\n", parms->dwFrom);

                int track = parms->dwFrom;
                if (track > lastTrack)
                    track = lastTrack;

                snprintf(cmdbuf, sizeof cmdbuf, "MUSIC\\Track%02d.ogg", track);
                int ret = plr_play(cmdbuf);
                playing = 1;
                player = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)player_main, NULL, 0, NULL);
            }

            if (fdwCommand & MCI_TO)
            {
                dprintf("    dwTo:   %d\r\n", parms->dwTo);
            }

            return 0;
        }

        if (uMsg == MCI_STOP)
        {
            dprintf("  MCI_STOP\r\n");

            playing = 0;
            if (player)
            {
                WaitForSingleObject(player, INFINITE);
            }

            return 0;
        }

        if (uMsg == MCI_STATUS)
        {
            LPMCI_STATUS_PARMS parms = (LPVOID)dwParam;

            dprintf("  MCI_STATUS\r\n");

            parms->dwReturn = 0;

            if (parms->dwItem == MCI_STATUS_ITEM)
            {
                dprintf("    MCI_STATUS_ITEM\r\n");
            }

            if (parms->dwItem == MCI_STATUS_CURRENT_TRACK)
            {
                dprintf("      MCI_STATUS_CURRENT_TRACK\r\n");
            }

            if (parms->dwItem == MCI_STATUS_LENGTH)
            {
                dprintf("      MCI_STATUS_LENGTH\r\n");
            }

            if (parms->dwItem == MCI_STATUS_NUMBER_OF_TRACKS)
            {
                dprintf("      MCI_STATUS_NUMBER_OF_TRACKS\r\n");
                parms->dwReturn = 9;
            }

            if (parms->dwItem == MCI_STATUS_POSITION)
            {
                dprintf("      MCI_STATUS_POSITION\r\n");
            }

            if (parms->dwItem == MCI_STATUS_MODE)
            {
                dprintf("      MCI_STATUS_MODE\r\n");

                parms->dwReturn = playing ? MCI_MODE_PLAY : MCI_MODE_STOP;
            }

            if (parms->dwItem == MCI_STATUS_MEDIA_PRESENT)
            {
                dprintf("      MCI_STATUS_MEDIA_PRESENT\r\n");
                parms->dwReturn = lastTrack > 0;
            }

            if (parms->dwItem == MCI_CDA_STATUS_TYPE_TRACK)
            {
                dprintf("      MCI_CDA_STATUS_TYPE_TRACK\r\n");
            }

            if (parms->dwItem == MCI_STATUS_READY)
            {
                dprintf("      MCI_STATUS_READY\r\n");
            }

            if (parms->dwItem == MCI_STATUS_TIME_FORMAT)
            {
                dprintf("      MCI_STATUS_TIME_FORMAT\r\n");
            }

            if (parms->dwItem == MCI_STATUS_START)
            {
                dprintf("      MCI_STATUS_START\r\n");
            }

            if (parms->dwItem == MCI_TRACK)
            {
                dprintf("    MCI_TRACK\r\n");
            }

            dprintf("  dwReturn %d\n", parms->dwReturn);

            return 0;
        }

        return MCIERR_UNRECOGNIZED_COMMAND;
    }

    if (real_mciSendCommandA)
    {
        return real_mciSendCommandA(IDDevice, uMsg, fdwCommand, dwParam);
    }
    else
    {
        /* fallback */
        return MCIERR_UNRECOGNIZED_COMMAND;
    }
}

UINT WINAPI fake_auxGetNumDevs()
{
    dprintf("fake_auxGetNumDevs()\r\n");
    return 1;
}

MMRESULT WINAPI fake_auxGetDevCapsA(UINT_PTR uDeviceID, LPAUXCAPS lpCaps, UINT cbCaps)
{
    dprintf("fake_auxGetDevCapsA(uDeviceID=%08X, lpCaps=%p, cbCaps=%08X\n", uDeviceID, lpCaps, cbCaps);

    lpCaps->wMid = 2 /*MM_CREATIVE*/;
    lpCaps->wPid = 401 /*MM_CREATIVE_AUX_CD*/;
    lpCaps->vDriverVersion = 1;
    strcpy(lpCaps->szPname, "ogg-winmm virtual CD");
    lpCaps->wTechnology = AUXCAPS_CDAUDIO;
    lpCaps->dwSupport = AUXCAPS_VOLUME;

    return MMSYSERR_NOERROR;
}


MMRESULT WINAPI fake_auxGetVolume(UINT uDeviceID, LPDWORD lpdwVolume)
{
    dprintf("fake_auxGetVolume(uDeviceId=%08X, lpdwVolume=%p)\r\n", uDeviceID, lpdwVolume);
    *lpdwVolume = 0x00000000;
    return MMSYSERR_NOERROR;
}

MMRESULT WINAPI fake_auxSetVolume(UINT uDeviceID, DWORD dwVolume)
{
    static DWORD oldVolume = -1;
    char cmdbuf[256];

    if (dwVolume == oldVolume)
    {
        return MMSYSERR_NOERROR;
    }

    oldVolume = dwVolume;

    unsigned short left = LOWORD(dwVolume);
    unsigned short right = HIWORD(dwVolume);

    dprintf("    left : %ud (%04X)\n", left, left);
    dprintf("    right: %ud (%04X)\n", right, right);

    plr_volume((left / 65535.0f) * 100);

    return MMSYSERR_NOERROR;
}
