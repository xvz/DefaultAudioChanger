#include "stdafx.h"
#include "DefaultAudioChanger.h"

#define MAX_LOADSTRING 100
#define	WM_USER_SHELLICON WM_USER + 1

// Global Variables:
HINSTANCE hInst;                              // current instance
NOTIFYICONDATA nidApp;
HMENU hPopMenu;
TCHAR szTitle[MAX_LOADSTRING];                // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];	        // the main window class name
TCHAR szApplicationToolTip[MAX_LOADSTRING];   // the main window class name

BOOL isDisabled = FALSE;           // all options initially enabled
UINT defaultDevId = 0xDEADBEEF;    // deviceId of selected default device

/*
 * Forward declarations
 */
HRESULT SetDefaultAudioPlaybackDevice(LPCWSTR);
HRESULT ChangeAudioPlaybackDevice(HMENU, int);
void DeviceMenuMake();
void DeviceMenuSelect(int);

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);


/***************************************************
* Modified code from EndPointController.cpp.
* http://www.daveamenta.com/2011-05/programmatically-or-command-line-change-the-default-sound-playback-device-in-windows-7/
*
* This is basically hack to set default audio device on Vista/Win7.
* On XP & Win2000, this could be done by sending DRVM_MAPPER_PREFERRED_SET via waveOutMessage().
***************************************************/
#include "Mmdeviceapi.h"
#include "PolicyConfig.h"
#include "Propidl.h"
#include "Functiondiscoverykeys_devpkey.h"

/*
 * Sets default audio playback device. Note that argument
 * is string (of sorts) rather than int.
 */
HRESULT SetDefaultAudioPlaybackDevice(LPCWSTR devID) {	
  IPolicyConfigVista *pPolicyConfig;
  ERole reserved = eConsole;
  
  HRESULT hr = CoCreateInstance(__uuidof(CPolicyConfigVistaClient), 
                                NULL, CLSCTX_ALL, __uuidof(IPolicyConfigVista), (LPVOID *)&pPolicyConfig);
  if (SUCCEEDED(hr)) {
    hr = pPolicyConfig->SetDefaultEndpoint(devID, reserved);
    pPolicyConfig->Release();
  }
  return hr;
}

/*
* Lists or changes default audio playback device.
*
* If option is -1, audio devices are inserted as options to
* provided HMENU with option "opt_flag". Otherwise, default
* playback device with DeviceId "option" is selected.
*/
HRESULT ChangeAudioPlaybackDevice(HMENU hPopMenu, int option) {
  HRESULT hr = CoInitialize(NULL);

  // TODO: awful awful if statement nesting
  if (SUCCEEDED(hr)) {
    IMMDeviceEnumerator *pEnum = NULL;
    // Create a multimedia device enumerator.
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
                          CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);

    if (SUCCEEDED(hr)) {
      IMMDeviceCollection *pDevices;
      // Enumerate the output devices.
      hr = pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices);
      
      if (SUCCEEDED(hr)) {
        UINT count;
        pDevices->GetCount(&count);

        if (SUCCEEDED(hr)) {
          // format flags for menu items
          UINT uFlag;
          UINT defFlag = MF_BYPOSITION|MF_STRING;
          if ( isDisabled ) { defFlag |= MF_GRAYED; }

          for (int i = 0; i < count; i++) {
            IMMDevice *pDevice;
            hr = pDevices->Item(i, &pDevice);
     
            if (SUCCEEDED(hr)) {
              LPWSTR wstrID = NULL;
              hr = pDevice->GetId(&wstrID);
            
              if (SUCCEEDED(hr)) {
                IPropertyStore *pStore;
                hr = pDevice->OpenPropertyStore(STGM_READ, &pStore);
                
                if (SUCCEEDED(hr)) {
                  PROPVARIANT friendlyName;
                  PropVariantInit(&friendlyName);
                  hr = pStore->GetValue(PKEY_Device_FriendlyName, &friendlyName);

                  if (SUCCEEDED(hr)) {
                    uFlag = defFlag;      // reset flags

                    // If no options, create menu item for device.
                    if (option == -1) {
                      if( i == defaultDevId ) {
                        uFlag |= MF_CHECKED;
                      } else { 
                        uFlag |= MF_UNCHECKED;
                      }

                      // IDM_DEVICE_CHOICE is sentinel value to ensure
                      // selection #s are more unique than just 0, 1, etc.
                      InsertMenu(hPopMenu,0xFFFFFFFF,uFlag,i+IDM_DEVICE_CHOICE,friendlyName.pwszVal);
                    }
                    
                    // Otherwise, find selected device & set it to be default
                    if (i == option) {
                      hr = SetDefaultAudioPlaybackDevice(wstrID);
                      InsertMenu(hPopMenu,0xFFFFFFFF,uFlag|MF_CHECKED,i+IDM_DEVICE_CHOICE,friendlyName.pwszVal);
                      defaultDevId = i;
                    }
                    PropVariantClear(&friendlyName);
                  }
                  pStore->Release();
                }
              }
              pDevice->Release();
            }
          }
        }
        pDevices->Release();
      }
      pEnum->Release();
    }
  }
  return hr;
}

/*
* Wrapper function for creating menus
*/
void DeviceMenuMake() {
  HRESULT ret = ChangeAudioPlaybackDevice(hPopMenu, -1);
  
  if( !SUCCEEDED(ret) ) {
    MessageBox(NULL,_T("Failed to draw menu!"), _T("Oops!"),MB_OK|MB_ICONERROR);
  }
}

/*
* Wrapper function for changing device
*/
void DeviceMenuSelect(int selectionId) {
  HRESULT ret = ChangeAudioPlaybackDevice(hPopMenu, selectionId-IDM_DEVICE_CHOICE);
  
  if( !SUCCEEDED(ret) ) {
    MessageBox(NULL,_T("Failed to set default device!"),_T("Oops!"),MB_OK|MB_ICONERROR);
  }
}



