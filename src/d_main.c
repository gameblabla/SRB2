// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  d_main.c
/// \brief SRB2 main program
///
///        SRB2 main program (D_SRB2Main) and game loop (D_SRB2Loop),
///        plus functions to parse command line parameters, configure game
///        parameters, and call the startup functions.

#if (defined (__unix__) && !defined (MSDOS)) || defined(__APPLE__) || defined (UNIXCOMMON)
#include <sys/stat.h>
#include <sys/types.h>
#endif

#ifdef __GNUC__
#include <unistd.h> // for getcwd
#endif

#ifdef PC_DOS
#include <stdio.h> // for snprintf
int	snprintf(char *str, size_t n, const char *fmt, ...);
//int	vsnprintf(char *str, size_t n, const char *fmt, va_list ap);
#endif

#ifdef _WIN32
#include <direct.h>
#include <malloc.h>
#endif

#include <time.h>

#include "doomdef.h"
#include "am_map.h"
#include "console.h"
#include "d_net.h"
#include "f_finale.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_misc.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "r_main.h"
#include "r_local.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "d_main.h"
#include "d_netfil.h"
#include "m_cheat.h"
#include "y_inter.h"
#include "p_local.h" // chasecam
#include "mserv.h" // ms_RoomId
#include "m_misc.h" // screenshot functionality
#include "dehacked.h" // Dehacked list test
#include "m_cond.h" // condition initialization
#include "fastcmp.h"
#include "keys.h"
#include "filesrch.h" // refreshdirmenu, mainwadstally
#include "g_input.h" // tutorial mode control scheming

#ifdef CMAKECONFIG
#include "config.h"
#else
#include "config.h.in"
#endif

#ifdef HWRENDER
#include "hardware/hw_main.h" // 3D View Rendering
#endif

#ifdef _WINDOWS
#include "win32/win_main.h" // I_DoStartupMouse
#endif

#ifdef HW3SOUND
#include "hardware/hw3sound.h"
#endif

#ifdef HAVE_BLUA
#include "lua_script.h"
#endif

// platform independant focus loss
UINT8 window_notinfocus = false;

//
// DEMO LOOP
//
static char *startupwadfiles[MAX_WADFILES];

boolean devparm = false; // started game with -devparm

boolean singletics = false; // timedemo
boolean lastdraw = false;

postimg_t postimgtype = postimg_none;
INT32 postimgparam;
postimg_t postimgtype2 = postimg_none;
INT32 postimgparam2;

// These variables are only true if
// whether the respective sound system is disabled
// or they're init'ed, but the player just toggled them
boolean midi_disabled = false;
boolean sound_disabled = false;
boolean digital_disabled = false;

boolean advancedemo;
#ifdef DEBUGFILE
INT32 debugload = 0;
#endif

char srb2home[256] = ".";
char srb2path[256] = ".";
boolean usehome = true;
const char *pandf = "%s" PATHSEP "%s";

//
// EVENT HANDLING
//
// Events are asynchronous inputs generally generated by the game user.
// Events can be discarded if no responder claims them
// referenced from i_system.c for I_GetKey()

event_t events[MAXEVENTS];
INT32 eventhead, eventtail;

boolean dedicated = false;

//
// D_PostEvent
// Called by the I/O functions when input is detected
//
void D_PostEvent(const event_t *ev)
{
	events[eventhead] = *ev;
	eventhead = (eventhead+1) & (MAXEVENTS-1);
}
// just for lock this function
#if defined (PC_DOS) && !defined (DOXYGEN)
void D_PostEvent_end(void) {};
#endif

// modifier keys
// Now handled in I_OsPolling
UINT8 shiftdown = 0; // 0x1 left, 0x2 right
UINT8 ctrldown = 0; // 0x1 left, 0x2 right
UINT8 altdown = 0; // 0x1 left, 0x2 right
boolean capslock = 0;	// gee i wonder what this does.

//
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
//
void D_ProcessEvents(void)
{
	event_t *ev;

	for (; eventtail != eventhead; eventtail = (eventtail+1) & (MAXEVENTS-1))
	{
		ev = &events[eventtail];

		// Screenshots over everything so that they can be taken anywhere.
		if (M_ScreenshotResponder(ev))
			continue; // ate the event

		if (gameaction == ga_nothing && gamestate == GS_TITLESCREEN)
		{
			if (cht_Responder(ev))
				continue;
		}

		// Menu input
		if (M_Responder(ev))
			continue; // menu ate the event

		// console input
		if (CON_Responder(ev))
			continue; // ate the event

		G_Responder(ev);
	}
}

//
// D_Display
// draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw
// added comment : there is a wipe eatch change of the gamestate
gamestate_t wipegamestate = GS_LEVEL;
// -1: Default; 0-n: Wipe index; INT16_MAX: do not wipe
INT16 wipetypepre = -1;
INT16 wipetypepost = -1;

