@echo off
chcp 936 >nul
setlocal enabledelayedexpansion

if "%~1"=="" (
    echo 用法: %~nx0 目标目录
    echo 示例: %~nx0 data\video_source\telegram
    pause
    exit /b 1
)

set "WORK_DIR=%~1"
if not exist "%WORK_DIR%" (
    echo [错误] 目录不存在: "%WORK_DIR%"
    pause
    exit /b 1
)

echo ========================================
echo  还原备份文件：*_source.ext  → *.ext
echo  工作目录: %WORK_DIR%
echo ========================================
echo.

for /r "%WORK_DIR%" %%f in (*_src.*) do (
    set "full=%%f"
    set "name=%%~nf"
    set "ext=%%~xf"

    :: 移除 _source
    set "new_name=!name:_src=!!ext!"
    set "new_path=%%~dpf!new_name!"

    if exist "!new_path!" (
        echo [跳过] 目标文件已存在: !new_path!
    ) else (
        echo [重命名] "!full!" ^> "!new_path!"
        ren "!full!" "!new_name!"
    )
)

echo.
echo 操作完成！
pause
exit /b