/*******************************
 * Modified code based heavily on SysTrayDemo, available online.
 *******************************/
int APIENTRY _tWinMain(HINSTANCE hInstance,
                       HINSTANCE hPrevInstance,
                       LPTSTR    lpCmdLine,
                       int       nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  
  // TODO: Place code here.
  MSG msg;
  HACCEL hAccelTable;
  
  // Initialize global strings
  LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
  LoadString(hInstance, IDC_DEFAULTAUDIOCHANGER, szWindowClass, MAX_LOADSTRING);
  
  MyRegisterClass(hInstance);
  
  // Perform application initialization:
  if (!InitInstance (hInstance, nCmdShow)) {
    return FALSE;
  }
  
  hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DEFAULTAUDIOCHANGER));
  
  // Main message loop:
  while (GetMessage(&msg, NULL, 0, 0)) {
    if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  
  return (int) msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance) {
  WNDCLASSEX wcex;
  
  wcex.cbSize = sizeof(WNDCLASSEX);
  
  wcex.style         = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc   = WndProc;
  wcex.cbClsExtra    = 0;
  wcex.cbWndExtra    = 0;
  wcex.hInstance     = hInstance;
  wcex.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DEFAULTAUDIOCHANGER));
  wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
  wcex.lpszMenuName  = MAKEINTRESOURCE(IDC_DEFAULTAUDIOCHANGER);
  wcex.lpszClassName = szWindowClass;
  wcex.hIconSm       = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_DEFAULTAUDIOCHANGER));

  return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
  HWND hWnd;
  HICON hMainIcon;
  
  hInst = hInstance; // Store instance handle in our global variable
  
  hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
                      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
  
  if (!hWnd) {
    return FALSE;
  }
  
  hMainIcon = LoadIcon(hInstance,(LPCTSTR)MAKEINTRESOURCE(IDI_DEFAULTAUDIOCHANGER)); 
  
  nidApp.cbSize = sizeof(NOTIFYICONDATA); // sizeof the struct in bytes 
  nidApp.hWnd = (HWND) hWnd;              // handle of the window which will process this app. messages 
  nidApp.uID = IDI_DEFAULTAUDIOCHANGER;   // ID of the icon that willl appear in the system tray 
  nidApp.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; // ORing of all the flags 
  nidApp.hIcon = hMainIcon;               // handle of the Icon to be displayed, obtained from LoadIcon 
  nidApp.uCallbackMessage = WM_USER_SHELLICON; 
  LoadString(hInstance, IDS_APPTOOLTIP,nidApp.szTip,MAX_LOADSTRING);
  Shell_NotifyIcon(NIM_ADD, &nidApp); 
  
  return TRUE;
}

void Init() {
  // user defined message that will be sent as the notification message to the Window Procedure 
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  int wmId, wmEvent;
  POINT lpClickPoint;
  
  switch (message) {
    case WM_USER_SHELLICON: 
      // systray msg callback 
      switch(LOWORD(lParam)) {   
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN: 
          GetCursorPos(&lpClickPoint);
          hPopMenu = CreatePopupMenu();
          
          DeviceMenuMake();
          
          InsertMenu(hPopMenu,0xFFFFFFFF,MF_SEPARATOR,IDM_SEP,_T("SEP"));				
          if ( isDisabled ) {
            InsertMenu(hPopMenu,0xFFFFFFFF,MF_BYPOSITION|MF_STRING,IDM_ENABLE,_T("Enable"));			
          } else {
            InsertMenu(hPopMenu,0xFFFFFFFF,MF_BYPOSITION|MF_STRING,IDM_DISABLE,_T("Disable"));			 
          }
          
          InsertMenu(hPopMenu,0xFFFFFFFF,MF_SEPARATOR,IDM_SEP,_T("SEP"));				
          InsertMenu(hPopMenu,0xFFFFFFFF,MF_BYPOSITION|MF_STRING,IDM_ABOUT,_T("About"));
          InsertMenu(hPopMenu,0xFFFFFFFF,MF_BYPOSITION|MF_STRING,IDM_EXIT,_T("Exit"));
          
          SetForegroundWindow(hWnd);
          TrackPopupMenu(hPopMenu,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_BOTTOMALIGN,
                         lpClickPoint.x, lpClickPoint.y,0,hWnd,NULL);
          return TRUE; 
      }
      break;
      
    case WM_COMMAND:
      {
        wmId    = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        
        // Parse the menu selections
        // Audio selections are a bit wonky, so don't use switch
        if( wmId >= IDM_DEVICE_CHOICE && wmId < IDM_DEVICE_CHOICE_MAX ) {
          DeviceMenuSelect(wmId);
        } else {
          // Otherwise, misc option was selected
          switch (wmId) {
            case IDM_ABOUT:
              DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
              break;
            case IDM_DISABLE:
              isDisabled = TRUE;
              break;
            case IDM_ENABLE:
              isDisabled = FALSE;
              break;
            case IDM_EXIT:
              Shell_NotifyIcon(NIM_DELETE,&nidApp);
              DestroyWindow(hWnd);
              break;
            default:
              return DefWindowProc(hWnd, message, wParam, lParam);
          }
        }
        break;
      }
      
      /*
        case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        // TODO: Add any drawing code here...
        EndPaint(hWnd, &ps);
        break;
      */
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  switch (message) {
    case WM_INITDIALOG:
      return (INT_PTR)TRUE;
      
    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
        EndDialog(hDlg, LOWORD(wParam));
        return (INT_PTR)TRUE;
      }
      break;
  }
  return (INT_PTR)FALSE;
}