static void D_Display(void)
{
	boolean forcerefresh = false;
	static boolean wipe = false;
	INT32 wipedefindex = 0;

	if (dedicated)
		return;

	if (nodrawers)
		return; // for comparative timing/profiling

	// check for change of screen size (video mode)
	if (setmodeneeded && !wipe)
		SCR_SetMode(); // change video mode

	if (vid.recalc)
		SCR_Recalc(); // NOTE! setsizeneeded is set by SCR_Recalc()

	// change the view size if needed
	if (setsizeneeded)
	{
		R_ExecuteSetViewSize();
		forcerefresh = true; // force background redraw
	}

	// draw buffered stuff to screen
	// Used only by linux GGI version
	I_UpdateNoBlit();

	// save the current screen if about to wipe
	wipe = (gamestate != wipegamestate);
	if (wipe && wipetypepre != INT16_MAX)
	{
		// set for all later
		wipedefindex = gamestate; // wipe_xxx_toblack
		if (gamestate == GS_INTERMISSION)
		{
			if (intertype == int_spec) // Special Stage
				wipedefindex = wipe_specinter_toblack;
			else if (intertype != int_coop) // Multiplayer
				wipedefindex = wipe_multinter_toblack;
		}

		if (wipetypepre < 0 || !F_WipeExists(wipetypepre))
			wipetypepre = wipedefs[wipedefindex];

		if (rendermode != render_none)
		{
			// Fade to black first
			if ((wipegamestate == (gamestate_t)FORCEWIPE ||
			        (wipegamestate != (gamestate_t)FORCEWIPEOFF
						&& !(gamestate == GS_LEVEL || (gamestate == GS_TITLESCREEN && titlemapinaction)))
					) // fades to black on its own timing, always
			 && wipetypepre != UINT8_MAX)
			{
				F_WipeStartScreen();
				V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, 31);
				F_WipeEndScreen();
				F_RunWipe(wipetypepre, gamestate != GS_TIMEATTACK && gamestate != GS_TITLESCREEN);
			}

			F_WipeStartScreen();
		}

		wipetypepre = -1;
	}
	else
		wipetypepre = -1;

	// do buffered drawing
	switch (gamestate)
	{
		case GS_TITLESCREEN:
			if (!titlemapinaction || !curbghide) {
				F_TitleScreenDrawer();
				break;
			}
			// Intentional fall-through
		case GS_LEVEL:
			if (!gametic)
				break;
			HU_Erase();
			AM_Drawer();
			break;

		case GS_INTERMISSION:
			Y_IntermissionDrawer();
			HU_Erase();
			HU_Drawer();
			break;

		case GS_TIMEATTACK:
			break;

		case GS_INTRO:
			F_IntroDrawer();
			if (wipegamestate == (gamestate_t)-1)
				wipe = true;
			break;

		case GS_CUTSCENE:
			F_CutsceneDrawer();
			HU_Erase();
			HU_Drawer();
			break;

		case GS_GAMEEND:
			F_GameEndDrawer();
			break;

		case GS_EVALUATION:
			F_GameEvaluationDrawer();
			HU_Erase();
			HU_Drawer();
			break;

		case GS_CONTINUING:
			F_ContinueDrawer();
			break;

		case GS_CREDITS:
			F_CreditDrawer();
			HU_Erase();
			HU_Drawer();
			break;

		case GS_WAITINGPLAYERS:
			// The clientconnect drawer is independent...
		case GS_DEDICATEDSERVER:
		case GS_NULL:
			break;
	}

	// STUPID race condition...
	if (wipegamestate == GS_INTRO && gamestate == GS_TITLESCREEN)
		wipegamestate = FORCEWIPEOFF;
	else
	{
		wipegamestate = gamestate;

		// clean up border stuff
		// see if the border needs to be initially drawn
		if (gamestate == GS_LEVEL || (gamestate == GS_TITLESCREEN && titlemapinaction && curbghide))
		{
			// draw the view directly

			if (!automapactive && !dedicated && cv_renderview.value)
			{
				if (players[displayplayer].mo || players[displayplayer].playerstate == PST_DEAD)
				{
					topleft = screens[0] + viewwindowy*vid.width + viewwindowx;
					objectsdrawn = 0;
	#ifdef HWRENDER
					if (rendermode != render_soft)
						HWR_RenderPlayerView(0, &players[displayplayer]);
					else
	#endif
					if (rendermode != render_none)
						R_RenderPlayerView(&players[displayplayer]);
				}

				// render the second screen
				if (splitscreen && players[secondarydisplayplayer].mo)
				{
	#ifdef HWRENDER
					if (rendermode != render_soft)
						HWR_RenderPlayerView(1, &players[secondarydisplayplayer]);
					else
	#endif
					if (rendermode != render_none)
					{
						viewwindowy = vid.height / 2;
						M_Memcpy(ylookup, ylookup2, viewheight*sizeof (ylookup[0]));

						topleft = screens[0] + viewwindowy*vid.width + viewwindowx;

						R_RenderPlayerView(&players[secondarydisplayplayer]);

						viewwindowy = 0;
						M_Memcpy(ylookup, ylookup1, viewheight*sizeof (ylookup[0]));
					}
				}

				// Image postprocessing effect
				if (rendermode == render_soft)
				{
					if (postimgtype)
						V_DoPostProcessor(0, postimgtype, postimgparam);
					if (postimgtype2)
						V_DoPostProcessor(1, postimgtype2, postimgparam2);
				}
			}

			if (lastdraw)
			{
				if (rendermode == render_soft)
				{
					VID_BlitLinearScreen(screens[0], screens[1], vid.width*vid.bpp, vid.height, vid.width*vid.bpp, vid.rowbytes);
					usebuffer = true;
				}
				lastdraw = false;
			}

			if (gamestate == GS_LEVEL)
			{
				ST_Drawer();
				F_TextPromptDrawer();
				HU_Drawer();
			}
			else
				F_TitleScreenDrawer();
		}
	}

	// change gamma if needed
	// (GS_LEVEL handles this already due to level-specific palettes)
	if (forcerefresh && !(gamestate == GS_LEVEL || (gamestate == GS_TITLESCREEN && titlemapinaction)))
		V_SetPalette(0);

	// draw pause pic
	if (paused && cv_showhud.value && (!menuactive || netgame))
	{
#if 0
		INT32 py;
		patch_t *patch;
		if (automapactive)
			py = 4;
		else
			py = viewwindowy + 4;
		patch = W_CachePatchName("M_PAUSE", PU_CACHE);
		V_DrawScaledPatch(viewwindowx + (BASEVIDWIDTH - SHORT(patch->width))/2, py, 0, patch);
#else
		INT32 y = ((automapactive) ? (32) : (BASEVIDHEIGHT/2));
		M_DrawTextBox((BASEVIDWIDTH/2) - (60), y - (16), 13, 2);
		V_DrawCenteredString(BASEVIDWIDTH/2, y - (4), V_YELLOWMAP, "Game Paused");
#endif
	}

	// vid size change is now finished if it was on...
	vid.recalc = 0;

	// FIXME: draw either console or menu, not the two
	if (gamestate != GS_TIMEATTACK)
		CON_Drawer();

	M_Drawer(); // menu is drawn even on top of everything
	// focus lost moved to M_Drawer

	//
	// wipe update
	//
	if (wipe && wipetypepost != INT16_MAX)
	{
		// note: moved up here because NetUpdate does input changes
		// and input during wipe tends to mess things up
		wipedefindex += WIPEFINALSHIFT;

		if (wipetypepost < 0 || !F_WipeExists(wipetypepost))
			wipetypepost = wipedefs[wipedefindex];

		if (rendermode != render_none)
		{
			F_WipeEndScreen();
			F_RunWipe(wipetypepost, gamestate != GS_TIMEATTACK && gamestate != GS_TITLESCREEN);
		}

		wipetypepost = -1;
	}
	else
		wipetypepost = -1;

	NetUpdate(); // send out any new accumulation

	// It's safe to end the game now.
	if (G_GetExitGameFlag())
	{
		Command_ExitGame_f();
		G_ClearExitGameFlag();
	}

	//
	// normal update
	//
	if (!wipe)
	{
		if (cv_netstat.value)
		{
			char s[50];
			Net_GetNetStat();

			s[sizeof s - 1] = '\0';

			snprintf(s, sizeof s - 1, "get %d b/s", getbps);
			V_DrawRightAlignedString(BASEVIDWIDTH, BASEVIDHEIGHT-ST_HEIGHT-40, V_YELLOWMAP, s);
			snprintf(s, sizeof s - 1, "send %d b/s", sendbps);
			V_DrawRightAlignedString(BASEVIDWIDTH, BASEVIDHEIGHT-ST_HEIGHT-30, V_YELLOWMAP, s);
			snprintf(s, sizeof s - 1, "GameMiss %.2f%%", gamelostpercent);
			V_DrawRightAlignedString(BASEVIDWIDTH, BASEVIDHEIGHT-ST_HEIGHT-20, V_YELLOWMAP, s);
			snprintf(s, sizeof s - 1, "SysMiss %.2f%%", lostpercent);
			V_DrawRightAlignedString(BASEVIDWIDTH, BASEVIDHEIGHT-ST_HEIGHT-10, V_YELLOWMAP, s);
		}

		I_FinishUpdate(); // page flip or blit buffer
	}
}

