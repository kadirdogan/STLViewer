#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <uxtheme.h>
#include <shellapi.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "resource.h"
#include "stl.h"

#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"comdlg32.lib")
#pragma comment(lib,"uxtheme.lib")
#pragma comment(lib,"opengl32.lib")
#pragma comment(lib,"glu32.lib")

static HINSTANCE gInst;
static HWND gMain,gToolbar,gTree,gProps,gView,gStatus;
static STLModel gModel;
static bool gLoaded=false,gWire=false,gGrid=true,gAxes=true;
static float gYaw=-32.f,gPitch=22.f,gZoom=1.f;
static POINT gLast{};
static bool gDrag=false;
static HGLRC gRC=nullptr;
static HDC gDC=nullptr;

static std::wstring F(float v) {
    std::wostringstream s; s<<std::fixed<<std::setprecision(3)<<v; return s.str();
}
static std::wstring SizeText(uint64_t b) {
    std::wostringstream s;
    if(b>1024ull*1024) s<<std::fixed<<std::setprecision(1)<<(double)b/(1024*1024)<<L" MB";
    else s<<b/1024<<L" KB";
    return s.str();
}
static void AddProp(const wchar_t* p,const std::wstring& v) {
    int row=ListView_GetItemCount(gProps);
    LVITEM li{}; li.mask=LVIF_TEXT; li.iItem=row; li.pszText=(LPWSTR)p;
    ListView_InsertItem(gProps,&li);
    ListView_SetItemText(gProps,row,1,(LPWSTR)v.c_str());
}
static void RefreshInfo() {
    TreeView_DeleteAllItems(gTree);
    ListView_DeleteAllItems(gProps);
    if(!gLoaded) return;
    TVINSERTSTRUCT ti{}; ti.hParent=TVI_ROOT; ti.hInsertAfter=TVI_LAST;
    ti.item.mask=TVIF_TEXT; ti.item.pszText=(LPWSTR)gModel.filename.c_str();
    HTREEITEM root=TreeView_InsertItem(gTree,&ti);
    std::wstring tris=L"Triangles: "+std::to_wstring(gModel.triangles.size());
    ti.hParent=root; ti.item.pszText=(LPWSTR)tris.c_str(); TreeView_InsertItem(gTree,&ti);
    std::wstring ctr=L"Center ("+F(gModel.center.x)+L", "+F(gModel.center.y)+L", "+F(gModel.center.z)+L")";
    ti.item.pszText=(LPWSTR)ctr.c_str(); TreeView_InsertItem(gTree,&ti);
    TreeView_Expand(gTree,root,TVE_EXPAND);

    AddProp(L"File name",gModel.filename);
    AddProp(L"Format",gModel.format);
    AddProp(L"File size",SizeText(gModel.fileSize));
    AddProp(L"Triangles",std::to_wstring(gModel.triangles.size()));
    AddProp(L"X (mm)",F(gModel.max.x-gModel.min.x));
    AddProp(L"Y (mm)",F(gModel.max.y-gModel.min.y));
    AddProp(L"Z (mm)",F(gModel.max.z-gModel.min.z));
    AddProp(L"Center X",F(gModel.center.x));
    AddProp(L"Center Y",F(gModel.center.y));
    AddProp(L"Center Z",F(gModel.center.z));

    std::wstring dim=F(gModel.max.x-gModel.min.x)+L" x "+F(gModel.max.y-gModel.min.y)+L" x "+F(gModel.max.z-gModel.min.z)+L" mm";
    SendMessage(gStatus,SB_SETTEXT,1,(LPARAM)(L"  "+std::to_wstring(gModel.triangles.size())+L" triangles").c_str());
    SendMessage(gStatus,SB_SETTEXT,2,(LPARAM)(L"  "+dim).c_str());
    SetWindowText(gMain,(L"STL Viewer - "+gModel.filename).c_str());
}
static void Fit() { gZoom=1.f; InvalidateRect(gView,nullptr,FALSE); }

static void OpenFile(const wchar_t* direct=nullptr) {
    wchar_t path[MAX_PATH]{};
    if(direct) lstrcpyn(path,direct,MAX_PATH);
    else {
        OPENFILENAME ofn{sizeof(ofn)};
        ofn.hwndOwner=gMain; ofn.lpstrFilter=L"STL files (*.stl)\0*.stl\0All files\0*.*\0";
        ofn.lpstrFile=path; ofn.nMaxFile=MAX_PATH; ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
        if(!GetOpenFileName(&ofn)) return;
    }
    std::wstring err;
    if(!LoadSTL(path,gModel,err)){MessageBox(gMain,err.c_str(),L"STL Viewer",MB_ICONERROR);return;}
    gLoaded=true; gYaw=-32; gPitch=22; Fit(); RefreshInfo(); InvalidateRect(gView,nullptr,FALSE);
}

