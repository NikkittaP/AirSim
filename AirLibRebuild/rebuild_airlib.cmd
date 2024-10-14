@echo off
REM //---------- set up variable ----------
setlocal
set ROOT_DIR=%~dp0

REM Assume we run script from AirSim plugin folder
set SOURCE_DIR=%ROOT_DIR%\..\Source\AirLib
set TEMP_BUILD_DIR=%ROOT_DIR%
set TEMP_BUILD_AIRLIB_DIR=%TEMP_BUILD_DIR%\AirLib
set DEST_DIR=%SOURCE_DIR%
set PROP_FILE=%ROOT_DIR%\AirSim.props

REM Create temp build directory if it doesn't exist
if not exist "%TEMP_BUILD_DIR%" (
    mkdir "%TEMP_BUILD_DIR%"
)
if not exist "%TEMP_BUILD_AIRLIB_DIR%" (
    mkdir "%TEMP_BUILD_AIRLIB_DIR%"
)

REM Copy files from source to temp build directory, overwriting existing files
robocopy "%SOURCE_DIR%" "%TEMP_BUILD_AIRLIB_DIR%" /MIR /njh /njs /ndl /np

REM //check VS version
if "%VisualStudioVersion%" == "" (
    echo(
    echo oh oh... You need to run this command from x64 Native Tools Command Prompt for VS 2022.
    goto :buildfailed_nomsg
)
if "%VisualStudioVersion%" lss "17.0" (
    echo(
    echo Hello there! We just upgraded AirSim to Unreal Engine 4.27 and Visual Studio 2022.
    echo Here are few easy steps for upgrade so everything is new and shiny:
    echo https://github.com/Microsoft/AirSim/blob/main/docs/unreal_upgrade.md
    goto :buildfailed_nomsg
)

if "%1"=="" goto noargs
if "%1"=="--no-full-poly-car" set "noFullPolyCar=y"
if "%1"=="--Debug" set "buildMode=Debug"
if "%1"=="--Release" set "buildMode=Release"
if "%1"=="--RelWithDebInfo" set "buildMode=RelWithDebInfo"

if "%2"=="" goto noargs
if "%2"=="--Debug" set "buildMode=Debug"
if "%2"=="--Release" set "buildMode=Release"
if "%2"=="--RelWithDebInfo" set "buildMode=RelWithDebInfo"

:noargs

set powershell=powershell
where powershell > nul 2>&1
if ERRORLEVEL 1 goto :pwsh
echo found Powershell && goto start
:pwsh
set powershell=pwsh
where pwsh > nul 2>&1
if ERRORLEVEL 1 goto :nopwsh
set PWSHV7=1
echo found pwsh && goto start
:nopwsh
echo Powershell or pwsh not found, please install it.
goto :eof

:start

REM //---------- Check cmake version ----------
CALL check_cmake.bat
if ERRORLEVEL 1 (
  CALL check_cmake.bat
  if ERRORLEVEL 1 (
    echo(
    echo ERROR: cmake was not installed correctly, we tried.
    goto :buildfailed
  )
)

chdir /d %TEMP_BUILD_DIR% 

REM //---------- get rpclib ----------
IF NOT EXIST external\rpclib mkdir external\rpclib

set RPC_VERSION_FOLDER=rpclib-2.3.0
IF NOT EXIST external\rpclib\%RPC_VERSION_FOLDER% (
    REM //leave some blank lines because %powershell% shows download banner at top of console
    ECHO(
    ECHO(   
    ECHO(   
    ECHO *****************************************************************************************
    ECHO Downloading rpclib
    ECHO *****************************************************************************************
    @echo on
    if "%PWSHV7%" == "" (
        %powershell% -command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; iwr https://github.com/rpclib/rpclib/archive/v2.3.0.zip -OutFile external\rpclib.zip }"
    ) else (
        %powershell% -command "iwr https://github.com/rpclib/rpclib/archive/v2.3.0.zip -OutFile external\rpclib.zip"
    )
    @echo off
    
    REM //remove any previous versions
    rmdir external\rpclib /q /s

    %powershell% -command "Expand-Archive -Path external\rpclib.zip -DestinationPath external\rpclib"
    del external\rpclib.zip /q
    
    REM //Fail the build if unable to download rpclib
    IF NOT EXIST external\rpclib\%RPC_VERSION_FOLDER% (
        ECHO Unable to download rpclib, stopping build
        goto :buildfailed
    )
)

REM //---------- Build rpclib ------------
ECHO Starting cmake to build rpclib...
IF NOT EXIST external\rpclib\%RPC_VERSION_FOLDER%\build mkdir external\rpclib\%RPC_VERSION_FOLDER%\build
cd external\rpclib\%RPC_VERSION_FOLDER%\build
cmake -G"Visual Studio 17 2022" ..

if "%buildMode%" == "" (
cmake --build . 
cmake --build . --config Release
) else (
cmake --build . --config %buildMode%
)

if ERRORLEVEL 1 goto :buildfailed
chdir /d %TEMP_BUILD_DIR% 

REM //---------- copy rpclib binaries and include folder inside AirLib folder ----------
set RPCLIB_TARGET_LIB=AirLib\deps\rpclib\lib\x64
if NOT exist %RPCLIB_TARGET_LIB% mkdir %RPCLIB_TARGET_LIB%
set RPCLIB_TARGET_INCLUDE=AirLib\deps\rpclib\include
if NOT exist %RPCLIB_TARGET_INCLUDE% mkdir %RPCLIB_TARGET_INCLUDE%
robocopy /MIR external\rpclib\%RPC_VERSION_FOLDER%\include %RPCLIB_TARGET_INCLUDE%

if "%buildMode%" == "" (
robocopy /MIR external\rpclib\%RPC_VERSION_FOLDER%\build\Debug %RPCLIB_TARGET_LIB%\Debug
robocopy /MIR external\rpclib\%RPC_VERSION_FOLDER%\build\Release %RPCLIB_TARGET_LIB%\Release
) else (
robocopy /MIR external\rpclib\%RPC_VERSION_FOLDER%\build\%buildMode% %RPCLIB_TARGET_LIB%\%buildMode%
)

REM //---------- get Eigen library ----------
IF NOT EXIST AirLib\deps mkdir AirLib\deps
IF NOT EXIST AirLib\deps\eigen3 (
    if "%PWSHV7%" == "" (
        %powershell% -command "& { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; iwr https://gitlab.com/libeigen/eigen/-/archive/3.3.7/eigen-3.3.7.zip -OutFile eigen3.zip }"
    ) else (
        %powershell% -command "iwr https://gitlab.com/libeigen/eigen/-/archive/3.3.7/eigen-3.3.7.zip -OutFile eigen3.zip"
    )
    %powershell% -command "Expand-Archive -Path eigen3.zip -DestinationPath AirLib\deps"
    %powershell% -command "Move-Item -Path AirLib\deps\eigen* -Destination AirLib\deps\del_eigen"
    REM move AirLib\deps\eigen* AirLib\deps\del_eigen
    mkdir AirLib\deps\eigen3
    move AirLib\deps\del_eigen\Eigen AirLib\deps\eigen3\Eigen
    rmdir /S /Q AirLib\deps\del_eigen
    del eigen3.zip
)
IF NOT EXIST AirLib\deps\eigen3 goto :buildfailed


REM //---------- now we have all dependencies to compile AirSim.sln which will also compile MavLinkCom ----------
if "%buildMode%" == "" (
msbuild /m /p:CL_MPCount=12 -maxcpucount:12 /p:Platform=x64 /p:Configuration=Debug AirSim.sln /t:MavLinkCom
msbuild /m /p:CL_MPCount=12 -maxcpucount:12 /p:Platform=x64 /p:Configuration=Debug AirSim.sln /t:AirLib
if ERRORLEVEL 1 goto :buildfailed
msbuild /m /p:CL_MPCount=12 -maxcpucount:12 /p:Platform=x64 /p:Configuration=Release AirSim.sln /t:MavLinkCom
msbuild /m /p:CL_MPCount=12 -maxcpucount:12 /p:Platform=x64 /p:Configuration=Release AirSim.sln /t:AirLib
if ERRORLEVEL 1 goto :buildfailed
) else (
msbuild /m /p:CL_MPCount=12 -maxcpucount:12 /p:Platform=x64 /p:Configuration=%buildMode% AirSim.sln /t:MavLinkCom
msbuild /m /p:CL_MPCount=12 -maxcpucount:12 /p:Platform=x64 /p:Configuration=%buildMode% AirSim.sln /t:AirLib
if ERRORLEVEL 1 goto :buildfailed
)

REM //---------- copy binaries and include for MavLinkCom in deps ----------
set MAVLINK_TARGET_LIB=AirLib\deps\MavLinkCom\lib
if NOT exist %MAVLINK_TARGET_LIB% mkdir %MAVLINK_TARGET_LIB%
set MAVLINK_TARGET_INCLUDE=AirLib\deps\MavLinkCom\include
if NOT exist %MAVLINK_TARGET_INCLUDE% mkdir %MAVLINK_TARGET_INCLUDE%
robocopy /MIR MavLinkCom\include %MAVLINK_TARGET_INCLUDE%
robocopy /MIR MavLinkCom\lib %MAVLINK_TARGET_LIB%

REM Ensure destination folder exists
if not exist "%DEST_DIR%" (
    mkdir "%DEST_DIR%"
)

REM Mirror temp build folder to destination, excluding temp files and directories
robocopy /MIR "%TEMP_BUILD_AIRLIB_DIR%" "%DEST_DIR%" /XD temp *. /njh /njs /ndl /np

REM Copy the AirSim.props file to the destination folder
copy /y "%PROP_FILE%" "%DEST_DIR%"

echo Build and copy completed successfully.
exit /b 0

:buildfailed
echo(
echo #### Build failed - see messages above. 1>&2

:buildfailed_nomsg
exit /b 1