// =========================================================================
// D_SRB2Loop
// =========================================================================

tic_t rendergametic;

void D_SRB2Loop(void)
{
	tic_t oldentertics = 0, entertic = 0, realtics = 0, rendertimeout = INFTICS;

	if (dedicated)
		server = true;

	if (M_CheckParm("-voodoo")) // 256x256 Texture Limiter
		COM_BufAddText("gr_voodoocompatibility on\n");

	// Pushing of + parameters is now done back in D_SRB2Main, not here.

	CONS_Printf("I_StartupKeyboard()...\n");
	I_StartupKeyboard();

#ifdef _WINDOWS
	CONS_Printf("I_StartupMouse()...\n");
	I_DoStartupMouse();
#endif

	oldentertics = I_GetTime();

	// end of loading screen: CONS_Printf() will no more call FinishUpdate()
	con_startup = false;

	// make sure to do a d_display to init mode _before_ load a level
	SCR_SetMode(); // change video mode
	SCR_Recalc();

	// Check and print which version is executed.
	// Use this as the border between setup and the main game loop being entered.
	CONS_Printf(
	"===========================================================================\n"
	"                   We hope you enjoy this game as\n"
	"                     much as we did making it!\n"
	"                            ...wait. =P\n"
	"===========================================================================\n");

	// hack to start on a nice clear console screen.
	COM_ImmedExecute("cls;version");

	if (rendermode == render_soft)
		V_DrawScaledPatch(0, 0, 0, (patch_t *)W_CacheLumpNum(W_GetNumForName("CONSBACK"), PU_CACHE));
	I_FinishUpdate(); // page flip or blit buffer

	for (;;)
	{
		if (lastwipetic)
		{
			oldentertics = lastwipetic;
			lastwipetic = 0;
		}

		// get real tics
		entertic = I_GetTime();
		realtics = entertic - oldentertics;
		oldentertics = entertic;

		refreshdirmenu = 0; // not sure where to put this, here as good as any?

#ifdef DEBUGFILE
		if (!realtics)
			if (debugload)
				debugload--;
#endif

		if (!realtics && !singletics)
		{
			I_Sleep();
			continue;
		}

#ifdef HW3SOUND
		HW3S_BeginFrameUpdate();
#endif

		// don't skip more than 10 frames at a time
		// (fadein / fadeout cause massive frame skip!)
		if (realtics > 8)
			realtics = 1;

		// process tics (but maybe not if realtic == 0)
		TryRunTics(realtics);

		if (lastdraw || singletics || gametic > rendergametic)
		{
			rendergametic = gametic;
			rendertimeout = entertic+TICRATE/17;

			// Update display, next frame, with current state.
			D_Display();

			if (moviemode)
				M_SaveFrame();
			if (takescreenshot) // Only take screenshots after drawing.
				M_DoScreenShot();
		}
		else if (rendertimeout < entertic) // in case the server hang or netsplit
		{
			// Lagless camera! Yay!
			if (gamestate == GS_LEVEL && netgame)
			{
				if (splitscreen && camera2.chase)
					P_MoveChaseCamera(&players[secondarydisplayplayer], &camera2, false);
				if (camera.chase)
					P_MoveChaseCamera(&players[displayplayer], &camera, false);
			}
			D_Display();

			if (moviemode)
				M_SaveFrame();
			if (takescreenshot) // Only take screenshots after drawing.
				M_DoScreenShot();
		}

		// consoleplayer -> displayplayer (hear sounds from viewpoint)
		S_UpdateSounds(); // move positional sounds

		// check for media change, loop music..
		I_UpdateCD();

#ifdef HW3SOUND
		HW3S_EndFrameUpdate();
#endif

#ifdef HAVE_BLUA
		LUA_Step();
#endif
	}
}

