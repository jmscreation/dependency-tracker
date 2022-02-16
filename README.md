# Project Dependency Tracker

 This program tracks and updates dependencies for any repository in this account that uses the new library dependency standard.
 
 
 
 This simple-to-use application will automatically download and update all dependency repositories for any project that uses this dependency system. Keep in mind that this application simply uses `git` to actually perform the download/update process. So you'll need git installed and configured. See below.
 
 
 # How To Build
 
 You can easily run the build script provided for both Linux and Windows.
 
 Linux: `./build.sh`
 Windows: `.\build.bat`
 
 # Installation
 It is important you have `git` installed / and configured in your PATH. This program relies on the *git* command.
 
 ## For Windows
 
 Place *deps.exe* in a binary folder that has its path configured.
 
 For example, you can install it in: `C:\buildtools\deps\deps.exe` then make sure "C:\buildtools\deps" is in your environment PATH.
 
 ## For Linux
 
 Place *deps* in `/usr/bin` to install
 
 # How to Run
 
 Please see the help information within the application for more information on how to track dependencies

 For help, pass the help parameter: `deps -help`
