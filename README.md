Radium Window Manager
============================

Requirements
------------
In order to build radium you need the Xlib and Lua 5.4 header files.

Installation
------------
Run the following command to build and install radium (if
necessary as root):

    make clean install


Running radium
-----------
Add the following line to your .xinitrc to start radium using startx:

    exec radium

Configuration
-------------
radium runs ~/.config/radium/init.lua when it starts, or
$XDG_CONFIG_HOME/radium/init.lua when XDG_CONFIG_HOME is set.

Testing
-------
    Xephyr :1 -screen 1280x720 -ac -br -noreset & DISPLAY=:1 ./build/radium