//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
void D_AdvanceDemo(void)
{
	advancedemo = true;
}

// =========================================================================
// D_SRB2Main
// =========================================================================

//
// D_StartTitle
//
void D_StartTitle(void)
{
	INT32 i;

	S_StopMusic();

	if (netgame)
	{
		if (gametype == GT_COOP)
		{
			G_SetGamestate(GS_WAITINGPLAYERS); // hack to prevent a command repeat

			if (server)
			{
				char mapname[6];

				strlcpy(mapname, G_BuildMapName(spstage_start), sizeof (mapname));
				strlwr(mapname);
				mapname[5] = '\0';

				COM_BufAddText(va("map %s\n", mapname));
			}
		}

		return;
	}

	// okay, stop now
	// (otherwise the game still thinks we're playing!)
	SV_StopServer();
	SV_ResetServer();

	for (i = 0; i < MAXPLAYERS; i++)
		CL_ClearPlayer(i);

	splitscreen = false;
	SplitScreen_OnChange();
	botingame = false;
	botskin = 0;
	cv_debug = 0;
	emeralds = 0;
	lastmaploaded = 0;

	// In case someone exits out at the same time they start a time attack run,
	// reset modeattacking
	modeattacking = ATTACKING_NONE;

	// empty maptol so mario/etc sounds don't play in sound test when they shouldn't
	maptol = 0;

	// reset to default player stuff
	COM_BufAddText (va("%s \"%s\"\n",cv_playername.name,cv_defaultplayername.string));
	COM_BufAddText (va("%s \"%s\"\n",cv_skin.name,cv_defaultskin.string));
	COM_BufAddText (va("%s \"%s\"\n",cv_playercolor.name,cv_defaultplayercolor.string));
	COM_BufAddText (va("%s \"%s\"\n",cv_playername2.name,cv_defaultplayername2.string));
	COM_BufAddText (va("%s \"%s\"\n",cv_skin2.name,cv_defaultskin2.string));
	COM_BufAddText (va("%s \"%s\"\n",cv_playercolor2.name,cv_defaultplayercolor2.string));

	gameaction = ga_nothing;
	displayplayer = consoleplayer = 0;
	gametype = GT_COOP;
	paused = false;
	advancedemo = false;
	F_InitMenuPresValues();
	F_StartTitleScreen();

	currentMenu = &MainDef; // reset the current menu ID

	// Reset the palette
	if (rendermode != render_none)
		V_SetPaletteLump("PLAYPAL");

	// The title screen is obviously not a tutorial! (Unless I'm mistaken)
	if (tutorialmode && tutorialgcs)
	{
		G_CopyControls(gamecontrol, gamecontroldefault[gcs_custom], gcl_tutorial_full, num_gcl_tutorial_full); // using gcs_custom as temp storage
		CV_SetValue(&cv_usemouse, tutorialusemouse);
		CV_SetValue(&cv_alwaysfreelook, tutorialfreelook);
		CV_SetValue(&cv_mousemove, tutorialmousemove);
		CV_SetValue(&cv_analog, tutorialanalog);
		M_StartMessage("Do you want to \x82save the recommended \x82movement controls?\x80\n\nPress 'Y' or 'Enter' to confirm\nPress 'N' or any key to keep \nyour current controls",
			M_TutorialSaveControlResponse, MM_YESNO);
	}
	tutorialmode = false;
}

//
// D_AddFile
//
static void D_AddFile(const char *file)
{
	size_t pnumwadfiles;
	char *newfile;

	for (pnumwadfiles = 0; startupwadfiles[pnumwadfiles]; pnumwadfiles++)
		;

	newfile = malloc(strlen(file) + 1);
	if (!newfile)
	{
		I_Error("No more free memory to AddFile %s",file);
	}
	strcpy(newfile, file);

	startupwadfiles[pnumwadfiles] = newfile;
}

static inline void D_CleanFile(void)
{
	size_t pnumwadfiles;
	for (pnumwadfiles = 0; startupwadfiles[pnumwadfiles]; pnumwadfiles++)
	{
		free(startupwadfiles[pnumwadfiles]);
		startupwadfiles[pnumwadfiles] = NULL;
	}
}

