@echo off
setlocal

rem 切换到脚本所在目录
cd /d "%~dp0"

rem 防止在错误目录运行
if not exist "CMakeLists.txt" (
    echo [ERROR] Current directory does not contain CMakeLists.txt.
    echo Please put this script in the Qt project root directory.
    pause
    exit /b 1
)

echo Creating project directories...

for %%D in (
    "src\models"
    "src\interfaces"
    "src\repositories"
    "src\services"
    "src\application"
    "src\ui"
    "data"
    "database"
    "tests"
    "docs"
) do (
    if not exist "%%~D" (
        mkdir "%%~D"
        echo Created: %%~D
    ) else (
        echo Exists:  %%~D
    )
)

rem Git不记录空文件夹，因此添加占位文件
for %%D in (
    "src\models"
    "src\interfaces"
    "src\repositories"
    "src\services"
    "src\application"
    "src\ui"
    "data"
    "database"
    "tests"
    "docs"
) do (
    if not exist "%%~D\.gitkeep" (
        type nul > "%%~D\.gitkeep"
    )
)

echo.
echo Project structure created successfully.
echo.
tree /f

pause