static void SetupPixel(HWND h) {
    gDC=GetDC(h);
    PIXELFORMATDESCRIPTOR p{sizeof(p),1,PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,32,0,0,0,0,0,0,0,0,0,0,0,0,24,8,0,PFD_MAIN_PLANE,0,0,0,0};
    int pf=ChoosePixelFormat(gDC,&p); SetPixelFormat(gDC,pf,&p);
    gRC=wglCreateContext(gDC); wglMakeCurrent(gDC,gRC);
    glEnable(GL_DEPTH_TEST); glEnable(GL_NORMALIZE); glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    GLfloat amb[]={.25f,.25f,.28f,1}, dif[]={.85f,.85f,.85f,1};
    glLightfv(GL_LIGHT0,GL_AMBIENT,amb); glLightfv(GL_LIGHT0,GL_DIFFUSE,dif);
}
static void DrawGrid(float s) {
    if(!gGrid)return; glDisable(GL_LIGHTING); glColor3f(.32f,.36f,.39f);
    glBegin(GL_LINES); for(int i=-10;i<=10;i++){float p=i*s/10; glVertex3f(-s,p,0);glVertex3f(s,p,0);glVertex3f(p,-s,0);glVertex3f(p,s,0);} glEnd();
    glEnable(GL_LIGHTING);
}
static void DrawAxes(float s) {
    if(!gAxes)return; glDisable(GL_LIGHTING); glLineWidth(2);
    glBegin(GL_LINES); glColor3f(1,0,0);glVertex3f(0,0,0);glVertex3f(s,0,0);
    glColor3f(0,1,0);glVertex3f(0,0,0);glVertex3f(0,s,0);
    glColor3f(0,0.45f,1);glVertex3f(0,0,0);glVertex3f(0,0,s); glEnd();
    glLineWidth(1); glEnable(GL_LIGHTING);
}
static void Render(HWND h) {
    if(!gRC)return; wglMakeCurrent(gDC,gRC);
    RECT r; GetClientRect(h,&r); int w=std::max(1L,r.right),hh=std::max(1L,r.bottom);
    glViewport(0,0,w,hh); glClearColor(.12f,.15f,.18f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); gluPerspective(45.0,(double)w/hh,.1,10000);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    float span=100;
    if(gLoaded) span=std::max({gModel.max.x-gModel.min.x,gModel.max.y-gModel.min.y,gModel.max.z-gModel.min.z,1.f});
    gluLookAt(0,-span*2.4/gZoom,span*1.4/gZoom, 0,0,0, 0,0,1);
    glRotatef(gPitch,1,0,0); glRotatef(gYaw,0,0,1);
    DrawGrid(span*1.5f); DrawAxes(span*.35f);
    if(gLoaded) {
        glTranslatef(-gModel.center.x,-gModel.center.y,-gModel.center.z);
        GLfloat mat[]={.72f,.74f,.76f,1}; glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,mat);
        glPolygonMode(GL_FRONT_AND_BACK,gWire?GL_LINE:GL_FILL);
        glBegin(GL_TRIANGLES);
        for(auto& t:gModel.triangles){glNormal3f(t.n.x,t.n.y,t.n.z);glVertex3f(t.a.x,t.a.y,t.a.z);glVertex3f(t.b.x,t.b.y,t.b.z);glVertex3f(t.c.x,t.c.y,t.c.z);}
        glEnd(); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    }
    SwapBuffers(gDC);
}
static void Layout(HWND h) {
    RECT r; GetClientRect(h,&r);
    SendMessage(gToolbar,TB_AUTOSIZE,0,0); RECT tb; GetWindowRect(gToolbar,&tb); int th=tb.bottom-tb.top;
    SendMessage(gStatus,WM_SIZE,0,0); RECT sb; GetWindowRect(gStatus,&sb); int sh=sb.bottom-sb.top;
    int left=325, top=th, bottom=r.bottom-sh;
    MoveWindow(gTree,8,top+8,left-16,220,TRUE);
    MoveWindow(gProps,8,top+236,left-16,std::max(50,bottom-top-244),TRUE);
    MoveWindow(gView,left,top,r.right-left,bottom-top,TRUE);
    int parts[]={r.right-430,r.right-230,-1}; SendMessage(gStatus,SB_SETPARTS,3,(LPARAM)parts);
}
static HMENU MakeMenu() {
    HMENU bar=CreateMenu(), file=CreatePopupMenu(), view=CreatePopupMenu(), render=CreatePopupMenu(), help=CreatePopupMenu();
    AppendMenu(file,MF_STRING,IDM_OPEN,L"&Open...\tCtrl+O"); AppendMenu(file,MF_SEPARATOR,0,nullptr); AppendMenu(file,MF_STRING,IDM_EXIT,L"E&xit");
    AppendMenu(view,MF_STRING,IDM_FIT,L"&Fit to window"); AppendMenu(view,MF_STRING,IDM_GRID,L"&Grid"); AppendMenu(view,MF_STRING,IDM_AXES,L"&Axes");
    AppendMenu(render,MF_STRING,IDM_SHADED,L"&Shaded"); AppendMenu(render,MF_STRING,IDM_WIREFRAME,L"&Wireframe");
    AppendMenu(help,MF_STRING,IDM_ABOUT,L"&About");
    AppendMenu(bar,MF_POPUP,(UINT_PTR)file,L"&File"); AppendMenu(bar,MF_POPUP,(UINT_PTR)view,L"&View"); AppendMenu(bar,MF_POPUP,(UINT_PTR)render,L"&Render"); AppendMenu(bar,MF_POPUP,(UINT_PTR)help,L"&Help");
    return bar;
}
static void AddToolbarButton(int id,int image,const wchar_t* text,BYTE style=BTNS_BUTTON) {
    TBBUTTON b{}; b.iBitmap=image; b.idCommand=id; b.fsState=TBSTATE_ENABLED; b.fsStyle=style|BTNS_AUTOSIZE|BTNS_SHOWTEXT; b.iString=(INT_PTR)text;
    SendMessage(gToolbar,TB_ADDBUTTONS,1,(LPARAM)&b);
}
static LRESULT CALLBACK ViewProc(HWND h,UINT m,WPARAM w,LPARAM l) {
    switch(m){
    case WM_CREATE: SetupPixel(h); return 0;
    case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);Render(h);EndPaint(h,&ps);return 0;}
    case WM_LBUTTONDOWN:gDrag=true;gLast={GET_X_LPARAM(l),GET_Y_LPARAM(l)};SetCapture(h);return 0;
    case WM_LBUTTONUP:gDrag=false;ReleaseCapture();return 0;
    case WM_MOUSEMOVE: if(gDrag){POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};gYaw+=(p.x-gLast.x)*.5f;gPitch+=(p.y-gLast.y)*.5f;gLast=p;InvalidateRect(h,nullptr,FALSE);} return 0;
    case WM_MOUSEWHEEL:gZoom*=GET_WHEEL_DELTA_WPARAM(w)>0?1.12f:.89f;gZoom=std::clamp(gZoom,.08f,30.f);InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_DESTROY: if(gRC){wglMakeCurrent(nullptr,nullptr);wglDeleteContext(gRC);gRC=nullptr;} if(gDC)ReleaseDC(h,gDC); return 0;
    } return DefWindowProc(h,m,w,l);
}
static LRESULT CALLBACK MainProc(HWND h,UINT m,WPARAM w,LPARAM l) {
    switch(m){
    case WM_CREATE:{
        gToolbar=CreateWindowEx(0,TOOLBARCLASSNAME,nullptr,WS_CHILD|WS_VISIBLE|TBSTYLE_FLAT|TBSTYLE_LIST|CCS_TOP,0,0,0,0,h,(HMENU)IDC_TOOLBAR,gInst,nullptr);
        SendMessage(gToolbar,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0);
        SendMessage(gToolbar,TB_SETEXTENDEDSTYLE,0,TBSTYLE_EX_MIXEDBUTTONS);
        AddToolbarButton(IDM_OPEN,I_IMAGENONE,L"Open");
        AddToolbarButton(IDM_FIT,I_IMAGENONE,L"Fit");
        AddToolbarButton(IDM_FRONT,I_IMAGENONE,L"Front"); AddToolbarButton(IDM_BACK,I_IMAGENONE,L"Back");
        AddToolbarButton(IDM_LEFT,I_IMAGENONE,L"Left"); AddToolbarButton(IDM_RIGHT,I_IMAGENONE,L"Right");
        AddToolbarButton(IDM_TOP,I_IMAGENONE,L"Top"); AddToolbarButton(IDM_BOTTOM,I_IMAGENONE,L"Bottom");
        AddToolbarButton(IDM_SHADED,I_IMAGENONE,L"Shaded"); AddToolbarButton(IDM_WIREFRAME,I_IMAGENONE,L"Wireframe");
        AddToolbarButton(IDM_GRID,I_IMAGENONE,L"Grid"); AddToolbarButton(IDM_AXES,I_IMAGENONE,L"Axes");

        gTree=CreateWindowEx(WS_EX_CLIENTEDGE,WC_TREEVIEW,nullptr,WS_CHILD|WS_VISIBLE|TVS_HASLINES|TVS_LINESATROOT|TVS_HASBUTTONS,0,0,0,0,h,(HMENU)IDC_TREE,gInst,nullptr);
        gProps=CreateWindowEx(WS_EX_CLIENTEDGE,WC_LISTVIEW,nullptr,WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL,0,0,0,0,h,(HMENU)IDC_PROPS,gInst,nullptr);
        ListView_SetExtendedListViewStyle(gProps,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
        LVCOLUMN c{};c.mask=LVCF_TEXT|LVCF_WIDTH;c.cx=145;c.pszText=(LPWSTR)L"Property";ListView_InsertColumn(gProps,0,&c);c.cx=145;c.pszText=(LPWSTR)L"Value";ListView_InsertColumn(gProps,1,&c);
        gView=CreateWindowEx(WS_EX_CLIENTEDGE,L"STLView",nullptr,WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,0,0,0,0,h,(HMENU)IDC_VIEW,gInst,nullptr);
        gStatus=CreateWindowEx(0,STATUSCLASSNAME,nullptr,WS_CHILD|WS_VISIBLE|SBARS_SIZEGRIP,0,0,0,0,h,(HMENU)IDC_STATUS,gInst,nullptr);
        SendMessage(gStatus,SB_SETTEXT,0,(LPARAM)L"Ready");
        SetWindowTheme(gTree,L"Explorer",nullptr); SetWindowTheme(gProps,L"Explorer",nullptr);
        DragAcceptFiles(h,TRUE); Layout(h); return 0;
    }
    case WM_SIZE: Layout(h); return 0;
    case WM_DROPFILES:{HDROP d=(HDROP)w;wchar_t p[MAX_PATH];if(DragQueryFile(d,0,p,MAX_PATH))OpenFile(p);DragFinish(d);return 0;}
    case WM_COMMAND:{
        switch(LOWORD(w)){
        case IDM_OPEN:OpenFile();break; case IDM_EXIT:DestroyWindow(h);break; case IDM_FIT:Fit();break;
        case IDM_FRONT:gYaw=0;gPitch=90;InvalidateRect(gView,nullptr,FALSE);break;
        case IDM_BACK:gYaw=180;gPitch=90;InvalidateRect(gView,nullptr,FALSE);break;
        case IDM_LEFT:gYaw=90;gPitch=90;InvalidateRect(gView,nullptr,FALSE);break;
        case IDM_RIGHT:gYaw=-90;gPitch=90;InvalidateRect(gView,nullptr,FALSE);break;
        case IDM_TOP:gYaw=0;gPitch=0;InvalidateRect(gView,nullptr,FALSE);break;
        case IDM_BOTTOM:gYaw=0;gPitch=180;InvalidateRect(gView,nullptr,FALSE);break;
        case IDM_SHADED:gWire=false;InvalidateRect(gView,nullptr,FALSE);break;
        case IDM_WIREFRAME:gWire=true;InvalidateRect(gView,nullptr,FALSE);break;
        case IDM_GRID:gGrid=!gGrid;InvalidateRect(gView,nullptr,FALSE);break;
        case IDM_AXES:gAxes=!gAxes;InvalidateRect(gView,nullptr,FALSE);break;
        case IDM_ABOUT:MessageBox(h,L"STL Viewer\nNative Win32 + Common Controls + OpenGL",L"About",MB_OK|MB_ICONINFORMATION);break;
        } return 0;
    }
    case WM_DESTROY:PostQuitMessage(0);return 0;
    } return DefWindowProc(h,m,w,l);
}
int APIENTRY wWinMain(HINSTANCE hi,HINSTANCE,LPWSTR cmd,int show) {
    gInst=hi;
    INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_WIN95_CLASSES|ICC_BAR_CLASSES|ICC_TREEVIEW_CLASSES|ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&ic);
    WNDCLASSEX vc{sizeof(vc)};vc.style=CS_OWNDC|CS_HREDRAW|CS_VREDRAW;vc.lpfnWndProc=ViewProc;vc.hInstance=hi;vc.hCursor=LoadCursor(nullptr,IDC_ARROW);vc.lpszClassName=L"STLView";RegisterClassEx(&vc);
    WNDCLASSEX wc{sizeof(wc)};wc.lpfnWndProc=MainProc;wc.hInstance=hi;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hIcon=LoadIcon(nullptr,IDI_APPLICATION);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName=L"STLViewerMain";RegisterClassEx(&wc);
    gMain=CreateWindowEx(0,wc.lpszClassName,L"STL Viewer",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1280,800,nullptr,MakeMenu(),hi,nullptr);
    ShowWindow(gMain,show);UpdateWindow(gMain);
    if(cmd && *cmd){std::wstring p=cmd;if(p.size()>1&&p.front()==L'"'&&p.back()==L'"')p=p.substr(1,p.size()-2);OpenFile(p.c_str());}
    MSG msg;while(GetMessage(&msg,nullptr,0,0)){TranslateMessage(&msg);DispatchMessage(&msg);}return (int)msg.wParam;
}