// ==========================================================================
// Identify the SRB2 version, and IWAD file to use.
// ==========================================================================

static void IdentifyVersion(void)
{
	char *srb2wad;
	const char *srb2waddir = NULL;

#if (defined (__unix__) && !defined (MSDOS)) || defined (UNIXCOMMON) || defined (HAVE_SDL)
	// change to the directory where 'srb2.pk3' is found
	srb2waddir = I_LocateWad();
#endif

	// get the current directory (possible problem on NT with "." as current dir)
	if (srb2waddir)
	{
		strlcpy(srb2path,srb2waddir,sizeof (srb2path));
	}
	else
	{
		if (getcwd(srb2path, 256) != NULL)
			srb2waddir = srb2path;
		else
		{
			srb2waddir = ".";
		}
	}

#if defined (macintosh) && !defined (HAVE_SDL)
	// cwd is always "/" when app is dbl-clicked
	if (!stricmp(srb2waddir, "/"))
		srb2waddir = I_GetWadDir();
#endif
	// Commercial.
	srb2wad = malloc(strlen(srb2waddir)+1+8+1);
	if (srb2wad == NULL)
		I_Error("No more free memory to look in %s", srb2waddir);
	else
		sprintf(srb2wad, pandf, srb2waddir, "srb2.pk3");

	// will be overwritten in case of -cdrom or unix/win home
	snprintf(configfile, sizeof configfile, "%s" PATHSEP CONFIGFILENAME, srb2waddir);
	configfile[sizeof configfile - 1] = '\0';

	// Load the IWAD
	if (srb2wad != NULL && FIL_ReadFileOK(srb2wad))
		D_AddFile(srb2wad);
	else
		I_Error("srb2.pk3 not found! Expected in %s, ss file: %s\n", srb2waddir, srb2wad);

	if (srb2wad)
		free(srb2wad);

	// if you change the ordering of this or add/remove a file, be sure to update the md5
	// checking in D_SRB2Main

	// Add the maps
	D_AddFile(va(pandf,srb2waddir,"zones.dta"));

	// Add the players
	D_AddFile(va(pandf,srb2waddir, "player.dta"));

#ifdef USE_PATCH_DTA
	// Add our crappy patches to fix our bugs
	D_AddFile(va(pandf,srb2waddir,"patch.pk3"));
#endif

#if !defined (HAVE_SDL) || defined (HAVE_MIXER)
	{
#define MUSICTEST(str) \
		{\
			const char *musicpath = va(pandf,srb2waddir,str);\
			int ms = W_VerifyNMUSlumps(musicpath); \
			if (ms == 1) \
				D_AddFile(musicpath); \
			else if (ms == 0) \
				I_Error("File "str" has been modified with non-music/sound lumps"); \
		}

		MUSICTEST("music.dta")
#ifdef DEVELOP // remove when music_new.dta is merged into music.dta
		MUSICTEST("music_new.dta")
#endif
	}
#endif
}

#ifdef PC_DOS
/* ======================================================================== */
// Code for printing SRB2's title bar in DOS
/* ======================================================================== */

//
// Center the title string, then add the date and time of compilation.
//
static inline void D_MakeTitleString(char *s)
{
	char temp[82];
	char *t;
	const char *u;
	INT32 i;

	for (i = 0, t = temp; i < 82; i++)
		*t++=' ';

	for (t = temp + (80-strlen(s))/2, u = s; *u != '\0' ;)
		*t++ = *u++;

	u = compdate;
	for (t = temp + 1, i = 11; i-- ;)
		*t++ = *u++;
	u = comptime;
	for (t = temp + 71, i = 8; i-- ;)
		*t++ = *u++;

	temp[80] = '\0';
	strcpy(s, temp);
}

static inline void D_Titlebar(void)
{
	char title1[82]; // srb2 title banner
	char title2[82];

	strcpy(title1, "Sonic Robo Blast 2");
	strcpy(title2, "Sonic Robo Blast 2");

	D_MakeTitleString(title1);

	// SRB2 banner
	clrscr();
	textattr((BLUE<<4)+WHITE);
	clreol();
	cputs(title1);

	// standard srb2 banner
	textattr((RED<<4)+WHITE);
	clreol();
	gotoxy((80-strlen(title2))/2, 2);
	cputs(title2);
	normvideo();
	gotoxy(1,3);
}
#endif

