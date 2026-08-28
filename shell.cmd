@if "%DOCKER_IMAGE%" == "" set DOCKER_IMAGE=node:22.12.0

@for /D %%I in (%~dp0) do set PROJECT_ROOT=%%~dfI
@set REMOTE_PROJECT_ROOT=/%PROJECT_ROOT::=%
set REMOTE_PROJECT_ROOT=%REMOTE_PROJECT_ROOT:\=/%

docker run -it -w /project --rm -v %REMOTE_PROJECT_ROOT%:/project %DOCKER_IMAGE% bash
