@echo off 
::关闭命令回显，不打印执行的指令，再gbk下正常
::终端使用gbk，所以文件保存为ansi，中文不会乱码
chcp 936 >nul
::开启 延迟变量扩展 ，启用 !变量! 开启延迟扩展，是脚本能够正常处理带空格文件名的核心前提。
setlocal enabledelayedexpansion

:: %var% 立即扩展（解析阶段求值）
:: !var! 延迟扩展（运行时求值）
:: %var% 是在解析时展开的，如果文件名有空格会导致路径被截断；而 !var! 在运行时展开，能完整保留带空格的路径

::定义目标根目录，转换完成的视频存放位置
set "TARGET_ROOT=data\video"

::%~1 = 传入的第一个参数 %~nx0：自动获取当前脚本文件名。
if "%~1"=="" (
    echo 用法: %~nx0 源目录
    echo 例如: %~nx0 data\video_source\TwDown
    pause
    exit /b 1
)

::接收传入的源目录，判断目录是否存在，不存在直接退出。
set "SOURCE_DIR=%~1"
if not exist "%SOURCE_DIR%" (
    echo 错误：源目录 "%SOURCE_DIR%" 不存在！
    pause
    exit /b 1
)

:: 计算目标子目录（保留相对路径）
:: %变量:旧字符串=新字符串% 格式：%VAR:A=B% 把变量 VAR 里面所有 A 替换成 B 
:: %SOURCE_DIR:data\video_source\=% 把路径前缀 data\video_source\ 删除，得到后面的相对子目录
:: 把 data\video_source\ 从路径剔除，得到子目录名称，再和TARGET_ROOT拼接得到目标目录
:: 源目录 data\video_source\telegram → 目标 data\video\telegram 不存在目录则创建 mkdir。
set "REL_PATH=%SOURCE_DIR:data\video_source\=%"
if "%REL_PATH%"=="%SOURCE_DIR%" set "REL_PATH="
set "TARGET_DIR=%TARGET_ROOT%\%REL_PATH%"
if not exist "%TARGET_DIR%" mkdir "%TARGET_DIR%"

echo ========================================
echo   视频兼容转换工具（H.264 + AAC + FastStart）
echo   源目录: %SOURCE_DIR%
echo   目标目录: %TARGET_DIR%
echo   若已是 H.264 则优先复制流，失败则转码
echo   非 H.264 优先硬件加速，失败则 CPU 转码
echo   转换后源文件将重命名为 *_source.*
echo ========================================
echo.

set EXTS=.mp4 .mov .mkv .avi .flv .webm

:: 第一层遍历后缀  第二层遍历 某目录中 对应后缀所有文件。
:: call :process_file "%%f" 调用子函数，把完整文件路径传入。
for %%x in (%EXTS%) do (
    for %%f in ("%SOURCE_DIR%\*%%x") do (
        if exist "%%f" call :process_file "%%f"
    )
)

echo.
echo 全部处理完成！
pause
exit /b

:: %~1：接收传入的带引号路径，自动去除引号。
:: %~n1：获取文件名（不含后缀）
:: %~x1：获取文件后缀
:: 仅在这里使用 % 赋值，后续全部使用！变量！，这是修复空格文件名报错关键。
:process_file
set "file=%~1"
set "name=%~n1"
set "ext=%~x1"

:: 跳过已备份文件
:: 管道 | 将 echo 输出送给 findstr /i 忽略大小写；查找文件名是否包含 _source 找到就跳过，
:: goto :eof 退出当前子函数。  >nul 屏蔽 findstr 输出。
echo !name! | findstr /i "_src" >nul
if not errorlevel 1 (
    echo [跳过] !file! 已包含 _src
    goto :eof
)

:: 拼接输出文件路径；使用 ! 延迟扩展，兼容空格文件名。
:: 目标文件存在直接跳过，避免重复转换。
set "outfile=!TARGET_DIR!\!name!!ext!"
if exist "!outfile!" (
    echo [跳过] 目标文件已存在: !outfile!
    goto :eof
)

:: 获取视频编码
:: for /f 捕获外部命令输出。
:: ffprobe 参数解释：
:: -v error 只输出错误
:: -select_streams v:0 选择第一条视频流
:: show_entries stream=codec_name 只输出编码名称
:: default=noprint_wrappers=1:nokey=1 精简输出，只保留编码名
:: 2>nul 屏蔽 ffprobe 错误输出。
:: 执行后 codec 变量保存 h264 / hevc 这类编码名称。
for /f "delims=" %%i in ('ffprobe -v error -select_streams v:0 -show_entries stream^=codec_name -of default^=noprint_wrappers^=1:nokey^=1 "!file!" 2^>nul') do set "codec=%%i"
if "!codec!"=="" (
    echo [错误] 无法读取 !file! 的编码信息，跳过
    goto :eof
)

:: 判断是否为 h264 直接判断输出 codec 中有没有h264，不使用^匹配开头也不使用$匹配末尾，因为 ffprobe 输出带输出自带\r\n
echo !file! 编码为 !codec! 
echo !codec! | findstr /i "h264" >nul
:: 分支 1：视频已经是 H.264   2>nul 屏蔽 ffmpeg 日志。 流复制失败时，回退 CPU 重新编码：
if not errorlevel 1 (
    :: 已是 H.264：优先复制流 + 移动 moov
    echo [处理] !file! 已是 H.264，尝试复制流并移动 moov ...
    ffmpeg -i "!file!" -c copy -movflags +faststart "!outfile!" 2>nul
    if errorlevel 1 (
        echo [失败] 复制流失败，尝试 CPU 转码 ...
        ffmpeg -i "!file!" -c:v libx264 -preset slow -crf 23 -c:a aac -movflags +faststart "!outfile!" 2>nul
    )
) else (
    :: 非 H.264：尝试硬件加速转码 优先 NVENC 硬件编码 硬件失败自动回退 libx264 CPU 编码。
    echo [处理] !file! 编码为 !codec!，尝试硬件加速转码 ...
    ffmpeg -i "!file!" -c:v h264_nvenc -cq 23 -preset p6 -rc vbr -b:v 0 -c:a aac -movflags +faststart "!outfile!" 2>nul
    if errorlevel 1 (
        echo [失败] 硬件转码失败，回退到 CPU 软件转码 ...
        ffmpeg -i "!file!" -c:v libx264 -preset slow -crf 23 -c:a aac -movflags +faststart "!outfile!" 2>nul
    )
)

:: 最终检查输出文件是否有效
if not exist "!outfile!" (
    echo [失败] 未生成输出文件: !outfile!
    goto :eof
)
:: 校验：是否生成文件、文件大小不能为 0；空文件直接删除。
for %%a in ("!outfile!") do if %%~za equ 0 (
    echo [失败] 输出文件大小为0，转换失败
    del "!outfile!" 2>nul
    goto :eof
)

:: 转换成功，重命名源文件为 *_source.* 源文件重命名 xxx.mp4 → xxx_source.mp4，作为备份。
set "source_backup=!SOURCE_DIR!\!name!_src!ext!"
move "!file!" "!source_backup!" >nul
if errorlevel 1 (
    echo [警告] 转换成功但无法重命名源文件，请手动处理: !file!
) else (
    echo [成功] 已生成 !outfile! ，源文件已备份为 !source_backup!
)
echo.
goto :eof