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

#define MAGIC_DEVICEID 0xDEADBEEF
#define MAX_TRACKS 32

static int mp3Tracks[MAX_TRACKS];

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

        dprintf("wav-winmm searching tracks...\r\n");

        HMODULE hMod = GetModuleHandle("system32\\winmm.dll");
        real_mciSendCommandA = (void *)GetProcAddress(hMod, "mciSendCommandA");

        DIR *dp = opendir("./MUSIC");
        struct dirent *ep;
        if (dp)
        {
            while (ep = readdir(dp))
            {
                int track;
                if (sscanf(ep->d_name, "Track%02d.wav", &track) == 1)
                {
                    if (track > lastTrack)
                        lastTrack = track;

                    dprintf("WAV Track %02d: %s\r\n", track, ep->d_name);
                }
                else if (sscanf(ep->d_name, "Track%02d.mp3", &track) == 1)
                {
                    if (track-1 < MAX_TRACKS)
                    {
                        mp3Tracks[track-1] = 1;

                        if (track > lastTrack)
                            lastTrack = track;

                        dprintf("MP3 Track %02d: %s\r\n", track, ep->d_name);
                    }
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

        if (fdwCommand & MCI_OPEN_TYPE)
        {
            dprintf("    MCI_OPEN_TYPE\r\n");
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
    }

    if (IDDevice == MAGIC_DEVICEID)
    {
        if (uMsg == MCI_SET)
        {
            dprintf("  MCI_SET\r\n");

            return 0;
        }

        if (uMsg == MCI_CLOSE)
        {
            dprintf("  MCI_CLOSE\r\n");

            mciSendString("close music wait", NULL, 0, 0);

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

                mciSendString("close music wait", NULL, 0, 0);

                if (track > 0 && track < MAX_TRACKS-1 && mp3Tracks[track-1])
                    snprintf(cmdbuf, sizeof cmdbuf, "open mpegvideo!MUSIC\\Track%02d.mp3 alias music wait", track);
                else
                    snprintf(cmdbuf, sizeof cmdbuf, "open waveaudio!MUSIC\\Track%02d.wav alias music wait", track);

                mciSendString(cmdbuf, NULL, 0, 0);
                mciSendString("play music", NULL, 0, 0);
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

            mciSendString("close music wait", NULL, 0, 0);

            return 0;
        }

        if (uMsg == MCI_STATUS)
        {
            LPMCI_STATUS_PARMS parms = (LPVOID)dwParam;

            dprintf("  MCI_STATUS\r\n");

            parms->dwReturn = 0;

            if (fdwCommand & MCI_STATUS_ITEM)
            {
                dprintf("    MCI_STATUS_ITEM\r\n");

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

                    mciSendString("status music mode wait", cmdbuf, sizeof cmdbuf, 0);

                    parms->dwReturn = strcmp(cmdbuf, "playing") == 0 ? MCI_MODE_PLAY : MCI_MODE_STOP;
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
            }

            if (fdwCommand & MCI_TRACK)
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
