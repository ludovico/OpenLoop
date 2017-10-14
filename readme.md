The OpenLoop is an experimental project, where the Juce Framework is exposed through Lua bindings.

Originally I wanted to use a lisp-type of language, but Lua was the easiest to bind, thanks to Sol2, a Lua binding library. I have no idea how complex the Juce Framework is, and if this will work, but inital tests seems promising. I am not a c++ coder - I'm not even a coder, I am a musician - so there will be bugs.

My goal is to open the c++ audio ecosystem - especially audio plugins and hosting, which usually are either low level, static c++, or locked inside a daw, behind a UI - for dynamic programming.

SuperCollider introduced me to programming in 2010 - I really loved the way to work and think, but sadly it has no vst support, which makes great-sounding code (u-he, falcon, fabfilter etc.) unavailable. This projects aims to be close to SuperCollider in spirit.

I use sublime text 3, for the simple reason that I can send snippets of (syntax-highlighted) Lua code via TCP to OpenLoop (which in turn manipulates low-level c++ juce stuff). 

I hope someone forks this idea! And implements it in lisp.



..if you want to test it, you have to have Projucer installed, be sure to read the doc in Resources as well. Maybe you'll get it to work. I'm compiling for Windows 10, 64 bit, but Lua and Juce are cross-platform so it might work fine on other platforms as well.