//
// D_SRB2Main
//
void D_SRB2Main(void)
{
	INT32 p;

	INT32 pstartmap = 1;
	boolean autostart = false;

	// Print GPL notice for our console users (Linux)
	CONS_Printf(
	"\n\nSonic Robo Blast 2\n"
	"Copyright (C) 1998-2018 by Sonic Team Junior\n\n"
	"This program comes with ABSOLUTELY NO WARRANTY.\n\n"
	"This is free software, and you are welcome to redistribute it\n"
	"and/or modify it under the terms of the GNU General Public License\n"
	"as published by the Free Software Foundation; either version 2 of\n"
	"the License, or (at your option) any later version.\n"
	"See the 'LICENSE.txt' file for details.\n\n"
	"Sonic the Hedgehog and related characters are trademarks of SEGA.\n"
	"We do not claim ownership of SEGA's intellectual property used\n"
	"in this program.\n\n");

	// keep error messages until the final flush(stderr)
#if !defined (PC_DOS) && !defined(NOTERMIOS)
	if (setvbuf(stderr, NULL, _IOFBF, 1000))
		I_OutputMsg("setvbuf didnt work\n");
#endif

#ifdef GETTEXT
	// initialise locale code
	M_StartupLocale();
#endif

	// get parameters from a response file (eg: srb2 @parms.txt)
	M_FindResponseFile();

	// MAINCFG is now taken care of where "OBJCTCFG" is handled
	G_LoadGameSettings();

	// Test Dehacked lists
	DEH_Check();

	// identify the main IWAD file to use
	IdentifyVersion();

#if !defined(NOTERMIOS)
	setbuf(stdout, NULL); // non-buffered output
#endif

#if 0 //defined (_DEBUG)
	devparm = M_CheckParm("-nodebug") == 0;
#else
	devparm = M_CheckParm("-debug") != 0;
#endif

	// for dedicated server
#if !defined (_WINDOWS) //already check in win_main.c
	dedicated = M_CheckParm("-dedicated") != 0;
#endif

#ifdef PC_DOS
	D_Titlebar();
#endif

	if (devparm)
		CONS_Printf(M_GetText("Development mode ON.\n"));

	// default savegame
	strcpy(savegamename, SAVEGAMENAME"%u.ssg");

	{
		const char *userhome = D_Home(); //Alam: path to home

		if (!userhome)
		{
#if ((defined (__unix__) && !defined (MSDOS)) || defined(__APPLE__) || defined (UNIXCOMMON)) && !defined (__CYGWIN__)
			I_Error("Please set $HOME to your home directory\n");
#else
			if (dedicated)
				snprintf(configfile, sizeof configfile, "d"CONFIGFILENAME);
			else
				snprintf(configfile, sizeof configfile, CONFIGFILENAME);
#endif
		}
		else
		{
			// use user specific config file
#ifdef DEFAULTDIR
			snprintf(srb2home, sizeof srb2home, "%s" PATHSEP DEFAULTDIR, userhome);
			snprintf(downloaddir, sizeof downloaddir, "%s" PATHSEP "DOWNLOAD", srb2home);
			if (dedicated)
				snprintf(configfile, sizeof configfile, "%s" PATHSEP "d"CONFIGFILENAME, srb2home);
			else
				snprintf(configfile, sizeof configfile, "%s" PATHSEP CONFIGFILENAME, srb2home);

			// can't use sprintf since there is %u in savegamename
			strcatbf(savegamename, srb2home, PATHSEP);

			I_mkdir(srb2home, 0700);
#else
			snprintf(srb2home, sizeof srb2home, "%s", userhome);
			snprintf(downloaddir, sizeof downloaddir, "%s", userhome);
			if (dedicated)
				snprintf(configfile, sizeof configfile, "%s" PATHSEP "d"CONFIGFILENAME, userhome);
			else
				snprintf(configfile, sizeof configfile, "%s" PATHSEP CONFIGFILENAME, userhome);

			// can't use sprintf since there is %u in savegamename
			strcatbf(savegamename, userhome, PATHSEP);
#endif
		}

		configfile[sizeof configfile - 1] = '\0';
	}

	// rand() needs seeded regardless of password
	srand((unsigned int)time(NULL));

	if (M_CheckParm("-password") && M_IsNextParm())
		D_SetPassword(M_GetNextParm());

	// add any files specified on the command line with -file wadfile
	// to the wad list
	if (!(M_CheckParm("-connect") && !M_CheckParm("-server")))
	{
		if (M_CheckParm("-file"))
		{
			// the parms after p are wadfile/lump names,
			// until end of parms or another - preceded parm
			while (M_IsNextParm())
			{
				const char *s = M_GetNextParm();

				if (s) // Check for NULL?
				{
					if (!W_VerifyNMUSlumps(s))
						G_SetGameModified(true);
					D_AddFile(s);
				}
			}
		}
	}

	// get map from parms

	if (M_CheckParm("-server") || dedicated)
		netgame = server = true;

	if (M_CheckParm("-warp") && M_IsNextParm())
	{
		const char *word = M_GetNextParm();
		char ch; // use this with sscanf to catch non-digits with
		if (fastncmp(word, "MAP", 3)) // MAPxx name
			pstartmap = M_MapNumber(word[3], word[4]);
		else if (sscanf(word, "%d%c", &pstartmap, &ch) != 1) // a plain number
			I_Error("Cannot warp to map %s (invalid map name)\n", word);
		// Don't check if lump exists just yet because the wads haven't been loaded!
		// Just do a basic range check here.
		if (pstartmap < 1 || pstartmap > NUMMAPS)
			I_Error("Cannot warp to map %d (out of range)\n", pstartmap);
		else
		{
			if (!M_CheckParm("-server"))
				G_SetGameModified(true);
			autostart = true;
		}
	}

	CONS_Printf("Z_Init(): Init zone memory allocation daemon. \n");
	Z_Init();

	// adapt tables to SRB2's needs, including extra slots for dehacked file support
	P_PatchInfoTables();

	// initiate menu metadata before SOCcing them
	M_InitMenuPresTables();

	// init title screen display params
	if (M_CheckParm("-connect"))
		F_InitMenuPresValues();

	//---------------------------------------------------- READY TIME
	// we need to check for dedicated before initialization of some subsystems

	CONS_Printf("I_StartupTimer()...\n");
	I_StartupTimer();

	// Make backups of some SOCcable tables.
	P_BackupTables();

	// Setup character tables
	// Have to be done here before files are loaded
	M_InitCharacterTables();

	mainwads = 0;

#ifndef DEVELOP // md5s last updated 12/14/14

	// Check MD5s of autoloaded files
	W_VerifyFileMD5(mainwads++, ASSET_HASH_SRB2_PK3); // srb2.pk3
	W_VerifyFileMD5(mainwads++, ASSET_HASH_ZONES_DTA); // zones.dta
	W_VerifyFileMD5(mainwads++, ASSET_HASH_PLAYER_DTA); // player.dta
#ifdef USE_PATCH_DTA
	W_VerifyFileMD5(mainwads++, ASSET_HASH_PATCH_DTA); // patch.dta
#endif
	// don't check music.dta because people like to modify it, and it doesn't matter if they do
	// ...except it does if they slip maps in there, and that's what W_VerifyNMUSlumps is for.
	//mainwads++; // music.dta does not increment mainwads (see <= 2.1.21)
	//mainwads++; // neither does music_new.dta
#else

	mainwads++;	// srb2.pk3
	mainwads++; // zones.dta
	mainwads++; // player.dta
#ifdef USE_PATCH_DTA
	mainwads++; // patch.dta
#endif
	//mainwads++; // music.dta does not increment mainwads (see <= 2.1.21)
	//mainwads++; // neither does music_new.dta

	// load wad, including the main wad file
	CONS_Printf("W_InitMultipleFiles(): Adding IWAD and main PWADs.\n");
	if (!W_InitMultipleFiles(startupwadfiles, mainwads))
#ifdef _DEBUG
		CONS_Error("A WAD file was not found or not valid.\nCheck the log to see which ones.\n");
#else
		I_Error("A WAD file was not found or not valid.\nCheck the log to see which ones.\n");
#endif
	D_CleanFile();

#endif //ifndef DEVELOP

	mainwadstally = packetsizetally;

	mainwadstally = packetsizetally;

	cht_Init();

	//---------------------------------------------------- READY SCREEN
	// we need to check for dedicated before initialization of some subsystems

	CONS_Printf("I_StartupGraphics()...\n");
	I_StartupGraphics();

	//--------------------------------------------------------- CONSOLE
	// setup loading screen
	SCR_Startup();

	// we need the font of the console
	CONS_Printf("HU_Init(): Setting up heads up display.\n");
	HU_Init();

	COM_Init();
	CON_Init();

	D_RegisterServerCommands();
	D_RegisterClientCommands(); // be sure that this is called before D_CheckNetGame
	R_RegisterEngineStuff();
	S_RegisterSoundStuff();

	I_RegisterSysCommands();

	//--------------------------------------------------------- CONFIG.CFG
	M_FirstLoadConfig(); // WARNING : this do a "COM_BufExecute()"

	G_LoadGameData();

#if (defined (__unix__) && !defined (MSDOS)) || defined (UNIXCOMMON) || defined (HAVE_SDL)
	VID_PrepareModeList(); // Regenerate Modelist according to cv_fullscreen
#endif

	// set user default mode or mode set at cmdline
	SCR_CheckDefaultMode();

	wipegamestate = gamestate;

	savedata.lives = 0; // flag this as not-used

	//------------------------------------------------ COMMAND LINE PARAMS

	// Initialize CD-Audio
	if (M_CheckParm("-usecd") && !dedicated)
		I_InitCD();

	if (M_CheckParm("-noupload"))
		COM_BufAddText("downloading 0\n");

	CONS_Printf("M_Init(): Init miscellaneous info.\n");
	M_Init();

	CONS_Printf("R_Init(): Init SRB2 refresh daemon.\n");
	R_Init();

	// setting up sound
	if (dedicated)
	{
		sound_disabled = true;
		midi_disabled = digital_disabled = true;
	}
	else
	{
		CONS_Printf("S_InitSfxChannels(): Setting up sound channels.\n");
	}
	if (M_CheckParm("-nosound"))
		sound_disabled = true;
	if (M_CheckParm("-nomusic")) // combines -nomidimusic and -nodigmusic
		midi_disabled = digital_disabled = true;
	else
	{
		if (M_CheckParm("-nomidimusic"))
			midi_disabled = true; // WARNING: DOS version initmusic in I_StartupSound
		if (M_CheckParm("-nodigmusic"))
			digital_disabled = true; // WARNING: DOS version initmusic in I_StartupSound
	}
	I_StartupSound();
	I_InitMusic();
	S_InitSfxChannels(cv_soundvolume.value);

	CONS_Printf("ST_Init(): Init status bar.\n");
	ST_Init();

	if (M_CheckParm("-room"))
	{
		if (!M_IsNextParm())
			I_Error("usage: -room <room_id>\nCheck the Master Server's webpage for room ID numbers.\n");
		ms_RoomId = atoi(M_GetNextParm());

#ifdef UPDATE_ALERT
		GetMODVersion_Console();
#endif
	}

	// init all NETWORK
	CONS_Printf("D_CheckNetGame(): Checking network game status.\n");
	if (D_CheckNetGame())
		autostart = true;

	// check for a driver that wants intermission stats
	// start the apropriate game based on parms
	if (M_CheckParm("-metal"))
	{
		G_RecordMetal();
		autostart = true;
	}
	else if (M_CheckParm("-record") && M_IsNextParm())
	{
		G_RecordDemo(M_GetNextParm());
		autostart = true;
	}

	// user settings come before "+" parameters.
	if (dedicated)
		COM_ImmedExecute(va("exec \"%s"PATHSEP"adedserv.cfg\"\n", srb2home));
	else
		COM_ImmedExecute(va("exec \"%s"PATHSEP"autoexec.cfg\" -noerror\n", srb2home));

	if (!autostart)
		M_PushSpecialParameters(); // push all "+" parameters at the command buffer

	// demo doesn't need anymore to be added with D_AddFile()
	p = M_CheckParm("-playdemo");
	if (!p)
		p = M_CheckParm("-timedemo");
	if (p && M_IsNextParm())
	{
		char tmp[MAX_WADPATH];
		// add .lmp to identify the EXTERNAL demo file
		// it is NOT possible to play an internal demo using -playdemo,
		// rather push a playdemo command.. to do.

		strcpy(tmp, M_GetNextParm());
		// get spaced filename or directory
		while (M_IsNextParm())
		{
			strcat(tmp, " ");
			strcat(tmp, M_GetNextParm());
		}

		FIL_DefaultExtension(tmp, ".lmp");

		CONS_Printf(M_GetText("Playing demo %s.\n"), tmp);

		if (M_CheckParm("-playdemo"))
		{
			singledemo = true; // quit after one demo
			G_DeferedPlayDemo(tmp);
		}
		else
			G_TimeDemo(tmp);

		G_SetGamestate(GS_NULL);
		wipegamestate = GS_NULL;
		return;
	}

	if (M_CheckParm("-ultimatemode"))
	{
		autostart = true;
		ultimatemode = true;
	}

	// rei/miru: bootmap (Idea: starts the game on a predefined map)
	if (bootmap && !(M_CheckParm("-warp") && M_IsNextParm()))
	{
		pstartmap = bootmap;

		if (pstartmap < 1 || pstartmap > NUMMAPS)
			I_Error("Cannot warp to map %d (out of range)\n", pstartmap);
		else
		{
			autostart = true;
		}
	}

	if (autostart || netgame)
	{
		gameaction = ga_nothing;

		CV_ClearChangedFlags();

		// Do this here so if you run SRB2 with eg +timelimit 5, the time limit counts
		// as having been modified for the first game.
		M_PushSpecialParameters(); // push all "+" parameter at the command buffer

		if (M_CheckParm("-gametype") && M_IsNextParm())
		{
			// from Command_Map_f
			INT32 j;
			INT16 newgametype = -1;
			const char *sgametype = M_GetNextParm();

			newgametype = G_GetGametypeByName(sgametype);

			if (newgametype == -1) // reached end of the list with no match
			{
				j = atoi(sgametype); // assume they gave us a gametype number, which is okay too
				if (j >= 0 && j < NUMGAMETYPES)
					newgametype = (INT16)j;
			}

			if (newgametype != -1)
			{
				j = gametype;
				gametype = newgametype;
				D_GameTypeChanged(j);
			}
		}

		if (server && !M_CheckParm("+map"))
		{
			// Prevent warping to nonexistent levels
			if (W_CheckNumForName(G_BuildMapName(pstartmap)) == LUMPERROR)
				I_Error("Could not warp to %s (map not found)\n", G_BuildMapName(pstartmap));
			// Prevent warping to locked levels
			// ... unless you're in a dedicated server.  Yes, technically this means you can view any level by
			// running a dedicated server and joining it yourself, but that's better than making dedicated server's
			// lives hell.
			else if (!dedicated && M_MapLocked(pstartmap))
				I_Error("You need to unlock this level before you can warp to it!\n");
			else
			{
				D_MapChange(pstartmap, gametype, ultimatemode, true, 0, false, false);
			}
		}
	}
	else if (M_CheckParm("-skipintro"))
	{
		F_InitMenuPresValues();
		F_StartTitleScreen();
	}
	else
		F_StartIntro(); // Tails 03-03-2002

	CON_ToggleOff();

	if (dedicated && server)
	{
		levelstarttic = gametic;
		G_SetGamestate(GS_LEVEL);
		if (!P_SetupLevel(false))
			I_Quit(); // fail so reset game stuff
	}
}

