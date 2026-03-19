Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
Remove-Item Env:MSYS -ErrorAction SilentlyContinue
$env:IDF_TOOLS_PATH = 'D:\Espressif_tools'
$env:IDF_PATH = 'D:\Espressif'
$env:IDF_PYTHON_ENV_PATH = 'D:\Espressif_tools\python_env\idf5.5_py3.11_env'
$env:ESP_ROM_ELF_DIR = 'D:\Espressif_tools\tools\esp-rom-elfs\20241011\'
$env:PATH = 'D:\Espressif_tools\python_env\idf5.5_py3.11_env\Scripts;D:\Espressif_tools\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin;D:\Espressif_tools\tools\cmake\3.30.2\bin;D:\Espressif_tools\tools\ninja\1.12.1;D:\Espressif_tools\tools\idf-git\2.44.0\cmd;D:\Espressif_tools\tools\idf-exe\1.0.3;C:\Windows\system32;C:\Windows'
Set-Location 'D:\git\UWB650_MC'
& 'D:\Espressif_tools\python_env\idf5.5_py3.11_env\Scripts\python.exe' 'D:\Espressif\tools\idf.py' build 2>&1
