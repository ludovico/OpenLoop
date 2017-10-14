lua:

expected to be at c:\code, so copy it to there - or change the include and lib settings in projucer.

Settings for visual studio 2017:
Lua .lib: c:\code\lua\bin\lua53.lib
Lua includes: c:\code\lua\bin\include

!! In addition, lua53.dll has to be in the same folder as OpenLoop.exe !!




sublime:

expected to be at ~/AppData/Roaming/Sublime Text 3/Packages/User

sends lua snippets to OpenLoop via TCP.

Ctrl+Enter to send a selection, or the line if selection is empty.