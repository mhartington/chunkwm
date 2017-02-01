#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <map>

#include "../../api/plugin_api.h"
#include "../../common/accessibility/application.h"
#include "../../common/accessibility/observer.h"
#include "../../common/dispatch/carbon.h"
#include "../../common/dispatch/cgeventtap.h"
#include "../../common/ipc/daemon.h"

#define internal static

typedef std::map<pid_t, macos_application *> macos_application_map;
typedef macos_application_map::iterator macos_application_map_it;

internal event_tap EventTap;
internal macos_application_map Applications;
internal pthread_mutex_t ApplicationsMutex;

macos_application_map *BeginApplications()
{
    pthread_mutex_lock(&ApplicationsMutex);
    return &Applications;
}

void EndApplications()
{
    pthread_mutex_unlock(&ApplicationsMutex);
}

macos_application *GetApplicationFromPID(pid_t PID)
{
    macos_application *Result = NULL;

    BeginApplications();
    macos_application_map_it It = Applications.find(PID);
    if(It != Applications.end())
    {
        Result = It->second;
    }
    EndApplications();

    return Result;
}

internal void
AddApplication(macos_application *Application)
{
    BeginApplications();
    macos_application_map_it It = Applications.find(Application->PID);
    if(It == Applications.end())
    {
        Applications[Application->PID] = Application;
    }
    EndApplications();
}

internal void
RemoveApplication(pid_t PID)
{
    BeginApplications();
    macos_application_map_it It = Applications.find(PID);
    if(It != Applications.end())
    {
        Applications.erase(It);
    }
    EndApplications();
}

internal
DAEMON_CALLBACK(DaemonCallback)
{
    printf("    plugin daemon: %s\n", Message);
}

internal
EVENTTAP_CALLBACK(EventCallback)
{
    event_tap *EventTap = (event_tap *) Reference;

    switch(Type)
    {
        case kCGEventTapDisabledByTimeout:
        case kCGEventTapDisabledByUserInput:
        {
            CGEventTapEnable(EventTap->Handle, true);
        } break;
        case kCGEventMouseMoved:
        {
            printf("kCGEventMouseMoved\n");
        } break;
        case kCGEventLeftMouseDown:
        {
            printf("kCGEventLeftMouseDown\n");
        } break;
        case kCGEventLeftMouseUp:
        {
            printf("kCGEventLeftMouseUp\n");
        } break;
        case kCGEventLeftMouseDragged:
        {
            printf("kCGEventLeftMouseDragged\n");
        } break;
        case kCGEventRightMouseDown:
        {
            printf("kCGEventRightMouseDown\n");
        } break;
        case kCGEventRightMouseUp:
        {
            printf("kCGEventRightMouseUp\n");
        } break;
        case kCGEventRightMouseDragged:
        {
            printf("kCGEventRightMouseDragged\n");
        } break;

        default: {} break;
    }

    return Event;
}

internal
OBSERVER_CALLBACK(Callback)
{
    macos_application *Application = (macos_application *) Reference;

    if(CFEqual(Notification, kAXWindowCreatedNotification))
    {
        printf("kAXWindowCreatedNotification\n");
    }
    else if(CFEqual(Notification, kAXUIElementDestroyedNotification))
    {
        printf("kAXUIElementDestroyedNotification\n");
    }
    else if(CFEqual(Notification, kAXFocusedWindowChangedNotification))
    {
        printf("kAXFocusedWindowChangedNotification\n");
    }
    else if(CFEqual(Notification, kAXWindowMiniaturizedNotification))
    {
        printf("kAXWindowMiniaturizedNotification\n");
    }
    else if(CFEqual(Notification, kAXWindowDeminiaturizedNotification))
    {
        printf("kAXWindowDeminiaturizedNotification\n");
    }
    else if(CFEqual(Notification, kAXWindowMovedNotification))
    {
        printf("kAXWindowMovedNotification\n");
    }
    else if(CFEqual(Notification, kAXWindowResizedNotification))
    {
        printf("kAXWindowResizedNotification\n");
    }
    else if(CFEqual(Notification, kAXTitleChangedNotification))
    {
        printf("kAXWindowTitleChangedNotification\n");
    }
}

