@for /D %%I in (%~dp0) do set PROJECT_ROOT=%%~dfI
@set REMOTE_PROJECT_ROOT=/%PROJECT_ROOT::=%
set REMOTE_PROJECT_ROOT=%REMOTE_PROJECT_ROOT:\=/%

@REM Win32 build
call npx prebuildify --napi

@REM Linux64 build
call docker run -it -w /project --rm -v %REMOTE_PROJECT_ROOT%:/project node:22.12.0 npx prebuildify --napi

@REM Darwin64 build
@REM TODO
