@echo off
set IDF_TOOLS_PATH=D:\Espressif_tools
set IDF_PATH=D:\Espressif
set IDF_PYTHON_ENV_PATH=D:\Espressif_tools\python_env\idf5.5_py3.11_env
set PATH=%IDF_PATH%\tools;%IDF_PYTHON_ENV_PATH%\Scripts;%IDF_TOOLS_PATH%\tools\idf-git\2.44.0\cmd;%IDF_TOOLS_PATH%\tools\cmake\3.24.0\bin;%IDF_TOOLS_PATH%\tools\ninja\1.12.1;%PATH%
call "%IDF_PATH%\export.bat" >nul 2>&1
cd /d D:\git\UWB650_MC
echo === Setting target ESP32-S3 ===
"%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" "%IDF_PATH%\tools\idf.py" set-target esp32s3
echo === Starting build ===
"%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" "%IDF_PATH%\tools\idf.py" build
echo === Build exit code: %ERRORLEVEL% ===
pause