void ApplicationLaunchedHandler(const char *Data)
{
    carbon_application_details *Info =
        (carbon_application_details *) Data;

    macos_application *Application = AXLibConstructApplication(Info->PSN, Info->PID, Info->ProcessName);
    if(Application)
    {
        printf("    plugin: launched '%s'\n", Info->ProcessName);
        AddApplication(Application);
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.5 * NSEC_PER_SEC), dispatch_get_main_queue(),
        ^{
            if(AXLibAddApplicationObserver(Application, Callback))
            {
                printf("    plugin: subscribed to '%s' notifications\n", Application->Name);
            }
        });
    }
}

void ApplicationTerminatedHandler(const char *Data)
{
    carbon_application_details *Info =
        (carbon_application_details *) Data;

    macos_application *Application = GetApplicationFromPID(Info->PID);
    if(Application)
    {
        printf("    plugin: terminated '%s'\n", Info->ProcessName);
        RemoveApplication(Application->PID);
        AXLibDestroyApplication(Application);
    }
}

inline bool
StringsAreEqual(const char *A, const char *B)
{
    bool Result = (strcmp(A, B) == 0);
    return Result;
}

/*
 * NOTE(koekeishiya): Function parameters
 * const char *Node
 * const char *Data
 * return: bool
 * */
PLUGIN_MAIN_FUNC(PluginMain)
{
    if(StringsAreEqual(Node, "chunkwm_export_application_launched"))
    {
        ApplicationLaunchedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_application_terminated"))
    {
        ApplicationTerminatedHandler(Data);
        return true;
    }
    else if(StringsAreEqual(Node, "chunkwm_export_space_changed"))
    {
        printf("Active Space Changed\n");
        return true;
    }

    return false;
}

internal bool
Init()
{
#if 0
    int Port = 4020;
    EventTap.Mask = ((1 << kCGEventMouseMoved) |
                     (1 << kCGEventLeftMouseDragged) |
                     (1 << kCGEventLeftMouseDown) |
                     (1 << kCGEventLeftMouseUp) |
                     (1 << kCGEventRightMouseDragged) |
                     (1 << kCGEventRightMouseDown) |
                     (1 << kCGEventRightMouseUp));

    bool Result = ((pthread_mutex_init(&ApplicationsMutex, NULL) == 0) &&
                   (StartDaemon(Port, &DaemonCallback)) &&
                   (BeginEventTap(&EventTap, &EventCallback)));
#else
    bool Result = (pthread_mutex_init(&ApplicationsMutex, NULL) == 0);
#endif
    return Result;
}

internal void
Deinit()
{
#if 0
    StopDaemon();
    EndEventTap(&EventTap);
#endif
    pthread_mutex_destroy(&ApplicationsMutex);
}

/*
 * NOTE(koekeishiya):
 * return: bool -> true if startup succeeded
 */
PLUGIN_BOOL_FUNC(PluginInit)
{
    printf("Plugin Init!\n");
    return Init();
}

PLUGIN_VOID_FUNC(PluginDeInit)
{
    printf("Plugin DeInit!\n");
    Deinit();
}

// NOTE(koekeishiya): Enable to manually trigger ABI mismatch
#if 0
#undef PLUGIN_API_VERSION
#define PLUGIN_API_VERSION 0
#endif

// NOTE(koekeishiya): Initialize plugin function pointers.
CHUNKWM_PLUGIN_VTABLE(PluginInit, PluginDeInit, PluginMain)

// NOTE(koekeishiya): Subscribe to ChunkWM events!
chunkwm_plugin_export Subscriptions[] =
{
    chunkwm_export_application_unhidden,
    chunkwm_export_application_hidden,
    chunkwm_export_application_terminated,
    chunkwm_export_application_launched,
    chunkwm_export_space_changed,
};
CHUNKWM_PLUGIN_SUBSCRIBE(Subscriptions)

// NOTE(koekeishiya): Generate plugin
CHUNKWM_PLUGIN("Tiling", "0.0.1")