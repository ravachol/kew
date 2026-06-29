
Install:
========

Extract this folder anywhere (e.g. C:\kew)

You can just double-click the .exe and run it, but covers likely wont render well in Windows Terminal.

If you want covers displayed correctly, continue with the following steps:

1. Add this to Windows Terminal settings.json but replace YOUR_USERNAME:
   (Settings → open JSON file, add under profiles → list)

   {
       "name": "kew UCRT64",
       "commandline": "C:\\msys64\\usr\\bin\\bash.exe --login",
       "startingDirectory": "C:\\msys64\\home\\YOUR_USERNAME",
       "icon": "C:\\kew\\kew.ico",
       "environment": {
           "CHERE_INVOKING": "1",
           "MSYSTEM": "UCRT64"
       }
   }

2. Open that profile and run: kew

Shortcut:
=========

If you want to create a shortcut, there's an icon included and you can add this as the shortcut path:

C:\Users\YOUR_USERNAME\AppData\Local\Microsoft\WindowsApps\wt.exe -p "kew UCRT64" C:\msys64\usr\bin\bash.exe -li -c "kew"

Note: the paths have to be adjusted for your machine.
