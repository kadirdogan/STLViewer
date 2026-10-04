# STL Viewer

A deliberately old-school/native Windows STL viewer.

## UI direction
- Native Win32 window/menu
- `comctl32.dll` toolbar, TreeView, ListView/property table and status bar
- Visual Styles manifest (`comctl32` v6 / `uxtheme`)
- OpenGL viewport
- Office 2007/2010 / Windows 7-era dense desktop utility layout
- No Qt, Electron, CEF, GTK, WinUI or .NET runtime

## Current features
- Binary and ASCII STL
- File > Open
- command-line / Explorer `%1` path support
- drag & drop
- shaded / wireframe
- grid + XYZ axes
- mouse drag orbit
- mouse wheel zoom
- fit
- front/back/left/right/top/bottom views
- triangle count, file format/size, dimensions and center
- native status bar

## Build (Visual Studio 2022)
Open "x64 Native Tools Command Prompt for VS 2022":

    cd STLViewer
    cmake -S . -B build -A x64
    cmake --build build --config Release

Executable:

    build\Release\STLViewer.exe

You can also open the folder directly in Visual Studio (File > Open > Folder) because it is a CMake project.

## Notes
This first build intentionally uses legacy OpenGL immediate mode so the program stays small and the first native UI/loader prototype is easy to debug. Once the UI and interaction are settled, the renderer can be replaced with VBO/VAO shaders without changing the Win32 shell.

The toolbar currently uses native text buttons as a functional placeholder. The next visual pass should add the small Office-2010-era 16/24 px image strip from the approved mockup.
