@ECHO OFF
SETLOCAL

WHERE py >NUL 2>NUL
IF ERRORLEVEL 1 GOTO PythonFallback

py -3 "%~dp0Setup.py" --generate-only %*
EXIT /B %ERRORLEVEL%

:PythonFallback
WHERE python >NUL 2>NUL
IF ERRORLEVEL 1 GOTO PythonMissing

python "%~dp0Setup.py" --generate-only %*
EXIT /B %ERRORLEVEL%

:PythonMissing
ECHO Python 3 was not found. Install Python 3 and rerun Scripts\GenerateBuildFiles.bat.
EXIT /B 1
