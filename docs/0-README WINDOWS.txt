1. Extract this folder anywhere (e.g. C:\kew)

2. Add this to Windows Terminal settings.json
   (Settings → open JSON file, add under profiles → list):

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

3. Open that profile and run: kew

You can still just double-click the .exe and run it, but covers likely wont render well in Windows Terminal.