const char *D_Home(void)
{
	const char *userhome = NULL;

#ifdef ANDROID
	return "/data/data/org.srb2/";
#endif

	if (M_CheckParm("-home") && M_IsNextParm())
		userhome = M_GetNextParm();
	else
	{
#if !((defined (__unix__) && !defined (MSDOS)) || defined(__APPLE__) || defined (UNIXCOMMON)) && !defined (__APPLE__)
		if (FIL_FileOK(CONFIGFILENAME))
			usehome = false; // Let's NOT use home
		else
#endif
			userhome = I_GetEnv("HOME"); //Alam: my new HOME for srb2
	}
#ifdef _WIN32 //Alam: only Win32 have APPDATA and USERPROFILE
	if (!userhome && usehome) //Alam: Still not?
	{
		char *testhome = NULL;
		testhome = I_GetEnv("APPDATA");
		if (testhome != NULL
			&& (FIL_FileOK(va("%s" PATHSEP "%s" PATHSEP CONFIGFILENAME, testhome, DEFAULTDIR))))
		{
			userhome = testhome;
		}
	}
#ifndef __CYGWIN__
	if (!userhome && usehome) //Alam: All else fails?
	{
		char *testhome = NULL;
		testhome = I_GetEnv("USERPROFILE");
		if (testhome != NULL
			&& (FIL_FileOK(va("%s" PATHSEP "%s" PATHSEP CONFIGFILENAME, testhome, DEFAULTDIR))))
		{
			userhome = testhome;
		}
	}
#endif// !__CYGWIN__
#endif// _WIN32
	if (usehome) return userhome;
	else return NULL;
}
